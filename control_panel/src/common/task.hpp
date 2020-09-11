//
// Created by m.tsvetkov on 13.07.2020.
//

#pragma once

#include <condition_variable>
#include <mutex>

#include <boost/optional.hpp>
#include <boost/system/error_code.hpp>
#include <boost/variant.hpp>
#include <type_traits>

#include "common/function_traits.hpp"

namespace tsvetkov {
namespace details {
template<typename CompleteF, typename ErrorF, typename ErrorType, typename Tuple>
struct HelperCallTask
{
};
// variant<ReturnType, ErrorType> future(task) result
// variant<ReturnType, ErrorType> -> optional<ReturnType> erasure error
// for void
// variant<void, ErrorType> -> optional<ErrorType>: Success: boost::none, Error: ErrorType
// erasure error               bool                 Success: true       , Error: false

template<typename SuccessType,
         typename ErrorType,
         typename SuccessF,
         typename ErrorF,
         typename SuccessArgs,
         typename ErrorArgs>
struct ResultTask
{
};

template<typename ErrorType, typename SuccessF, typename ErrorF, typename... SuccessArgs, typename... ErrorArgs>
struct ResultTask<void, ErrorType, SuccessF, ErrorF, std::tuple<SuccessArgs...>, std::tuple<ErrorArgs...>>
{
    using result_type = boost::optional<ErrorType>;

    ResultTask(SuccessF&& success_f, ErrorF&& error_f) : success_f(std::move(success_f)), error_f(std::move(error_f)) {}

    void success(SuccessArgs... args)
    {
        success_f(std::forward<SuccessArgs>(args)...);
    }

    void error(ErrorArgs... args)
    {
        result_task = error_f(std::forward<ErrorArgs>(args)...);
    }

    SuccessF success_f;
    ErrorF error_f;

    result_type result_task;
};

template<typename SuccessType, typename SuccessF, typename ErrorF, typename... SuccessArgs, typename... ErrorArgs>
struct ResultTask<SuccessType, void, SuccessF, ErrorF, std::tuple<SuccessArgs...>, std::tuple<ErrorArgs...>>
{
    using result_type = boost::optional<SuccessType>;

    ResultTask(SuccessF&& success_f, ErrorF&& error_f) : success_f(std::move(success_f)), error_f(std::move(error_f)) {}

    void success(SuccessArgs... args)
    {
        result_task = success_f(std::forward<SuccessArgs>(args)...);
    }

    void error(ErrorArgs... args)
    {
        error_f(std::forward<ErrorArgs>(args)...);
    }

    SuccessF success_f;
    ErrorF error_f;

    result_type result_task;
};

template<typename SuccessF, typename ErrorF, typename... SuccessArgs, typename... ErrorArgs>
struct ResultTask<void, void, SuccessF, ErrorF, std::tuple<SuccessArgs...>, std::tuple<ErrorArgs...>>
{
    using result_type = void;

    ResultTask(SuccessF&& success_f, ErrorF&& error_f) : success_f(std::move(success_f)), error_f(std::move(error_f)) {}

    void success(SuccessArgs... args)
    {
        success_f(std::forward<SuccessArgs>(args)...);
    }

    void error(ErrorArgs... args)
    {
        error_f(std::forward<ErrorArgs>(args)...);
    }

    SuccessF success_f;
    ErrorF error_f;
};

template<typename SuccessType,
         typename ErrorType,
         typename SuccessF,
         typename ErrorF,
         typename... SuccessArgs,
         typename... ErrorArgs>
struct ResultTask<SuccessType, ErrorType, SuccessF, ErrorF, std::tuple<SuccessArgs...>, std::tuple<ErrorArgs...>>
{
    using result_type = boost::variant<SuccessType, ErrorType>;

    ResultTask(SuccessF&& success_f, ErrorF&& error_f) : success_f(std::move(success_f)), error_f(std::move(error_f)) {}

    void success(SuccessArgs... args)
    {
        result_task = success_f(std::forward<SuccessArgs>(args)...);
    }

    void error(ErrorArgs... args)
    {
        result_task = error_f(std::forward<ErrorArgs>(args)...);
    }

    SuccessF success_f;
    ErrorF error_f;

    result_type result_task;
};

template<typename F, typename MapF, typename ReturnType, typename ArgsF>
struct Transform
{
};

template<typename F, typename MapF, typename ReturnType, typename... ArgsF>
struct Transform<F, MapF, ReturnType, std::tuple<ArgsF...>>
{
    using map_return_type = typename tsvetkov::traits::function_traits<MapF>::return_type;

    static auto map(F&& f, MapF&& map_f)
    {
        return [f = std::forward<F>(f), map_f = std::forward<MapF>(map_f)](ArgsF... args) -> map_return_type {
            return map_f(f(std::forward<ArgsF>(args)...));
        };
    }
};

template<typename F, typename MapF, typename... ArgsF>
struct Transform<F, MapF, void, std::tuple<ArgsF...>>
{
    using map_return_type = typename tsvetkov::traits::function_traits<MapF>::return_type;

    static auto map(F&& f, MapF&& map_f)
    {
        return [f = std::forward<F>(f), map_f = std::forward<MapF>(map_f)](ArgsF... args) -> map_return_type {
            f(std::forward<ArgsF>(args)...);
            return map_f();
        };
    }
};

template<typename SuccessF, typename ErrorF, typename SuccessArgs, typename ErrorArgs>
struct SharedStateTask
{
};

template<typename SuccessF, typename ErrorF, typename... SuccessArgs, typename... ErrorArgs>
struct SharedStateTask<SuccessF, ErrorF, std::tuple<SuccessArgs...>, std::tuple<ErrorArgs...>>
{
    using success_return_type = typename tsvetkov::traits::function_traits<SuccessF>::return_type;
    using error_return_type   = typename tsvetkov::traits::function_traits<ErrorF>::return_type;
    using success_f_type      = SuccessF;
    using error_f_type        = ErrorF;

    using result_task_type = ResultTask<success_return_type,
                                        error_return_type,
                                        SuccessF,
                                        ErrorF,
                                        std::tuple<SuccessArgs...>,
                                        std::tuple<ErrorArgs...>>;

    SharedStateTask(SuccessF&& success_f, ErrorF&& error_f) : m_result_task(std::move(success_f), std::move(error_f)) {}

    void success(SuccessArgs... args)
    {
        std::lock_guard lock_guard(m_mutex);
        if (impl_is_ready() || impl_is_cancel()) {
            return;
        }
        m_result_task.success(std::forward<SuccessArgs>(args)...);
        m_is_ready = true;
    }

    void error(ErrorArgs... args)
    {
        std::lock_guard lock_guard(m_mutex);
        if (impl_is_ready() || impl_is_cancel()) {
            return;
        }
        m_result_task.error(std::forward<ErrorArgs>(args)...);
        m_is_ready = true;
    }

    bool cancel(ErrorArgs... args)
    {
        std::lock_guard lock_guard(m_mutex);
        if (impl_is_ready() || impl_is_cancel()) {
            return false;
        }
        m_result_task.error(std::forward<ErrorArgs>(args)...);
        m_is_cancel = true;
        return true;
    }

    bool is_ready() const
    {
        std::lock_guard lock_guard(m_mutex);
        return impl_is_ready();
    }

    bool is_cancel() const
    {
        std::lock_guard lock_guard(m_mutex);
        return impl_is_cancel();
    }

    template<typename SuccessMap, typename ErrorMap>
    auto map(SuccessMap&& success_transform, ErrorMap&& error_transform)
    {
        auto success_map = Transform<SuccessF, SuccessMap, success_return_type, std::tuple<SuccessArgs...>>::map(
            std::move(m_result_task.success_f), std::forward<SuccessMap>(success_transform));
        auto error_map = Transform<ErrorF, ErrorMap, error_return_type, std::tuple<ErrorArgs...>>::map(
            std::move(m_result_task.error_f), std::forward<ErrorMap>(error_transform));

        return std::make_shared<SharedStateTask<decltype(success_map),
                                                decltype(error_map),
                                                std::tuple<SuccessArgs...>,
                                                std::tuple<ErrorArgs...>>>(std::move(success_map),
                                                                           std::move(error_map));
    }

private:
    bool impl_is_ready() const
    {
        return m_is_ready;
    }

    bool impl_is_cancel() const
    {
        return m_is_cancel;
    }

    std::mutex m_mutex;
    result_task_type m_result_task;
    bool m_is_ready  = false;
    bool m_is_cancel = false;
};

template<typename SuccessF, typename ErrorF, typename ErrorType, typename... SuccessArgs>
struct HelperCallTask<SuccessF, ErrorF, ErrorType, std::tuple<SuccessArgs...>>
{
    using error_arguments = typename tsvetkov::traits::function_traits<ErrorF>::arguments;

    using shared_state_task_type =
        details::SharedStateTask<SuccessF, ErrorF, std::tuple<SuccessArgs...>, error_arguments>;

    HelperCallTask(SuccessF&& complete, ErrorF&& error)
        : shared_state_task(std::make_shared<shared_state_task_type>(std::move(complete), std::move(error)))
    {
    }

    explicit HelperCallTask(std::shared_ptr<shared_state_task_type>&& shared_state_task)
        : shared_state_task(std::move(shared_state_task))
    {
    }

    template<typename SuccessMap, typename ErrorMap>
    auto map(SuccessMap&& success_then, ErrorMap&& error_then) &&
    {
        auto map_shared_state_task =
            shared_state_task->map(std::forward<SuccessMap>(success_then), std::forward<ErrorMap>(error_then));
        return HelperCallTask<typename decltype(map_shared_state_task)::element_type::success_f_type,
                              typename decltype(map_shared_state_task)::element_type::error_f_type,
                              ErrorType,
                              std::tuple<SuccessArgs...>>(std::move(map_shared_state_task));
    }

    void operator()(const ErrorType& ec, SuccessArgs... args)
    {
        if (ec) {
            shared_state_task->error(ec);
            return;
        }
        shared_state_task->success(std::forward<SuccessArgs>(args)...);
    }

private:
    std::shared_ptr<shared_state_task_type> shared_state_task;
};
} // namespace details

template<typename CompleteF, typename ErrorF>
struct AsioTask : details::HelperCallTask<CompleteF,
                                          ErrorF,
                                          boost::system::error_code,
                                          typename tsvetkov::traits::function_traits<CompleteF>::arguments>
{
    AsioTask(CompleteF&& complete, ErrorF&& error)
        : details::HelperCallTask<CompleteF,
                                  ErrorF,
                                  boost::system::error_code,
                                  typename tsvetkov::traits::function_traits<CompleteF>::arguments>(std::move(complete),
                                                                                                    std::move(error))
    {
    }

    AsioTask(AsioTask&&) noexcept = default;
};

template<typename CompleteF, typename ErrorF>
AsioTask<CompleteF, ErrorF> make_asio_task(CompleteF&& complete, ErrorF&& error)
{
    return AsioTask<CompleteF, ErrorF>(std::forward<CompleteF>(complete), std::forward<ErrorF>(error));
}
} // namespace tsvetkov