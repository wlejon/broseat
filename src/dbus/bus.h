#pragma once

#include <systemd/sd-bus.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace broseat::dbus {

class Slot {
public:
    Slot() = default;
    Slot(sd_bus_slot* slot, std::shared_ptr<void> userdata);
    ~Slot();

    Slot(const Slot&) = delete;
    Slot& operator=(const Slot&) = delete;

    Slot(Slot&& other) noexcept;
    Slot& operator=(Slot&& other) noexcept;

    void reset();
    bool is_valid() const noexcept { return slot_ != nullptr; }

private:
    sd_bus_slot* slot_ = nullptr;
    std::shared_ptr<void> userdata_;
};

using SignalCallback = std::function<void(sd_bus_message* msg)>;

class Bus {
public:
    Bus() = default;
    explicit Bus(sd_bus* bus);
    ~Bus();

    Bus(const Bus&) = delete;
    Bus& operator=(const Bus&) = delete;

    Bus(Bus&& other) noexcept;
    Bus& operator=(Bus&& other) noexcept;

    static std::unique_ptr<Bus> open_system(std::string* error = nullptr);
    static std::unique_ptr<Bus> open_user(std::string* error = nullptr);
    static std::unique_ptr<Bus> open_address(const std::string& address, std::string* error = nullptr);

    sd_bus* raw() const noexcept { return bus_; }
    bool is_valid() const noexcept { return bus_ != nullptr; }

    int get_fd() const noexcept;
    int process();
    int wait(uint64_t timeout_usec = UINT64_MAX);

    Slot add_match(const std::string& match_rule, SignalCallback callback, std::string* error = nullptr);

    bool get_property_bool(
        const std::string& destination,
        const std::string& path,
        const std::string& interface,
        const std::string& property,
        bool* out,
        std::string* error = nullptr);

    bool get_property_string(
        const std::string& destination,
        const std::string& path,
        const std::string& interface,
        const std::string& property,
        std::string* out,
        std::string* error = nullptr);

    bool get_property_uint32(
        const std::string& destination,
        const std::string& path,
        const std::string& interface,
        const std::string& property,
        uint32_t* out,
        std::string* error = nullptr);

    bool call_string_array(
        const std::string& destination,
        const std::string& path,
        const std::string& interface,
        const std::string& member,
        const std::vector<std::string>& strings,
        std::string* error = nullptr);

private:
    sd_bus* bus_ = nullptr;
};

}  // namespace broseat::dbus
