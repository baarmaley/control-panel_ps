//
// Created by mtsvetkov on 19.07.2020.
//

#include "client.hpp"

#include "common/action_if_exists.hpp"
#include "common/pc_adapters.hpp"
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
    : io_context(io), client_strand(io), socket(io), endpoint(asio::ip::make_address(remote_address), port)
{
    commandHandler.subscribe([this](std::uint32_t id, protocol::HelloResponse hello_response) {
        std::cout << "HelloResponse" << std::endl;
        std::cout << "hello_response.type_device: " << static_cast<std::uint32_t>(hello_response.type_device)
                  << std::endl;
        std::cout << "hello_response.low_device_id: " << hello_response.low_device_id << std::endl;
        std::cout << "hello_response.high_device_id: " << hello_response.high_device_id << std::endl;

        // Connection task, step 1
        if (this->hello_response_promise) {
            this->hello_response_promise->set_value(hello_response);
        }
    });
    commandHandler.subscribe([this](protocol::SmartPowerStatus smart_power_status) {
        std::cout << "Status notification" << std::endl;
        for (const auto& item : smart_power_status.status) {
            std::cout << "smart_power_status, pin: " << static_cast<int>(item.first)
                      << " status: " << (item.second == protocol::SmartPowerStatus::Status::On ? "On" : "Off")
                      << std::endl;
        }

        // Connection task, step 2
        if (this->smart_power_status_promise) {
            this->smart_power_status_promise->set_value(std::move(smart_power_status));
        }
    });
    commandHandler.subscribe([this](std::uint32_t id, protocol::OkResponse) {
        //        std::cout << "OkResponse" << std::endl;
        //        std::cout << "id: " << id << std::endl;
        response(id, std::nullopt);
    });
    commandHandler.subscribe([this](std::uint32_t id, protocol::ErrorResponse error_response) {
        std::cout << "ErrorResponse" << std::endl;
        std::cout << "id: " << id << std::endl;
        response(id, error_response.error_response_type);
    });
}

pc::future<protocol::SmartPowerStatus> Client::async_connect()
{
    return pc::async(client_strand, action_if_exists(make_single_context(shared_from_this()), [](Client* self) {
                         self->connections_to_client.emplace_back();
                         if (self->connections_to_client.size() == 1) {
                             self->impl_async_connect();
                         }
                         return self->connections_to_client.back().get_future();
                     }));
}

void Client::impl_async_connect()
{
    ping_task = {};
    output_buffer.clear();
    auto single_ctx = make_single_context(shared_from_this());
    asio::async_connect(socket, std::vector<asio::ip::tcp::endpoint>{endpoint}, use_future)
        .next(client_strand,
              action_if_exists(single_ctx,
                               [](Client* self, const asio::ip::tcp::endpoint&) {
                                   std::cout << "async_connect ok!" << std::endl;
                                   self->async_read();
                                   self->hello_response_promise = pc::promise<protocol::HelloResponse>();
                                   self->send_hello_request();
                                   return self->hello_response_promise->get_future();
                               }))
        .next(action_if_exists(single_ctx,
                               [](Client* self, protocol::HelloResponse) {
                                   self->hello_response_promise.reset();
                                   self->smart_power_status_promise = pc::promise<protocol::SmartPowerStatus>();
                                   return self->smart_power_status_promise->get_future();
                               }))
        .next(action_if_exists(single_ctx,
                               [](Client* self, protocol::SmartPowerStatus smart_power_status) {
                                   self->set_async_connect_result(
                                       [&](pc::promise<protocol::SmartPowerStatus>& promise) {
                                           promise.set_value(smart_power_status);
                                       });
                                   self->last_response_ping = std::chrono::steady_clock::now();
                                   self->start_ping();
                               }))
        .then([single_ctx](pc::future<void> future) {
            try {
                future.get();
            } catch (const context_is_destroyed&) {
                std::cout << "Connection error: context is destroyed" << std::endl;
            } catch (const std::system_error& e) {
                std::cout << "Connection system_error: " << e.what() << std::endl;
                action_if_exists(single_ctx, &Client::system_error_filter)(e, [single_ctx](Client* self) {
                    auto reconnect_timer = std::make_shared<asio::steady_timer>(self->io_context);
                    reconnect_timer->expires_after(std::chrono::seconds(5));
                    reconnect_timer->async_wait(use_future)
                        .next(self->client_strand,
                              action_if_exists(single_ctx,
                                               [reconnect_timer](Client* self) { self->impl_async_connect(); }))
                        .detach();
                });
            } catch (const std::exception& e) {
                std::cout << "Connection error: exception " << e.what() << std::endl;
            }
        })
        .detach();
}

protocol::SmartPowerStatus Client::connect()
{
    return async_connect().get();
}

void Client::disconnect()
{
    async_post([this] {
        std::error_code ec;
        socket.close(ec);
        if (ec) {
            std::cout << "Client::disconnect()" << ec << ": " << ec.message() << std::endl;
        }
    }).get();
}

void Client::send_all_on()
{
    async_add_request(&tsvetkov::protocol::make_all_on_command).get();
}
void Client::send_all_off()
{
    async_add_request(&tsvetkov::protocol::make_all_off_command).get();
}

void Client::inversion(std::uint8_t pin)
{
    async_add_request(&tsvetkov::protocol::make_inversion_command, pin).get();
}

void Client::send_hello_request()
{
    push_to_queue(tsvetkov::protocol::make_hello_request(next_id()));
}

void Client::send_ping()
{
    async_add_request(tsvetkov::protocol::make_ping_command)
        .next(client_strand,
              action_if_exists(make_single_context(shared_from_this()),
                               [](Client* self, std::optional<protocol::ErrorResponseType>) {
                                   self->last_response_ping = std::chrono::steady_clock::now();
                               }))
        .detach();
}

void Client::async_write()
{
    if (is_async_write || output_buffer.empty()) {
        return;
    }
    is_async_write = true;
    auto buffer    = std::make_unique<std::string>(std::move(output_buffer.front()));
    output_buffer.pop_front();
    auto buffer_ptr = buffer.get();
    auto single_ctx = make_single_context(shared_from_this());
    asio::async_write(socket, asio::buffer(*buffer_ptr), use_future)
        .next(client_strand,
              action_if_exists(single_ctx,
                               [buffer = std::move(buffer)](Client* self, std::size_t bytes_transferred) {
                                   //                                   std::cout << "async_write, bytes_transferred: "
                                   //                                   << bytes_transferred
                                   //                                             << " buffer: " << buffer->size() <<
                                   //                                             std::endl;
                                   self->is_async_write = false;
                                   self->async_write();
                               }))
        .then([single_ctx](pc::future<void> future) {
            try {
                future.get();
            } catch (const std::system_error& e) {
                std::cout << "async_write system_error: " << e.what() << std::endl;
                action_if_exists(single_ctx, &Client::system_error_filter)(e, [](Client* self) {
                    self->impl_disconnect();
                    self->reconnect();
                });
            } catch (const std::exception& e) {
                std::cout << "async_write error: " << e.what() << std::endl;
            }
        })
        .detach();
}

void Client::async_read()
{
    auto single_ctx          = make_single_context(shared_from_this());
    auto incoming_buffer     = std::make_unique<std::array<char, 1024>>();
    auto incoming_buffer_ptr = incoming_buffer.get();
    socket.async_read_some(asio::buffer(*incoming_buffer_ptr), use_future)
        .next(client_strand,
              action_if_exists(
                  single_ctx,
                  [incoming_buffer = std::move(incoming_buffer)](Client* client, std::size_t bytes_transferred) {
                      //                      std::cout << "async_read, bytes_transferred: " << bytes_transferred <<
                      //                      std::endl;

                      client->accumulate_incoming_buffer.append(&(*incoming_buffer)[0], bytes_transferred);

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
                  }))
        .then([single_ctx](pc::future<void> future) {
            try {
                future.get();
            } catch (const std::system_error& e) {
                std::cout << "async read system_error: " << e.what() << std::endl;
                action_if_exists(single_ctx, &Client::system_error_filter)(e, [](Client* self) {
                    self->impl_disconnect();
                    self->reconnect();
                });
            } catch (const std::exception& e) {
                std::cout << "async read error: " << e.what() << std::endl;
            }
        })
        .detach();
}

void Client::start_ping()
{
    auto timer = std::make_shared<asio::steady_timer>(io_context);
    timer->expires_after(std::chrono::seconds(1));
    ping_task = timer->async_wait(use_future)
                    .then(client_strand,
                          action_if_exists(make_single_context(shared_from_this()),
                                           [timer](Client* self, pc::promise<void> complete, pc::future<void> future) {
                                               if (!complete.is_awaiten()) {
                                                   return;
                                               }
                                               auto now = std::chrono::steady_clock::now();
                                               if (now - self->last_response_ping > std::chrono::seconds(5)) {
                                                   self->impl_disconnect();
                                                   self->reconnect();
                                               } else {
                                                   self->send_ping();
                                                   self->start_ping();
                                               }
                                           }));
}

void Client::reconnect()
{
    if (!connections_to_client.empty()) {
        return;
    }
    async_connect().detach();
}

std::uint32_t Client::next_id()
{
    return counter_id++;
}

void Client::response(std::uint32_t id, std::optional<protocol::ErrorResponseType> error_response)
{
    auto it = request.find(id);
    if (it == request.end()) {
        return;
    }
    it->second.set_value(error_response);
    request.erase(it);
}

void Client::impl_disconnect()
{
    std::error_code ec;
    socket.close(ec);
    if (ec) {
        std::cout << "Client::disconnect()" << ec << ": " << ec.message() << std::endl;
    }
}

} // namespace tsvetkov