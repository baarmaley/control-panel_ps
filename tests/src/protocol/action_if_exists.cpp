//
// Created by mtsvetkov on 19.07.2020.
//

#include <catch2/catch.hpp>

#include <common/action_if_exists.hpp>
#include <common/context.hpp>

#include <tuple>

namespace {
struct Foo : std::enable_shared_from_this<Foo> {
    int call(std::size_t first, std::size_t second) {
        return 1;
    }
    int call_foo(Foo* foo, std::size_t first, std::size_t second) {
        return 1;
    }
};

int f(Foo* self, std::size_t first, std::size_t second) {
    return 1;
}

int f_1(Foo* foo_1, Foo* foo_2, std::size_t first, std::size_t second) {
    return 1;
}

void f_void(Foo* foo_1, Foo* foo_2, std::size_t first, std::size_t second) {}
}

TEST_CASE("action_if_exists"){
    using namespace tsvetkov;


    auto foo = std::make_shared<Foo>();
    auto foo_2 = std::make_shared<Foo>();

    //auto multi_ctx = make_multi_context(foo, foo_2);

    auto multictx_function_wrapper = action_if_exists(make_multi_context(foo, foo_2), &f_1);
    auto multictx_with_one_arg_function_wrapper = action_if_exists(make_multi_context(foo), &f);
    auto weak_ptr_ctx_function_wrapper = action_if_exists(std::weak_ptr<Foo>(foo), &f);
    auto multictx_void_function_wrapper = action_if_exists(make_multi_context(foo, foo_2), &f_void);

    auto weak_ptr_ctx_method = action_if_exists(std::weak_ptr<Foo>(foo), &Foo::call);
    auto multictx_method = action_if_exists(make_multi_context(foo, foo_2), &Foo::call_foo);

    multictx_function_wrapper(std::size_t{1}, std::size_t{2});
    multictx_with_one_arg_function_wrapper(std::size_t{1}, std::size_t{2});
    weak_ptr_ctx_function_wrapper(std::size_t{1}, std::size_t{2});
    multictx_void_function_wrapper(std::size_t{1}, std::size_t{2});
    weak_ptr_ctx_method(std::size_t{1}, std::size_t{2});
    multictx_method(std::size_t{1}, std::size_t{2});

    //
    //    auto function_wrapper = action_if_exists(foo, &f);
    //    function_wrapper(std::size_t{1}, std::size_t{2});
    //
    bool is_mutable_lambda_run = false;
    bool is_lambda_run = false;

    auto l_mutable = [&is_mutable_lambda_run](Foo* self, std::size_t, std::size_t) mutable {
      is_mutable_lambda_run = true;
    };
    auto l = [&is_lambda_run](Foo* self, std::size_t, std::size_t) mutable -> int {
      is_lambda_run = true;
      return 1;
    };
    //
//    auto l_wrapper = action_if_exists(std::weak_ptr<Foo>(foo), l);
//    auto l_mutable_wrapper = action_if_exists(std::weak_ptr<Foo>(foo), l_mutable);
}
