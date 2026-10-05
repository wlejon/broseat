#include "broseat/session.h"
#include "common/utils.h"
#include "dbus/bus.h"

#include <cstdlib>
#include <sstream>
#include <unistd.h>

namespace broseat {

namespace {

bool fallback_import_environment(const std::vector<std::string>& var_names, std::string* error) {
    if (var_names.empty()) return true;

    std::ostringstream cmd;
    cmd << "systemctl --user import-environment";
    for (const auto& name : var_names) {
        cmd << " " << name;
    }

    int ret = std::system(cmd.str().c_str());
    if (ret != 0) {
        if (error) *error = "systemctl --user import-environment failed with code " + std::to_string(ret);
        return false;
    }
    return true;
}

}  // namespace

bool export_environment(
    const std::vector<std::pair<std::string, std::string>>& vars,
    std::string* error) {
    if (vars.empty()) return true;

    std::vector<std::string> assignments;
    std::vector<std::string> var_names;
    assignments.reserve(vars.size());
    var_names.reserve(vars.size());

    for (const auto& [name, val] : vars) {
        if (name.empty()) continue;
        utils::set_env(name, val);
        assignments.push_back(name + "=" + val);
        var_names.push_back(name);
    }

    std::string dbus_err;
    auto user_bus = dbus::Bus::open_user(&dbus_err);
    if (user_bus) {
        if (user_bus->call_string_array(
                "org.freedesktop.systemd1",
                "/org/freedesktop/systemd1",
                "org.freedesktop.systemd1.Manager",
                "SetEnvironment",
                assignments,
                &dbus_err)) {
            return true;
        }
    }

    // Fallback to systemctl --user import-environment
    return fallback_import_environment(var_names, error ? error : nullptr);
}

bool export_environment(
    const std::vector<std::string>& var_names,
    std::string* error) {
    if (var_names.empty()) return true;

    std::vector<std::pair<std::string, std::string>> vars;
    vars.reserve(var_names.size());
    for (const auto& name : var_names) {
        const char* val = std::getenv(name.c_str());
        vars.emplace_back(name, val ? val : "");
    }
    return export_environment(vars, error);
}

bool unset_environment(
    const std::vector<std::string>& var_names,
    std::string* error) {
    if (var_names.empty()) return true;

    for (const auto& name : var_names) {
        ::unsetenv(name.c_str());
    }

    std::string dbus_err;
    auto user_bus = dbus::Bus::open_user(&dbus_err);
    if (user_bus) {
        if (user_bus->call_string_array(
                "org.freedesktop.systemd1",
                "/org/freedesktop/systemd1",
                "org.freedesktop.systemd1.Manager",
                "UnsetEnvironment",
                var_names,
                &dbus_err)) {
            return true;
        }
    }

    if (error) {
        *error = dbus_err.empty() ? "failed to unset environment" : dbus_err;
    }
    return false;
}

}  // namespace broseat
