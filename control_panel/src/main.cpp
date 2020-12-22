//
// Created by m.tsvetkov on 05.07.2020.
//

//#include "boost/asio.hpp"
#include "asio.hpp"
#include "boost/endian/conversion.hpp"
//#include "portable_concurrency/thread_pool"

#include <cxxopts.hpp>

#include "client/client.hpp"
#include "client_finder/client_finder.hpp"
#include "menu/menu.hpp"
#include "protocol/protocol.hpp"

#include <iostream>
#include <memory>
#include <thread>

namespace protocol = tsvetkov::protocol;
// namespace asio     = boost::asio;

// TODO:
// 1. Cохранение параметров в фс на устройстве
// 1. Оформить как библиотеку

int main(int argc, char** argv)
{
    protocol::register_big_endian_to_native_32(&boost::endian::big_to_native);
    protocol::register_native_to_big_endian_32(&boost::endian::native_to_big);
    protocol::register_big_endian_to_native_16(&boost::endian::big_to_native);
    protocol::register_native_to_big_endian_16(&boost::endian::native_to_big);

    std::string remote_address;
    std::uint16_t port;
    try {
        cxxopts::Options options("control_panel", "Control remote unit");
        options.add_options()("ip", "remote address", cxxopts::value<std::string>())(
            "port", "remote port", cxxopts::value<std::uint16_t>()->default_value("2000"));

        auto result = options.parse(argc, argv);

        if (result.count("help") || result.count("ip") == 0) {
            std::cout << options.help() << std::endl;
            return 0;
        }

        remote_address = result["ip"].as<std::string>();
        port           = result["port"].as<std::uint16_t>();

        std::cout << "Client ip:" << remote_address << std::endl;
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }

    asio::io_context io;
    asio::executor_work_guard<asio::io_context::executor_type> work_guard(io.get_executor());
    auto asio_worker = std::thread([&] { io.run(); });

    tsvetkov::Menu menu;

    auto get_number = [] {
        std::string i;
        std::getline(std::cin, i);
        return static_cast<std::uint32_t>(std::stoi(i));
    };

    auto found_device = [&] {
        auto client_finder = std::make_shared<tsvetkov::ClientFinder>(io);

        auto found_device_promise = std::make_shared<pc::promise<tsvetkov::FoundDevice>>();
        auto found_device_future  = found_device_promise->get_future();

        client_finder->subscribe_to_found_new_device_event(tsvetkov::action_if_exists(
            tsvetkov::make_single_context(client_finder),
            [found_device_promise = std::move(found_device_promise)](tsvetkov::ClientFinder* self,
                                                                     tsvetkov::FoundDevice found_device) mutable {
                if (found_device_promise) {
                    std::cout << "Found device!!! ip: " << found_device.ip_address << std::endl;
                    found_device_promise->set_value(std::move(found_device));
                    found_device_promise.reset();
                    self->stop();
                }
            }));

        client_finder->start();
        return found_device_future.get();
    }();

    auto client = std::make_shared<tsvetkov::Client>(io, found_device.ip_address, port);

    menu.add_item("All On", [&client] { client->send_all_on(); });
    menu.add_item("All Off", [&client] { client->send_all_off(); });

    auto smart_power_status_future = client->async_connect();

    try {
        auto smart_power_status = smart_power_status_future.get();
        for (const auto& pair : smart_power_status.status) {
            auto pin = pair.first;
            menu.add_item("Inversion " + std::to_string(pin), [&client, pin] { client->inversion(pin); });
        }
    } catch (const std::exception& e) {
        std::cout << "Connection error: " << e.what() << std::endl;
    }

    bool is_continue = true;

    menu.add_item("Exit", [&] { is_continue = false; });
    menu.add_item("Test", [&] { client.reset(); });

    while (is_continue) {
        std::cout << menu.str();
        std::size_t i = get_number();
        std::cout << "selected: " << i << std::endl;
        menu.item(i);
    }

    work_guard.reset();
    io.stop();
    asio_worker.join();

    return 0;
}