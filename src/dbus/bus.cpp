#include "dbus/bus.h"

#include <utility>

namespace broseat::dbus {

std::unique_ptr<Bus> Bus::open_system(std::string* error) {
    auto bus = brodbus::Bus::open_system(error);
    if (!bus) return nullptr;
    return std::make_unique<Bus>(std::move(*bus));
}

std::unique_ptr<Bus> Bus::open_user(std::string* error) {
    auto bus = brodbus::Bus::open_user(error);
    if (!bus) return nullptr;
    return std::make_unique<Bus>(std::move(*bus));
}

std::unique_ptr<Bus> Bus::open_address(const std::string& address, std::string* error) {
    auto bus = brodbus::Bus::open_address(address, error);
    if (!bus) return nullptr;
    return std::make_unique<Bus>(std::move(*bus));
}

bool Bus::call_string_array(
    const std::string& destination,
    const std::string& path,
    const std::string& interface,
    const std::string& member,
    const std::vector<std::string>& strings,
    std::string* error) {
    if (!raw()) {
        if (error) *error = "bus not connected";
        return false;
    }

    auto m = new_method_call(destination, path, interface, member, error);
    if (!m.is_valid()) return false;

    if (!m.append_string_list(strings)) {
        if (error) *error = "failed to append strings";
        return false;
    }

    brodbus::Error err;
    auto reply = call(m, 0, &err);
    if (err.is_set()) {
        if (error) {
            const char* msg = err.message();
            *error = (msg && *msg) ? msg : err.to_string();
        }
        return false;
    }
    return true;
}

}  // namespace broseat::dbus
