//
// Created by mtsvetkov on 30.09.2020.
//

#pragma once

#include "asio.hpp"
#include "protocol/command_handler.hpp"

#include "portable_concurrency/future"

#include "boost/container_hash/hash.hpp"

#include <functional>
#include <unordered_set>

namespace tsvetkov {
struct FoundDevice
{
    FoundDevice(protocol::DeviceType type_device,
                std::uint32_t high_device_id,
                std::uint32_t low_device_id,
                std::string ip_address)
        : type_device(type_device), high_device_id(high_device_id), low_device_id(low_device_id), ip_address(ip_address)
    {
    }

    protocol::DeviceType type_device;
    std::uint32_t high_device_id;
    std::uint32_t low_device_id;
    std::string ip_address;

    bool operator==(const FoundDevice& other) const
    {
        return std::tie(type_device, high_device_id, low_device_id, ip_address) ==
               std::tie(other.type_device, other.high_device_id, other.low_device_id, other.ip_address);
    }
};
} // namespace tsvetkov
namespace std {
template<>
struct hash<tsvetkov::FoundDevice>
{
    std::size_t operator()(const tsvetkov::FoundDevice& v) const
    {
        std::size_t seed = 0;
        boost::hash_combine(seed, v.type_device);
        boost::hash_combine(seed, v.high_device_id);
        boost::hash_combine(seed, v.low_device_id);
        boost::hash_combine(seed, v.ip_address);
        return seed;
    }
};
} // namespace std
namespace tsvetkov {
class ClientFinder : public std::enable_shared_from_this<ClientFinder>
{
public:
    explicit ClientFinder(asio::io_context& io);

    using found_new_device_type = std::function<void(FoundDevice)>;

    void subscribe_to_found_new_device_event(found_new_device_type sub);

    void start();
    void stop();

private:
    using knock_knock_command_buffer_type = std::array<char, protocol::KnockKnock::packet_size>;
    using receive_buffer_type             = std::array<char, 65507>;

    void impl_send_packet();
    void async_read();

    template<typename F>
    auto async_post(F f)
    {
        return pc::async(client_finder_strand_, [f = std::forward<F>(f)]() mutable { f(); });
    }

    asio::io_context& io_context;
    asio::io_context::strand client_finder_strand_;
    asio::ip::udp::endpoint broadcast_endpoint_;
    asio::ip::udp::endpoint unicast_endpoint_;
    asio::ip::udp::endpoint sender_endpoint_;
    asio::ip::udp::socket broadcast_socket_;
    asio::ip::udp::socket unicast_socket_;
    std::shared_ptr<knock_knock_command_buffer_type> msg_;
    std::shared_ptr<receive_buffer_type> receive_buffer_;
    pc::future<void> next_send_task_;
    protocol::CommandHandler commandHandler_;
    found_new_device_type found_new_device_;
    std::unordered_set<FoundDevice> found_devices_;
};
} // namespace tsvetkov
