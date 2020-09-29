//
// Created by mtsvetkov on 19.07.2020.
//

#pragma once

#include "common/context.hpp"
#include "common/function_traits.hpp"

#include <boost/optional.hpp>
#include <portable_concurrency/future>

namespace tsvetkov {
struct context_is_destroyed : std::exception
{
};

template<typename F, typename Self, typename... Args>
auto impl_call_action(F&& f, std::shared_ptr<Self>&& context, Args&&... args)
{
    return f(context.get(), std::forward<Args>(args)...);
}

template<typename F, typename... Self, size_t... I, typename... Args>
auto impl_call_action(F&& f, std::tuple<Self...>&& context, std::index_sequence<I...>, Args&&... args)
{
    // std::get<I>(tuple).get()... -> T*...
    return f(std::get<I>(context).get()..., std::forward<Args>(args)...);
}

template<typename F, typename... Self, typename... Args>
auto impl_call_action(F&& f, MultiContextHolder<Self...>&& context, Args&&... args)
{
    // context.get() -> tuple<T...>
    return impl_call_action(
        f, std::move(context.get()), std::make_index_sequence<sizeof...(Self)>{}, std::forward<Args>(args)...);
}

template<typename ReturnType>
struct return_type_helper
{
    using return_type = boost::optional<ReturnType>;
    static boost::optional<ReturnType> make_none()
    {
        return boost::none;
    }
};

template<>
struct return_type_helper<void>
{
    using return_type = void;
    static void make_none() {}
};

template<typename T>
struct return_type_helper<pc::future<T>>
{
    using return_type = pc::future<T>;
    static pc::future<T> make_none() {
        pc::promise<T> promise_result;
        promise_result.set_exception(std::make_exception_ptr(context_is_destroyed{}));
        return promise_result.get_future();
    }
};

template<template<typename...> class Context, typename F, size_t... I, typename... ContextElement>
auto impl_make_action(std::index_sequence<I...>, Context<ContextElement...>&& weak_context, F&& f)
{
    using f_traits             = typename traits::function_traits<std::remove_reference_t<decltype(f)>>;
    using f_arguments          = typename f_traits::arguments;
    using f_return_type_helper = return_type_helper<typename f_traits::return_type>;

    return
        [weak_context = std::move(weak_context), f = std::forward<F>(f)](
            typename std::tuple_element<sizeof...(ContextElement) + I, f_arguments>::type... args) mutable ->
        typename f_return_type_helper::return_type {
            auto strong_context = weak_context.lock();
            if (!strong_context) {
                return f_return_type_helper::make_none();
            }
            return impl_call_action(
                std::forward<F>(f),
                std::move(strong_context),
                std::forward<typename std::tuple_element<sizeof...(ContextElement) + I, f_arguments>::type>(args)...);
        };
}

template<template<typename...> class Context, typename F, typename... ContextElement>
auto action_if_exists(Context<ContextElement...> weak_context, F&& f)
{
    return impl_make_action(
        std::make_index_sequence<traits::function_traits<std::remove_reference_t<decltype(f)>>::arity -
                                 sizeof...(ContextElement)>{},
        std::move(weak_context),
        std::forward<F>(f));
}

template<template<typename...> class Context,
         typename T,
         typename ReturnType,
         typename... ContextElement,
         typename... Args>
auto action_if_exists(Context<T, ContextElement...> weak_context, ReturnType (T::*action)(Args...))
{
    return action_if_exists(std::move(weak_context),
                            [action](T* t, Args... args) { return (*t.*action)(std::forward<Args>(args)...); });
}
// method const
template<template<typename...> class Context,
         typename T,
         typename ReturnType,
         typename... ContextElement,
         typename... Args>
auto action_if_exists(Context<T, ContextElement...> weak_context, ReturnType (T::*action)(Args...) const)
{
    return action_if_exists(std::move(weak_context),
                            [action](T* t, Args... args) { return (*t.*action)(std::forward<Args>(args)...); });
}

} // namespace tsvetkov
