//
// Created by mtsvetkov on 19.07.2020.
//

#include <catch2/catch.hpp>

#include <common/action_if_exists.hpp>
#include <common/context.hpp>

#include <tuple>

namespace {
struct Foo : std::enable_shared_from_this<Foo>
{
    int call(std::size_t first, std::size_t second)
    {
        is_call = true;
        return 1;
    }
    int call_foo(Foo* foo, std::size_t first, std::size_t second)
    {
        is_call_foo = true;
        return 1;
    }

    void call_void(std::size_t, std::size_t)
    {
        is_call_void = true;
    }

    void call_without_arg()
    {
        is_call_without_arg = true;
    }

    void call_const() const
    {
        const_cast<Foo*>(this)->is_call_const = true;
    }

    bool is_call             = false;
    bool is_call_foo         = false;
    bool is_call_void        = false;
    bool is_call_without_arg = false;
    bool is_call_const       = false;
};

int f(Foo* self, std::size_t first, std::size_t second)
{
    return 1;
}

int f_1(Foo* foo_1, Foo* foo_2, std::size_t first, std::size_t second)
{
    return 1;
}

void f_void(Foo* foo_1, Foo* foo_2, std::size_t first, std::size_t second) {}
} // namespace

TEST_CASE("multictx")
{
    using namespace tsvetkov;
    auto foo_1     = std::make_shared<Foo>();
    auto foo_2     = std::make_shared<Foo>();
    auto multi_ctx = make_multi_context(foo_1, foo_2);

    SECTION("1")
    {
        auto ctx_holder = multi_ctx.lock();
        REQUIRE(ctx_holder);
    }
    SECTION("2")
    {
        foo_1.reset();
        auto ctx_holder = multi_ctx.lock();
        REQUIRE_FALSE(ctx_holder);
    }
}
TEST_CASE("pc action_if_exists")
{
    using namespace tsvetkov;
    {
        auto foo_1           = std::make_shared<Foo>();
        auto foo_promise     = pc::promise<std::uint32_t>();
        auto another_promise = pc::promise<std::string>();

        auto foo_future = foo_promise.get_future().next(
            action_if_exists(make_single_context(foo_1),
                             [&another_promise](Foo* self, std::uint32_t) { return another_promise.get_future(); }));

        foo_promise.set_value(123);
        another_promise.set_value("123");
        REQUIRE(foo_future.get() == "123");
    }
    {
        auto foo_1           = std::make_shared<Foo>();
        auto foo_promise     = pc::promise<std::uint32_t>();
        auto another_promise = pc::promise<std::string>();

        auto foo_future = foo_promise.get_future().next(
            action_if_exists(make_single_context(foo_1),
                             [&another_promise](Foo* self, std::uint32_t) { return another_promise.get_future(); }));
        foo_1.reset();
        foo_promise.set_value(123);
        REQUIRE_THROWS_AS(foo_future.get(), context_is_destroyed);
    }
}

TEST_CASE("action_if_exists")
{
    using namespace tsvetkov;

    auto foo   = std::make_shared<Foo>();
    auto foo_2 = std::make_shared<Foo>();

    auto multictx_function_wrapper              = action_if_exists(make_multi_context(foo, foo_2), &f_1);
    auto multictx_with_one_arg_function_wrapper = action_if_exists(make_multi_context(foo), &f);
    auto weak_ptr_ctx_function_wrapper          = action_if_exists(make_single_context(foo), &f);
    auto multictx_void_function_wrapper         = action_if_exists(make_multi_context(foo, foo_2), &f_void);

    auto weak_ptr_ctx_method     = action_if_exists(make_single_context(foo), &Foo::call);
    auto multictx_method         = action_if_exists(make_multi_context(foo, foo_2), &Foo::call_foo);
    auto void_method             = action_if_exists(make_single_context(foo), &Foo::call_void);
    auto without_argument_method = action_if_exists(make_single_context(foo), &Foo::call_without_arg);
    auto const_method            = action_if_exists(make_single_context(foo), &Foo::call_const);

    multictx_function_wrapper(std::size_t{1}, std::size_t{2});
    multictx_with_one_arg_function_wrapper(std::size_t{1}, std::size_t{2});
    weak_ptr_ctx_function_wrapper(std::size_t{1}, std::size_t{2});
    multictx_void_function_wrapper(std::size_t{1}, std::size_t{2});
    weak_ptr_ctx_method(std::size_t{1}, std::size_t{2});
    multictx_method(std::size_t{1}, std::size_t{2});
    void_method(std::size_t{1}, std::size_t{2});
    without_argument_method();
    const_method();

    REQUIRE(foo->is_call);
    REQUIRE(foo->is_call_foo);
    REQUIRE(foo->is_call_void);
    REQUIRE(foo->is_call_without_arg);
    REQUIRE(foo->is_call_const);

    bool is_mutable_lambda_run = false;
    bool is_lambda_run         = false;

    auto l_mutable = [&is_mutable_lambda_run](Foo* self, std::size_t, std::size_t) mutable {
        is_mutable_lambda_run = true;
    };
    auto l = [&is_lambda_run](Foo* self, std::size_t, std::size_t) mutable -> int {
        is_lambda_run = true;
        return 1;
    };

    auto l_wrapper         = action_if_exists(make_single_context(foo), l);
    auto l_mutable_wrapper = action_if_exists(make_single_context(foo), l_mutable);

    l_wrapper(std::size_t{1}, std::size_t{2});
    l_mutable_wrapper(std::size_t{1}, std::size_t{2});

    REQUIRE(is_mutable_lambda_run);
    REQUIRE(is_lambda_run);

    auto result = multictx_method(std::size_t{1}, std::size_t{2});
    REQUIRE(!!result);
    REQUIRE(*result == 1);

    foo->is_call_foo = false;
    foo_2.reset();
    auto result_2 = multictx_method(std::size_t{1}, std::size_t{2});
    REQUIRE_FALSE(!!result_2);
    REQUIRE_FALSE(foo->is_call_foo);
}
