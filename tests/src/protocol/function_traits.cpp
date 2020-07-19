//
// Created by m.tsvetkov on 12.07.2020.
//

#include <catch2/catch.hpp>

#include "common/function_traits.hpp"

namespace {
void void_function(double, std::size_t) {}

std::size_t size_t_function(double, std::size_t)
{
    return std::size_t{0};
}

struct Foo
{
    void void_method(const std::string&, std::size_t) {}
    std::size_t size_t_method(const std::string&, std::size_t)
    {
        return std::size_t{0};
    }
    void void_const_method(const std::string&, std::size_t) const {}
};
}
TEST_CASE("Function traits")
{
    namespace traits = tsvetkov::traits;

    using void_function_traits = traits::function_traits<decltype(void_function)>;
    static_assert(std::is_same<void_function_traits::return_type, void>::value, "");
    static_assert(std::is_same<void_function_traits::argument<0>::type, double>::value, "");
    static_assert(std::is_same<void_function_traits::argument<1>::type, std::size_t>::value, "");

    using size_t_function_traits = traits::function_traits<decltype(size_t_function)>;
    static_assert(std::is_same<size_t_function_traits::return_type, std::size_t>::value, "");
    static_assert(std::is_same<size_t_function_traits::argument<0>::type, double>::value, "");
    static_assert(std::is_same<size_t_function_traits::argument<1>::type, std::size_t>::value, "");

    using void_method_traits = traits::function_traits<decltype(&Foo::void_method)>;
    static_assert(std::is_same<void_method_traits::return_type, void>::value, "");
    static_assert(std::is_same<void_method_traits::argument<0>::type, Foo&>::value, "");
    static_assert(std::is_same<void_method_traits::argument<1>::type, const std::string&>::value, "");
    static_assert(std::is_same<void_method_traits::argument<2>::type, std::size_t>::value, "");

    using size_t_method_traits = traits::function_traits<decltype(&Foo::size_t_method)>;
    static_assert(std::is_same<size_t_method_traits::return_type, std::size_t>::value, "");
    static_assert(std::is_same<size_t_method_traits::argument<0>::type, Foo&>::value, "");
    static_assert(std::is_same<size_t_method_traits::argument<1>::type, const std::string&>::value, "");
    static_assert(std::is_same<size_t_method_traits::argument<2>::type, std::size_t>::value, "");

    using void_const_method_traits = traits::function_traits<decltype(&Foo::void_const_method)>;
    static_assert(std::is_same<void_const_method_traits::return_type, void>::value, "");
    static_assert(std::is_same<void_const_method_traits::argument<0>::type, Foo&>::value, "");
    static_assert(std::is_same<void_const_method_traits::argument<1>::type, const std::string&>::value, "");
    static_assert(std::is_same<void_const_method_traits::argument<2>::type, std::size_t>::value, "");

    auto lambda = [](const std::string&) { return std::size_t{0}; };

    using lambda_traits = traits::function_traits<decltype(lambda)>;
    static_assert(std::is_same<lambda_traits::arguments, std::tuple<const std::string&>>::value, "");
    static_assert(std::is_same<lambda_traits::return_type, std::size_t>::value, "");
    static_assert(std::is_same<lambda_traits::argument<0>::type, const std::string&>::value, "");

    auto void_lambda         = [] {};
    using void_lambda_traits = traits::function_traits<decltype(void_lambda)>;
    static_assert(std::is_same<void_lambda_traits::arguments, std::tuple<>>::value, "");
    static_assert(std::is_same<void_lambda_traits::return_type, void>::value, "");

    std::function<void(std::size_t)> std_function = [](std::size_t) {};

    using function_traits = tsvetkov::traits::function_traits<decltype(std_function)>;
    static_assert(std::is_same<function_traits::return_type, void>::value, "");
    static_assert(std::is_same<function_traits::argument<0>::type, std::size_t>::value, "");
}