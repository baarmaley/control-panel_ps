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
    : socket(io), endpoint(asio::ip::make_address(remote_address), port), reconnect_timer(io), ping_timer(io)
{
    commandHandler.subscribe([](std::uint32_t id, protocol::HelloResponse hello_response) {
        std::cout << "HelloResponse" << std::endl;
        std::cout << "hello_response.type_device: " << static_cast<std::uint32_t>(hello_response.type_device)
                  << std::endl;
        std::cout << "hello_response.low_device_id: " << hello_response.low_device_id << std::endl;
        std::cout << "hello_response.high_device_id: " << hello_response.high_device_id << std::endl;
    });
    commandHandler.subscribe([](protocol::SmartPowerStatus smart_power_status) {
        std::cout << "Status notification" << std::endl;
        for (const auto& item : smart_power_status.status) {
            std::cout << "smart_power_status, pin: " << static_cast<int>(item.first)
                      << " status: " << (item.second == protocol::SmartPowerStatus::Status::On ? "On" : "Off")
                      << std::endl;
        }
    });
    commandHandler.subscribe([](std::uint32_t id, protocol::OkResponse) {
        std::cout << "OkResponse" << std::endl;
        std::cout << "id: " << id << std::endl;
    });
    commandHandler.subscribe([](std::uint32_t id, protocol::ErrorResponse error_response) {
        std::cout << "ErrorResponse" << std::endl;
        std::cout << "id: " << id << std::endl;
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
                                                                self->async_read();
                                                                self->send_hello_request();
                                                            }),
                                 [](const boost::system::error_code& ec) {
                                     std::cout << "async_connect error: " << ec << ": " << ec.message() << std::endl;
                                 }));
}

void Client::disconnect() {
    boost::system::error_code ec;
    socket.close(ec);
    if(ec){
        std::cout << "Client::disconnect()" << ec << ": " << ec.message() << std::endl;
    }
}

void Client::send_all_on()
{
    push_to_queue(tsvetkov::protocol::make_all_on_command(next_id()));
}
void Client::send_all_off()
{
    push_to_queue(tsvetkov::protocol::make_all_off_command(next_id()));
}

void Client::inversion(std::uint8_t pin)
{
    push_to_queue(tsvetkov::protocol::make_inversion_command(next_id(), pin));
}

void Client::send_hello_request()
{
    push_to_queue(tsvetkov::protocol::make_hello_request(next_id()));
}

void Client::async_write()
{
    if (output_buffer.size() != 1) {
        return;
    }
    const auto& front = output_buffer.front();
    asio::async_write(socket,
                      asio::buffer(front),
                      tsvetkov::make_asio_task(
                          tsvetkov::action_if_exists(make_single_context(shared_from_this()),
                                                     [](Client* self, std::size_t bytes_transferred) {
                                                         std::cout
                                                             << "async_write, bytes_transferred: " << bytes_transferred
                                                             << " buffer: " << self->output_buffer.front().size()
                                                             << std::endl;
                                                         self->output_buffer.pop_front();
                                                         self->async_write();
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

                    client->accumulate_incoming_buffer.append(&client->incoming_buffer[0], bytes_transferred);

                    while (true) {
                        // size packet
                        if (client->accumulate_incoming_buffer.size() < protocol::Message::packet_size) {
                            break;
                        }

                        auto data        = &client->accumulate_incoming_buffer[0];
                        auto size_packet = protocol::expected_packet_size(data);

                        if (size_packet > client->accumulate_incoming_buffer.size()) {
                            break;
                        }

                        auto ec = client->commandHandler.parse(data, client->accumulate_incoming_buffer.size());
                        if (!client->accumulate_incoming_buffer.empty()) {
                            client->accumulate_incoming_buffer.erase(0, size_packet);
                        }
                        if (ec) {
                            std::cout << "async_read, parse failed: " << ec << ": " << ec.message() << std::endl;
                            client->disconnect();
                            return;
                        }
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