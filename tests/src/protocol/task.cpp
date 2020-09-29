//
// Created by m.tsvetkov on 13.07.2020.
//

#include "common/task.hpp"
#include <boost/system/error_code.hpp>
#include <catch2/catch.hpp>

TEST_CASE("Task test")
{
    auto my_task = tsvetkov::make_asio_task([](std::string, std::size_t) {}, [](const std::error_code&) {});
    std::error_code error_code;
    std::string str;
    std::size_t size{0};
    my_task(error_code, str, size);

    auto my_task_2 = tsvetkov::make_asio_task([](std::string) {}, [](const std::error_code&) {});
    my_task_2(error_code, str);

    auto my_task_3 = tsvetkov::make_asio_task([]() {}, [](const std::error_code&) {});
    my_task_3(error_code);

    auto my_task_4 = tsvetkov::make_asio_task([]() { return std::string(); }, [](const std::error_code&) {});
    my_task_4(error_code);

    bool is_call = false;
    auto my_task_5 =
        tsvetkov::make_asio_task([](std::string) { return std::make_pair(std::string("x"), std::string("y")); },
                                 [](const std::error_code&) {})
            .map([&is_call](std::pair<std::string, std::string>) { is_call = true; }, []() {});

    my_task_5(error_code, str);

    REQUIRE(is_call);
}