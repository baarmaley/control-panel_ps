//
// Created by mtsvetkov on 30.09.2020.
//

#include "client_finder.hpp"

#include "common/action_if_exists.hpp"
#include "common/pc_adapters.hpp"

#include <iostream>

namespace tsvetkov {
ClientFinder::ClientFinder(asio::io_context& io)
    : io_context(io),
      client_finder_strand_(io),
      broadcast_endpoint_(asio::ip::address_v4::broadcast(), 5500),
      unicast_endpoint_(asio::ip::udp::v4(), 8000),
      broadcast_socket_(io, broadcast_endpoint_.protocol()),
      unicast_socket_(io, unicast_endpoint_),
      msg_(std::make_shared<knock_knock_command_buffer_type>(protocol::make_knock_knock_command(0, 8000))),
      receive_buffer_(std::make_shared<receive_buffer_type>())
{
    broadcast_socket_.set_option(asio::socket_base::broadcast(true));
    commandHandler_.subscribe([this](std::uint32_t, protocol::HelloResponse hello_response) {
        auto it = found_devices_.emplace(hello_response.type_device,
                                         hello_response.high_device_id,
                                         hello_response.low_device_id,
                                         sender_endpoint_.address().to_string());
        if (it.second && found_new_device_) {
            found_new_device_(*it.first);
        }
    });
}

void ClientFinder::subscribe_to_found_new_device_event(found_new_device_type sub)
{
    found_new_device_ = std::move(sub);
}

void ClientFinder::start()
{
    async_post(action_if_exists(make_single_context(shared_from_this()), [](ClientFinder* self) {
        self->broadcast_socket_.bind(self->broadcast_endpoint_);
        self->async_read();
        self->impl_send_packet();
    })).detach();
}

void ClientFinder::async_read()
{
    unicast_socket_.async_receive_from(asio::buffer(*receive_buffer_), sender_endpoint_, use_future)
        .next(client_finder_strand_,
              action_if_exists(make_single_context(shared_from_this()),
                               [receive_buffer = receive_buffer_](ClientFinder* self, std::size_t size) {
                                   std::cout << "Packet ok! " << size << std::endl;
                                   auto ec = self->commandHandler_.parse(receive_buffer->data(), size);
                                   if (ec) {
                                       std::cout << "ClientFinder::async_read(): " << ec.message() << std::endl;
                                   }
                                   self->async_read();
                               }))
        .detach();
}

void ClientFinder::impl_send_packet()
{
    auto single_ctx = make_single_context(shared_from_this());
    broadcast_socket_.async_send_to(asio::buffer(*msg_), broadcast_endpoint_, use_future)
        .next(client_finder_strand_,
              action_if_exists(
                  single_ctx,
                  [single_ctx, msg = msg_](ClientFinder* self, std::size_t) {
                      std::cout << "udp ok" << std::endl;
                      auto timer = std::make_shared<asio::steady_timer>(self->io_context);
                      timer->expires_from_now(std::chrono::seconds(5));
                      self->next_send_task_ =
                          timer->async_wait(use_future)
                              .then([single_ctx, timer](pc::promise<void> complete, pc::future<void> future) {
                                  if (!complete.is_awaiten()) {
                                      return;
                                  }
                                  try {
                                      future.get();
                                      action_if_exists(single_ctx,
                                                       [](ClientFinder* self) { self->impl_send_packet(); })();
                                  } catch (const context_is_destroyed&) {
                                      std::cout << "Client finder error: context is destroyed" << std::endl;
                                  } catch (const std::exception& e) {
                                      std::cout << "Client finder error: exception " << e.what() << std::endl;
                                  }
                              });
                  }))
        .then([](pc::future<void> f) {
            try {
                f.get();
            } catch (const std::exception& e) {
                std::cout << "Client finder error: " << e.what() << std::endl;
            }
        })
        .detach();
}

void ClientFinder::stop()
{
    async_post(action_if_exists(make_single_context(shared_from_this()), [](ClientFinder* self) {
        self->broadcast_socket_.close();
    })).detach();
}

} // namespace tsvetkov