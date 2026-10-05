#include "dbus/bus.h"

#include <cassert>
#include <iostream>

int main() {
    using namespace broseat::dbus;

    std::string err;
    auto user_bus = Bus::open_user(&err);
    if (!user_bus) {
        std::cout << "User bus open failed: " << err << "\n";
    } else {
        assert(user_bus->is_valid());
        assert(user_bus->get_fd() >= 0);
        assert(user_bus->raw() != nullptr);

        // Move semantics test
        Bus moved_bus = std::move(*user_bus);
        assert(moved_bus.is_valid());
        assert(moved_bus.get_fd() >= 0);
        assert(!user_bus->is_valid());

        // Test property read
        std::string unit_path;
        bool ok = moved_bus.get_property_string(
            "org.freedesktop.systemd1",
            "/org/freedesktop/systemd1",
            "org.freedesktop.systemd1.Manager",
            "Progress",
            &unit_path);
        if (ok) {
            std::cout << "systemd1 Manager.Progress: " << unit_path << "\n";
        }

        // Test match registration
        auto slot = moved_bus.add_match(
            "type='signal',interface='org.freedesktop.DBus',member='NameOwnerChanged'",
            [](sd_bus_message* /*m*/) {});
        assert(slot.is_valid());
        slot.reset();
        assert(!slot.is_valid());
    }

    auto sys_bus = Bus::open_system(&err);
    if (!sys_bus) {
        std::cout << "System bus open failed: " << err << "\n";
    } else {
        assert(sys_bus->is_valid());
        assert(sys_bus->get_fd() >= 0);
    }

    std::cout << "test_dbus PASSED\n";
    return 0;
}
