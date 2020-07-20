//
// Created by mtsvetkov on 19.07.2020.
//

#include "client.hpp"

#include "common/action_if_exists.hpp"
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
    commandHandler.subscribe([](std::uint32_t id, protocol::HelloResponse hello_response) {
        std::cout << "HelloResponse" << std::endl;
        std::cout << "hello_response.type_device: " << static_cast<std::uint32_t>(hello_response.type_device)
                  << std::endl;
        std::cout << "hello_response.low_device_id: " << hello_response.low_device_id << std::endl;
        std::cout << "hello_response.high_device_id: " << hello_response.high_device_id << std::endl;
    });
}

void Client::connect()
{
    asio::async_connect(
        socket,
        std::vector<asio::ip::tcp::endpoint>{endpoint},
        tsvetkov::make_asio_task(tsvetkov::action_if_exists(make_single_context(shared_from_this()),
                                                            [](Client* self, const asio::ip::tcp::endpoint&) {
                                                                std::cout << "async_connect ok!" << std::endl;
                                                                self->send_hello_request();
                                                            }),
                                 [](const boost::system::error_code& ec) {
                                     std::cout << "async_connect error: " << ec << ": " << ec.message() << std::endl;
                                 }));
}

void Client::disconnect() {}

void Client::send_hello_request()
{
    async_read();

    auto hello_request = make_shared_buffer(&tsvetkov::protocol::make_hello_request, next_id());
    asio::async_write(socket,
                      asio::buffer(*hello_request),
                      tsvetkov::make_asio_task(
                          tsvetkov::action_if_exists(make_single_context(shared_from_this()),
                                                     [hello_request](Client* self, std::size_t bytes_transferred) {
                                                         std::cout
                                                             << "async_write, bytes_transferred: " << bytes_transferred
                                                             << " buffer: " << hello_request->size() << std::endl;
                                                     }),
                          [](const boost::system::error_code& ec) {
                              std::cout << "async_write error: " << ec << ": " << ec.message() << std::endl;
                          }));
}

void Client::async_read()
{
    socket.async_read_some(
        asio::buffer(incoming_buffer),
        tsvetkov::make_asio_task(
            tsvetkov::action_if_exists(
                make_single_context(shared_from_this()),
                [](Client* client, std::size_t bytes_transferred) {
                    std::cout << "async_read, bytes_transferred: " << bytes_transferred << std::endl;
                    auto push_back_buffer = [&] {
                        client->accumulate_incoming_buffer.append(&client->incoming_buffer[0], bytes_transferred);
                    };
                    // size packet
                    if (client->accumulate_incoming_buffer.size() + bytes_transferred <
                        protocol::Message::packet_size) {
                        push_back_buffer();
                        client->async_read();
                        return;
                    }
                    if (!client->accumulate_incoming_buffer.empty()) {
                        push_back_buffer();
                    }

                    auto buffer = client->accumulate_incoming_buffer.empty()
                                      ? &client->incoming_buffer[0]
                                      : client->accumulate_incoming_buffer.c_str();

                    auto buffer_size = client->accumulate_incoming_buffer.empty()
                                           ? bytes_transferred
                                           : client->accumulate_incoming_buffer.size();

                    auto size_packet = protocol::expected_packet_size(&buffer[0]);

                    if (size_packet > buffer_size) {
                        if (client->accumulate_incoming_buffer.empty()) {
                            push_back_buffer();
                        }
                        client->async_read();
                        return;
                    }

                    auto ec = client->commandHandler.parse(buffer, buffer_size);
                    if (client->accumulate_incoming_buffer.empty()) {
                        client->accumulate_incoming_buffer.erase(0, size_packet);
                    }
                    if (ec) {
                        std::cout << "async_read, parse failed: " << ec << ": " << ec.message() << std::endl;
                        client->disconnect();
                        return;
                    }
                    client->async_read();
                }),
            [](const boost::system::error_code& ec) {
                std::cout << "async_read error: " << ec << ": " << ec.message() << std::endl;
            }));
}

std::uint32_t Client::next_id()
{
    return counter_id++;
}

} // namespace tsvetkov