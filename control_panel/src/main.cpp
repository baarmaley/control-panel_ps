//
// Created by m.tsvetkov on 05.07.2020.
//

//#include "boost/asio.hpp"
#include "asio.hpp"
#include "boost/endian/conversion.hpp"
//#include "portable_concurrency/thread_pool"

#include <cxxopts.hpp>

#include "client/client.hpp"
#include "menu/menu.hpp"
#include "protocol/protocol.hpp"

#include <iostream>
#include <memory>
#include <thread>

namespace protocol = tsvetkov::protocol;
// namespace asio     = boost::asio;

// TODO:
// 1. Добавить пинг
// 1. Добавить протокол обнаружения устройств
// 1. Cохранение параметров в фс на устройстве
// 1. Добавить переподключение к устройству
// 1. Добавить меню
// 1. Оформить как библиотеку

int main(int argc, char** argv)
{
    protocol::register_big_endian_to_native(&boost::endian::big_to_native);
    protocol::register_native_to_big_endian(&boost::endian::native_to_big);

    try {
        cxxopts::Options options("control_panel", "Control remote unit");
        options.add_options()("ip", "remote address", cxxopts::value<std::string>())(
            "port", "remote port", cxxopts::value<std::uint16_t>()->default_value("2000"));

        auto result = options.parse(argc, argv);

        if (result.count("help") || result.count("ip") == 0) {
            std::cout << options.help() << std::endl;
            return 0;
        }

        auto remote_address = result["ip"].as<std::string>();
        std::uint16_t port  = result["port"].as<std::uint16_t>();

        std::cout << "Client ip:" << remote_address << std::endl;

        asio::io_context io;

        tsvetkov::Menu menu;

        auto get_number = [] {
            std::string i;
            std::getline(std::cin, i);
            return static_cast<std::uint32_t>(std::stoi(i));
        };

        auto client = std::make_shared<tsvetkov::Client>(io, remote_address, port);

        menu.add_item("All On", [&client] { client->send_all_on(); });
        menu.add_item("All Off", [&client] { client->send_all_off(); });

        auto smart_power_status_future = client->async_connect();

        auto asio_worker = std::thread([&] { io.run(); });

        auto smart_power_status = smart_power_status_future.get();

        for (const auto& pair : smart_power_status.status) {
            auto pin = pair.first;
            menu.add_item("Inversion " + std::to_string(pin), [&client, pin] { client->inversion(pin); });
        }
        // smart_power_status_future.get();

        bool is_continue = true;

        while (is_continue) {
            std::cout << menu.str();
            std::size_t i = get_number();
            std::cout << "selected: " << i << std::endl;
            menu.item(i);
        }

        asio_worker.join();

    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }

    return 0;
}