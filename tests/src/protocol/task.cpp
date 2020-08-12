//
// Created by m.tsvetkov on 13.07.2020.
//

#include "common/task.hpp"
#include <boost/system/error_code.hpp>
#include <catch2/catch.hpp>

TEST_CASE("Task test")
{
    auto my_task = tsvetkov::make_asio_task([](std::string, std::size_t) {}, [](const boost::system::error_code&) {});
    boost::system::error_code error_code;
    std::string str;
    std::size_t size{0};
    my_task(error_code, str, size);

    auto my_task_2 = tsvetkov::make_asio_task([](std::string) {}, [](const boost::system::error_code&) {});
    my_task_2(error_code, str);

    auto my_task_3 = tsvetkov::make_asio_task([]() {}, [](const boost::system::error_code&) {});
    my_task_3(error_code);

    auto my_task_4 = tsvetkov::make_asio_task([]() { return std::string(); }, [](const boost::system::error_code&) {});
    my_task_4(error_code);
}