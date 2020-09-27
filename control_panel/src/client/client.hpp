//
// Created by mtsvetkov on 19.07.2020.
//

#pragma once

#include "asio.hpp"
#include "portable_concurrency/future"

#include "protocol/command_handler.hpp"

#include <deque>
#include <optional>

namespace tsvetkov {

struct Client : std::enable_shared_from_this<Client>
{
    Client(asio::io_context& io, const std::string& remote_address, std::uint16_t port);

    Client(const Client&) = delete;
    Client(Client&&)      = delete;

    Client& operator=(const Client&) = delete;
    Client& operator=(Client&&) = delete;

    pc::future<protocol::SmartPowerStatus> async_connect();
    protocol::SmartPowerStatus connect();

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

    template<typename F>
    auto async_post(F f)
    {
        return pc::async(client_strand, [f = std::forward<F>(f)] { f(); });
    }

    void send_hello_request();
    void async_write();
    void async_read();

    std::uint32_t next_id();

    asio::io_context& io_context;
    asio::io_context::strand client_strand;
    asio::ip::tcp::socket socket;
    asio::ip::tcp::endpoint endpoint;

    asio::steady_timer reconnect_timer;
    asio::steady_timer ping_timer;

    // Connection task
    // step 1
    std::optional<pc::promise<protocol::HelloResponse>> hello_response_promise;
    // step 2
    std::optional<pc::promise<protocol::SmartPowerStatus>> smart_power_status_promise;

    bool is_connected = false;

    std::uint32_t counter_id = 0;

    std::array<char, 1024> incoming_buffer;
    std::string accumulate_incoming_buffer;

    std::deque<std::string> output_buffer;
    std::unordered_map<std::uint32_t, pc::future<void>> response;

    protocol::CommandHandler commandHandler;
};
} // namespace tsvetkov