#include "broseat/session.h"
#include "dbus/bus.h"

#include <cstdlib>
#include <string>

namespace broseat {

namespace {

bool call_unit_mode_method(
    const std::string& member,
    const std::string& unit_name,
    const std::string& mode,
    std::string* error) {
    std::string dbus_err;
    auto user_bus = dbus::Bus::open_user(&dbus_err);
    if (user_bus) {
        sd_bus_message* m = nullptr;
        int r = sd_bus_message_new_method_call(
            user_bus->raw(), &m,
            "org.freedesktop.systemd1",
            "/org/freedesktop/systemd1",
            "org.freedesktop.systemd1.Manager",
            member.c_str());
        if (r >= 0) {
            sd_bus_message_append(m, "ss", unit_name.c_str(), mode.c_str());
            sd_bus_error err = SD_BUS_ERROR_NULL;
            sd_bus_message* reply = nullptr;
            r = sd_bus_call(user_bus->raw(), m, 0, &err, &reply);
            sd_bus_message_unref(m);
            if (r >= 0) {
                if (reply) sd_bus_message_unref(reply);
                return true;
            }
            if (err.message) dbus_err = err.message;
            sd_bus_error_free(&err);
        }
    }

    // Fallback to systemctl --user
    std::string action = "start";
    if (member == "StopUnit") action = "stop";
    else if (member == "RestartUnit") action = "restart";

    std::string cmd = "systemctl --user " + action + " " + unit_name + " >/dev/null 2>&1";
    int ret = std::system(cmd.c_str());
    if (ret == 0) return true;

    if (error) {
        *error = dbus_err.empty() ? ("systemctl " + action + " failed") : dbus_err;
    }
    return false;
}

}  // namespace

bool start_unit(const std::string& unit_name, const std::string& mode, std::string* error) {
    return call_unit_mode_method("StartUnit", unit_name, mode, error);
}

bool stop_unit(const std::string& unit_name, const std::string& mode, std::string* error) {
    return call_unit_mode_method("StopUnit", unit_name, mode, error);
}

bool restart_unit(const std::string& unit_name, const std::string& mode, std::string* error) {
    return call_unit_mode_method("RestartUnit", unit_name, mode, error);
}

bool reset_failed_unit(const std::string& unit_name, std::string* error) {
    std::string dbus_err;
    auto user_bus = dbus::Bus::open_user(&dbus_err);
    if (user_bus) {
        sd_bus_message* m = nullptr;
        int r = sd_bus_message_new_method_call(
            user_bus->raw(), &m,
            "org.freedesktop.systemd1",
            "/org/freedesktop/systemd1",
            "org.freedesktop.systemd1.Manager",
            "ResetFailedUnit");
        if (r >= 0) {
            sd_bus_message_append(m, "s", unit_name.c_str());
            sd_bus_error err = SD_BUS_ERROR_NULL;
            sd_bus_message* reply = nullptr;
            r = sd_bus_call(user_bus->raw(), m, 0, &err, &reply);
            sd_bus_message_unref(m);
            if (r >= 0) {
                if (reply) sd_bus_message_unref(reply);
                return true;
            }
            if (err.message) dbus_err = err.message;
            sd_bus_error_free(&err);
        }
    }

    std::string cmd = "systemctl --user reset-failed " + unit_name + " >/dev/null 2>&1";
    int ret = std::system(cmd.c_str());
    if (ret == 0) return true;

    if (error) {
        *error = dbus_err.empty() ? "reset-failed failed" : dbus_err;
    }
    return false;
}

bool is_unit_active(const std::string& unit_name, [[maybe_unused]] std::string* error) {
    std::string dbus_err;
    auto user_bus = dbus::Bus::open_user(&dbus_err);
    if (user_bus) {
        sd_bus_message* m = nullptr;
        int r = sd_bus_message_new_method_call(
            user_bus->raw(), &m,
            "org.freedesktop.systemd1",
            "/org/freedesktop/systemd1",
            "org.freedesktop.systemd1.Manager",
            "GetUnit");
        if (r >= 0) {
            sd_bus_message_append(m, "s", unit_name.c_str());
            sd_bus_error err = SD_BUS_ERROR_NULL;
            sd_bus_message* reply = nullptr;
            r = sd_bus_call(user_bus->raw(), m, 0, &err, &reply);
            sd_bus_message_unref(m);
            if (r >= 0 && reply) {
                const char* unit_path = nullptr;
                if (sd_bus_message_read(reply, "o", &unit_path) >= 0 && unit_path) {
                    std::string state;
                    if (user_bus->get_property_string(
                            "org.freedesktop.systemd1",
                            unit_path,
                            "org.freedesktop.systemd1.Unit",
                            "ActiveState",
                            &state)) {
                        sd_bus_message_unref(reply);
                        return state == "active";
                    }
                }
                sd_bus_message_unref(reply);
            }
            sd_bus_error_free(&err);
        }
    }

    std::string cmd = "systemctl --user is-active " + unit_name + " >/dev/null 2>&1";
    return std::system(cmd.c_str()) == 0;
}

}  // namespace broseat
