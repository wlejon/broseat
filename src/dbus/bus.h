#pragma once

#include <brodbus/brodbus.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace broseat::dbus {

using Slot = brodbus::Slot;
using SignalCallback = brodbus::Bus::RawSignalCallback;

class Bus : public brodbus::Bus {
public:
    using brodbus::Bus::Bus;

    Bus() noexcept = default;
    ~Bus() = default;

    Bus(const Bus&) = delete;
    Bus& operator=(const Bus&) = delete;

    Bus(Bus&&) noexcept = default;
    Bus& operator=(Bus&&) noexcept = default;

    Bus(brodbus::Bus&& other) noexcept : brodbus::Bus(std::move(other)) {}
    Bus& operator=(brodbus::Bus&& other) noexcept {
        brodbus::Bus::operator=(std::move(other));
        return *this;
    }

    static std::unique_ptr<Bus> open_system(std::string* error = nullptr);
    static std::unique_ptr<Bus> open_user(std::string* error = nullptr);
    static std::unique_ptr<Bus> open_address(const std::string& address, std::string* error = nullptr);

    bool call_string_array(
        const std::string& destination,
        const std::string& path,
        const std::string& interface,
        const std::string& member,
        const std::vector<std::string>& strings,
        std::string* error = nullptr);
};

}  // namespace broseat::dbus
