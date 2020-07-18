//
// Created by m.tsvetkov on 13.07.2020.
//

#pragma once

//#include <system_error>
#include <boost/system/error_code.hpp>

#include "function_traits.hpp"

namespace tsvetkov {
namespace details {
template<typename CompleteF, typename ErrorF, typename Tuple>
struct HelperCallTask
{
};

template<typename CompleteF, typename ErrorF, typename... Args>
struct HelperCallTask<CompleteF, ErrorF, std::tuple<Args...>>
{
    HelperCallTask(CompleteF&& _complete, ErrorF&& _error) : _complete(std::move(_complete)), _error(std::move(_error))
    {
    }

    void operator()(const boost::system::error_code& ec, Args... args)
    {
        if (ec) {
            _error(ec);
            return;
        }
        _complete(std::forward<Args>(args)...);
    }

private:
    CompleteF _complete;
    ErrorF _error;
};
} // namespace details

template<typename CompleteF, typename ErrorF>
struct AsioTask
    : details::HelperCallTask<CompleteF, ErrorF, typename tsvetkov::traits::function_traits<CompleteF>::arguments>
{
    AsioTask(CompleteF&& _complete, ErrorF&& _error)
        : details::HelperCallTask<CompleteF, ErrorF, typename tsvetkov::traits::function_traits<CompleteF>::arguments>(
              std::move(_complete), std::move(_error))
    {
    }

    AsioTask(AsioTask&&) noexcept = default;
};

template<typename CompleteF, typename ErrorF>
AsioTask<CompleteF, ErrorF> make_asio_task(CompleteF&& _complete, ErrorF&& _error)
{
    return AsioTask<CompleteF, ErrorF>(std::move(_complete), std::move(_error));
}
} // namespace tsvetkov