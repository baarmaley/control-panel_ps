//
// Created by m.tsvetkov on 05.07.2020.
//

#include "boost/asio.hpp"
#include "boost/endian/conversion.hpp"

#include <cxxopts.hpp>

#include <protocol/protocol.hpp>

#include "task.hpp"

#include <functional>
#include <iostream>
#include <memory>

namespace asio = boost::asio;
namespace protocol = tsvetkov::protocol;

template<typename F, typename... Args>
std::shared_ptr<typename tsvetkov::traits::function_traits<F>::return_type> make_shared_buffer(F&& make_buffer,
                                                                                               Args... args)
{
    return std::make_shared<typename tsvetkov::traits::function_traits<F>::return_type>(
        make_buffer(std::forward<Args>(args)...));
}

struct Client
{
    Client(asio::io_context& io, std::string remote_address, std::uint16_t port)
        : socket(io), endpoint(asio::ip::make_address(remote_address), port)
    {
    }

    Client(const Client&) = delete;
    Client(Client&&)      = delete;

    Client& operator=(const Client&) = delete;
    Client& operator=(Client&&) = delete;

    void connect()
    {
        asio::async_connect(socket,
                            std::vector<asio::ip::tcp::endpoint>{endpoint},
                            tsvetkov::make_asio_task(
                                [this](const asio::ip::tcp::endpoint&) {
                                    std::cout << "async_connect ok!" << std::endl;
                                    send_request();
                                },
                                [](const boost::system::error_code& ec) {
                                    std::cout << "async_connect error: " << ec << ": " << ec.message() << std::endl;
                                }));
    }

    void send_request()
    {
        auto hello_request = make_shared_buffer(&tsvetkov::protocol::make_hello_request, next_id());
        asio::async_write(socket,
                          asio::buffer(*hello_request),
                          tsvetkov::make_asio_task(
                              [hello_request](std::size_t bytes_transferred) {
                                  std::cout << "async_write, bytes_transferred: " << bytes_transferred
                                            << " buffer: " << hello_request->size() << std::endl;
                              },
                              [](const boost::system::error_code& ec) {
                                  std::cout << "async_write error: " << ec << ": " << ec.message() << std::endl;
                              }));
    }

    std::uint32_t next_id()
    {
        return counter_id++;
    }

    asio::ip::tcp::socket socket;
    asio::ip::tcp::endpoint endpoint;

    std::uint32_t counter_id = 0;
};


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
        Client client(io, remote_address, port);
        client.connect();
        io.run();
    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
    }

    return 0;
}