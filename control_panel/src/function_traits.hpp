//
// Created by m.tsvetkov on 12.07.2020.
//

#pragma once

#include <functional>

namespace tsvetkov {
namespace traits {
template<class F>
struct function_traits;

// function pointer
template<class R, class... Args>
struct function_traits<R (*)(Args...)> : public function_traits<R(Args...)>
{
};

template<class R, class... Args>
struct function_traits<R(Args...)>
{
    using return_type = R;
    using arguments = std::tuple<Args...>;

    static constexpr std::size_t arity = sizeof...(Args);

    template<std::size_t N>
    struct argument
    {
        static_assert(N < arity, "error: invalid parameter index.");
        using type = typename std::tuple_element<N, std::tuple<Args...>>::type;
    };
};

// member function pointer
template<class C, class R, class... Args>
struct function_traits<R(C::*)(Args...)> : public function_traits<R(C&,Args...)>
{};

// const member function pointer
template<class C, class R, class... Args>
struct function_traits<R(C::*)(Args...) const> : public function_traits<R(C&,Args...)>
{};

// member object pointer
template<class C, class R>
struct function_traits<R(C::*)> : public function_traits<R(C&)>
{};

template<typename Tuple>
struct argumnets_without_this;

template<typename Self, typename... Args>
struct argumnets_without_this<std::tuple<Self, Args...>>
{
    using arguments = std::tuple<Args...>;
};

// functor
template<class F>
struct function_traits
{
private:
    using call_type = function_traits<decltype(&F::operator())>;
public:
    using return_type = typename call_type::return_type;
    using arguments =  typename argumnets_without_this<typename call_type::arguments>::arguments;

    static constexpr std::size_t arity = call_type::arity - 1;

    template <std::size_t N>
    struct argument
    {
        static_assert(N < arity, "error: invalid parameter index.");
        using type = typename call_type::template argument<N+1>::type;
    };
};

}
}
