//
// Created by mtsvetkov on 19.07.2020.
//

#include "client.hpp"

#include "common/task.hpp"

#include "protocol/protocol.hpp"

#include <iostream>

namespace tsvetkov {

namespace {
template<typename F, typename... Args>
std::shared_ptr<typename tsvetkov::traits::function_traits<F>::return_type> make_shared_buffer(F&& make_buffer,
                                                                                               Args... args)
{
    return std::make_shared<typename tsvetkov::traits::function_traits<F>::return_type>(
        make_buffer(std::forward<Args>(args)...));
}
} // namespace

Client::Client(asio::io_context& io, const std::string& remote_address, std::uint16_t port)
    : socket(io), endpoint(asio::ip::make_address(remote_address), port)
{
}

void Client::connect()
{
    asio::async_connect(socket,
                        std::vector<asio::ip::tcp::endpoint>{endpoint},
                        tsvetkov::make_asio_task(
                            [this](const asio::ip::tcp::endpoint&) {
                                std::cout << "async_connect ok!" << std::endl;
                                send_hello_request();
                            },
                            [](const boost::system::error_code& ec) {
                                std::cout << "async_connect error: " << ec << ": " << ec.message() << std::endl;
                            }));
}

void Client::send_hello_request()
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

std::uint32_t Client::next_id()
{
    return counter_id++;
}

} // namespace tsvetkov