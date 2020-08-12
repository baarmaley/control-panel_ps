//
// Created by mtsvetkov on 19.07.2020.
//

#pragma once

#include "boost/asio.hpp"

#include "protocol/command_handler.hpp"

#include <deque>

namespace tsvetkov {
namespace asio = boost::asio;

struct Client : std::enable_shared_from_this<Client>
{
    Client(asio::io_context& io, const std::string& remote_address, std::uint16_t port);

    Client(const Client&) = delete;
    Client(Client&&)      = delete;

    Client& operator=(const Client&) = delete;
    Client& operator=(Client&&) = delete;

    void connect();
    void disconnect();

    void send_all_on();
    void send_all_off();

    void inversion(std::uint8_t pin);

private:
    template<typename Buffer>
    void push_to_queue(Buffer buffer)
    {
        output_buffer.emplace_back(buffer.begin(), buffer.end());
        async_write();
    }

    void send_hello_request();
    void async_write();
    void async_read();

    std::uint32_t next_id();

    asio::ip::tcp::socket socket;
    asio::ip::tcp::endpoint endpoint;

    asio::steady_timer reconnect_timer;
    asio::steady_timer ping_timer;

    bool is_connected = false;

    std::uint32_t counter_id = 0;

    std::array<char, 1024> incoming_buffer;
    std::string accumulate_incoming_buffer;

    std::deque<std::string> output_buffer;

    protocol::CommandHandler commandHandler;
};
} // namespace tsvetkov