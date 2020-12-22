//
// Created by mtsvetkov on 19.07.2020.
//

#pragma once

#include "asio.hpp"
#include "portable_concurrency/future"

#include "protocol/command_handler.hpp"

#include "common/action_if_exists.hpp"

#include <deque>
#include <optional>
#include <type_traits>

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
    template<typename F, typename... Args>
    pc::future<std::optional<protocol::ErrorResponseType>> async_add_request(F&& f, Args... args)
    {
        pc::promise<std::optional<protocol::ErrorResponseType>> request_promise;
        auto result = request_promise.get_future();
        async_post(
            action_if_exists(make_single_context(shared_from_this()),
                             [f               = std::forward<F>(f),
                              request_promise = std::move(request_promise),
                              args = std::make_tuple<typename std::decay_t<Args>...>(std::forward<Args>(args)...)](
                                 Client* self) mutable {
                                 auto id = self->next_id();
                                 self->request.emplace(std::piecewise_construct,
                                                       std::forward_as_tuple(id),
                                                       std::forward_as_tuple(std::move(request_promise)));
                                 self->push_to_queue(std::apply(f, std::tuple_cat(std::tie(id), std::move(args))));
                             }))
            .detach();
        return result;
    }

    void response(std::uint32_t id, std::optional<protocol::ErrorResponseType> error_response);

    template<typename Buffer>
    void push_to_queue(Buffer buffer)
    {
        output_buffer.emplace_back(buffer.begin(), buffer.end());
        async_write();
    }

    template<typename F>
    auto async_post(F f)
    {
        return pc::async(client_strand, [f = std::forward<F>(f)]() mutable { f(); });
    }

    void impl_async_connect();
    template<typename F>
    void set_async_connect_result(F&& f){
        std::vector<pc::promise<protocol::SmartPowerStatus>> temp_connections_to_client;
        std::swap(connections_to_client, temp_connections_to_client);
        std::for_each(temp_connections_to_client.begin(), temp_connections_to_client.end(), std::forward<F>(f));
    }

    void impl_disconnect();

    void send_hello_request();
    void send_ping();
    void async_write();
    void async_read();

    void start_ping();
    void reconnect();

    void system_error_filter(const std::system_error& error, std::function<void(Client*)> f){
        if (error.code() == asio::error::basic_errors::operation_aborted) {
            return;
        }
        f(this);
    }


    std::uint32_t next_id();

    asio::io_context& io_context;
    asio::io_context::strand client_strand;
    asio::ip::tcp::socket socket;
    asio::ip::tcp::endpoint endpoint;

    std::chrono::steady_clock::time_point last_response_ping;
    pc::future<void> ping_task;


    // Connection task
    // step 1
    std::optional<pc::promise<protocol::HelloResponse>> hello_response_promise;
    // step 2
    std::optional<pc::promise<protocol::SmartPowerStatus>> smart_power_status_promise;

    std::vector<pc::promise<protocol::SmartPowerStatus>> connections_to_client;

    bool is_connected = false;

    std::uint32_t counter_id = 0;

    std::string accumulate_incoming_buffer;

    bool is_async_write = false;
    std::deque<std::string> output_buffer;

    std::unordered_map<std::uint32_t, pc::promise<std::optional<protocol::ErrorResponseType>>> request;

    protocol::CommandHandler commandHandler;
};
} // namespace tsvetkov