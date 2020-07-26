//
// Created by m.tsvetkov on 26.06.2020.
//

#include "boost/endian/conversion.hpp"
#include "catch2/catch.hpp"

#include "protocol/command_handler.hpp"

namespace protocol = tsvetkov::protocol;

TEST_CASE("protocol test")
{
    protocol::register_big_endian_to_native(&boost::endian::big_to_native);
    protocol::register_native_to_big_endian(&boost::endian::native_to_big);

    std::uint32_t value_test = 1;
    REQUIRE(protocol::big_endian_to_native(protocol::native_to_big_endian(value_test)) == value_test);

    protocol::CommandHandler command_handler;

    bool trigger_hello_request     = false;
    std::uint32_t hello_request_id = 0;
    command_handler.subscribe([&trigger_hello_request, &hello_request_id](std::uint32_t id, protocol::HelloRequest) {
        trigger_hello_request = true;
        hello_request_id      = id;
    });

    auto hello_request = protocol::make_hello_request(1024);

    command_handler.parse(&hello_request[0], hello_request.size());
    REQUIRE(trigger_hello_request);
    REQUIRE(hello_request_id == 1024);

    bool trigger_hello_response     = false;
    std::uint32_t hello_response_id = 0;

    protocol::HelloResponse original_hello_response;

    original_hello_response.type_device    = protocol::DeviceType::SmartPowerStrip;
    original_hello_response.high_device_id = 16777215;
    original_hello_response.low_device_id  = 215777167;

    auto hello_response_buffer = protocol::make_hello_response(222555, original_hello_response);

    protocol::HelloResponse hello_response;

    command_handler.subscribe([&trigger_hello_response, &hello_response_id, &hello_response](
                                  std::uint32_t id, protocol::HelloResponse response) {
        trigger_hello_response = true;
        hello_response_id      = id;
        hello_response         = response;
    });

    command_handler.parse(&hello_response_buffer[0], hello_response_buffer.size());

    REQUIRE(hello_response_id == 222555);
    REQUIRE(original_hello_response.type_device == hello_response.type_device);
    REQUIRE(original_hello_response.high_device_id == hello_response.high_device_id);
    REQUIRE(original_hello_response.low_device_id == hello_response.low_device_id);

    protocol::Error smart_power_notification_error = protocol::Error::NoError;
    std::unordered_map<std::uint8_t, protocol::SmartPowerStatus::Status> status;
    status.emplace(std::uint8_t{0}, protocol::SmartPowerStatus::Status::On);
    status.emplace(std::uint8_t{1}, protocol::SmartPowerStatus::Status::Off);
    status.emplace(std::uint8_t{2}, protocol::SmartPowerStatus::Status::On);
    status.emplace(std::uint8_t{3}, protocol::SmartPowerStatus::Status::Off);

    auto smart_power_notification_buffer =
        protocol::make_smart_power_notification(smart_power_notification_error, status);

    for (std::uint8_t i = 4; i < 255; ++i) {
        status.emplace(i, protocol::SmartPowerStatus::Status::On);
    }

    protocol::Error overflow_smart_power_notification_error = protocol::Error::NoError;
    auto overflow_smart_power_notification_buffer =
        protocol::make_smart_power_notification(overflow_smart_power_notification_error, status);
    REQUIRE(overflow_smart_power_notification_error == protocol::Error::OverflowBuffer);

    REQUIRE(smart_power_notification_error == protocol::Error::NoError);

    bool trigger_smart_power_status_notification     = false;

    protocol::SmartPowerStatus original_smart_power_status_notification;

    command_handler.subscribe(
        [&trigger_smart_power_status_notification,
         &original_smart_power_status_notification](protocol::SmartPowerStatus notification) {
            trigger_smart_power_status_notification  = true;
            original_smart_power_status_notification = std::move(notification);
        });

    command_handler.parse(smart_power_notification_buffer.data(), smart_power_notification_buffer.size());

    REQUIRE(trigger_smart_power_status_notification);
    REQUIRE(original_smart_power_status_notification.status.size() == 4);

    auto check_smart_power_status = [&](std::uint8_t pin, protocol::SmartPowerStatus::Status pin_status) {
        const auto& status = original_smart_power_status_notification.status;
        auto it            = status.find(pin);
        REQUIRE_FALSE(it == status.end());
        REQUIRE(it->second == pin_status);
    };

    check_smart_power_status(std::uint8_t{0}, protocol::SmartPowerStatus::Status::On);
    check_smart_power_status(std::uint8_t{1}, protocol::SmartPowerStatus::Status::Off);
    check_smart_power_status(std::uint8_t{2}, protocol::SmartPowerStatus::Status::On);
    check_smart_power_status(std::uint8_t{3}, protocol::SmartPowerStatus::Status::Off);

    bool trigger_all_on_command = false;

    command_handler.subscribe(
        [&trigger_all_on_command](std::uint32_t, protocol::AllOnCommand) { trigger_all_on_command = true; });

    auto all_on_command_buffer = protocol::make_all_on_command(2);
    command_handler.parse(&all_on_command_buffer[0], all_on_command_buffer.size());

    REQUIRE(trigger_all_on_command);

    bool trigger_all_off_command = false;

    command_handler.subscribe(
        [&trigger_all_off_command](std::uint32_t, protocol::AllOffCommand) { trigger_all_off_command = true; });

    auto all_off_command_buffer = protocol::make_all_off_command(2);

    command_handler.parse(&all_off_command_buffer[0], all_off_command_buffer.size());

    REQUIRE(trigger_all_off_command);

    bool trigger_inversion_command = false;
    std::uint8_t inversion_pin     = 0;

    command_handler.subscribe(
        [&trigger_inversion_command, &inversion_pin](std::uint32_t, protocol::Inversion inversion) {
            trigger_inversion_command = true;
            inversion_pin             = inversion.port;
        });

    auto inversion_buffer = protocol::make_inversion_command(658, 128);
    command_handler.parse(&inversion_buffer[0], inversion_buffer.size());

    REQUIRE(trigger_inversion_command);
    REQUIRE(inversion_pin == 128);

    bool trigger_ok_response = false;

    command_handler.subscribe(
        [&trigger_ok_response](std::uint32_t, protocol::OkResponse) { trigger_ok_response = true; });

    auto ok_response_buffer = protocol::make_ok_response(2);

    command_handler.parse(&ok_response_buffer[0], ok_response_buffer.size());

    REQUIRE(trigger_ok_response);

    bool trigger_error_response = false;
    protocol::ErrorResponseType error_response_type;

    command_handler.subscribe(
        [&trigger_error_response, &error_response_type](std::uint32_t, protocol::ErrorResponse error_response) {
            trigger_error_response = true;
            error_response_type    = error_response.error_response_type;
        });

    auto error_response_buffer = protocol::make_error_response(2, protocol::ErrorResponseType::VersionError);
    command_handler.parse(&error_response_buffer[0], error_response_buffer.size());

    REQUIRE(trigger_error_response);
    REQUIRE(error_response_type == protocol::ErrorResponseType::VersionError);
}
