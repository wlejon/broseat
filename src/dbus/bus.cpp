#include "dbus/bus.h"

#include <cstring>
#include <utility>

namespace broseat::dbus {

Slot::Slot(sd_bus_slot* slot, std::shared_ptr<void> userdata)
    : slot_(slot), userdata_(std::move(userdata)) {}

Slot::~Slot() {
    reset();
}

Slot::Slot(Slot&& other) noexcept
    : slot_(other.slot_), userdata_(std::move(other.userdata_)) {
    other.slot_ = nullptr;
}

Slot& Slot::operator=(Slot&& other) noexcept {
    if (this != &other) {
        reset();
        slot_ = other.slot_;
        userdata_ = std::move(other.userdata_);
        other.slot_ = nullptr;
    }
    return *this;
}

void Slot::reset() {
    if (slot_) {
        sd_bus_slot_unref(slot_);
        slot_ = nullptr;
    }
    userdata_.reset();
}

Bus::Bus(sd_bus* bus) : bus_(bus) {}

Bus::~Bus() {
    if (bus_) {
        sd_bus_flush_close_unref(bus_);
        bus_ = nullptr;
    }
}

Bus::Bus(Bus&& other) noexcept : bus_(other.bus_) {
    other.bus_ = nullptr;
}

Bus& Bus::operator=(Bus&& other) noexcept {
    if (this != &other) {
        if (bus_) {
            sd_bus_flush_close_unref(bus_);
        }
        bus_ = other.bus_;
        other.bus_ = nullptr;
    }
    return *this;
}

std::unique_ptr<Bus> Bus::open_system(std::string* error) {
    sd_bus* bus = nullptr;
    int r = sd_bus_open_system(&bus);
    if (r < 0) {
        if (error) {
            *error = strerror(-r);
        }
        return nullptr;
    }
    return std::make_unique<Bus>(bus);
}

std::unique_ptr<Bus> Bus::open_user(std::string* error) {
    sd_bus* bus = nullptr;
    int r = sd_bus_open_user(&bus);
    if (r < 0) {
        if (error) {
            *error = strerror(-r);
        }
        return nullptr;
    }
    return std::make_unique<Bus>(bus);
}

std::unique_ptr<Bus> Bus::open_address(const std::string& address, std::string* error) {
    sd_bus* bus = nullptr;
    int r = sd_bus_new(&bus);
    if (r < 0) {
        if (error) *error = strerror(-r);
        return nullptr;
    }
    r = sd_bus_set_address(bus, address.c_str());
    if (r < 0) {
        if (error) *error = strerror(-r);
        sd_bus_unref(bus);
        return nullptr;
    }
    // A message bus, not a peer-to-peer connection: send Hello so the daemon
    // gives us a unique name and AddMatch rules are registered with it.
    r = sd_bus_set_bus_client(bus, 1);
    if (r < 0) {
        if (error) *error = strerror(-r);
        sd_bus_unref(bus);
        return nullptr;
    }
    r = sd_bus_start(bus);
    if (r < 0) {
        if (error) *error = strerror(-r);
        sd_bus_unref(bus);
        return nullptr;
    }
    return std::make_unique<Bus>(bus);
}

int Bus::get_fd() const noexcept {
    return bus_ ? sd_bus_get_fd(bus_) : -1;
}

int Bus::process() {
    if (!bus_) return -1;
    return sd_bus_process(bus_, nullptr);
}

int Bus::wait(uint64_t timeout_usec) {
    if (!bus_) return -1;
    return sd_bus_wait(bus_, timeout_usec);
}

namespace {
struct MatchContext {
    SignalCallback callback;
};

int handle_match_signal(sd_bus_message* m, void* userdata, sd_bus_error* /*ret_error*/) {
    auto* ctx = static_cast<MatchContext*>(userdata);
    if (ctx && ctx->callback) {
        ctx->callback(m);
    }
    return 0;
}
}  // namespace

Slot Bus::add_match(const std::string& match_rule, SignalCallback callback, std::string* error) {
    if (!bus_) {
        if (error) *error = "bus not initialized";
        return Slot();
    }

    auto ctx = std::make_shared<MatchContext>(MatchContext{std::move(callback)});
    sd_bus_slot* slot = nullptr;
    int r = sd_bus_add_match(bus_, &slot, match_rule.c_str(), handle_match_signal, ctx.get());
    if (r < 0) {
        if (error) *error = strerror(-r);
        return Slot();
    }
    return Slot(slot, ctx);
}

bool Bus::get_property_bool(
    const std::string& destination,
    const std::string& path,
    const std::string& interface,
    const std::string& property,
    bool* out,
    std::string* error) {
    if (!bus_ || !out) return false;
    sd_bus_error err = SD_BUS_ERROR_NULL;
    int val = 0;
    int r = sd_bus_get_property_trivial(
        bus_, destination.c_str(), path.c_str(), interface.c_str(), property.c_str(), &err, 'b', &val);
    if (r < 0) {
        if (error) *error = err.message ? err.message : strerror(-r);
        sd_bus_error_free(&err);
        return false;
    }
    *out = (val != 0);
    return true;
}

bool Bus::get_property_string(
    const std::string& destination,
    const std::string& path,
    const std::string& interface,
    const std::string& property,
    std::string* out,
    std::string* error) {
    if (!bus_ || !out) return false;
    sd_bus_error err = SD_BUS_ERROR_NULL;
    char* str = nullptr;
    int r = sd_bus_get_property_string(
        bus_, destination.c_str(), path.c_str(), interface.c_str(), property.c_str(), &err, &str);
    if (r < 0) {
        if (error) *error = err.message ? err.message : strerror(-r);
        sd_bus_error_free(&err);
        return false;
    }
    *out = str ? str : "";
    free(str);
    return true;
}

bool Bus::get_property_uint32(
    const std::string& destination,
    const std::string& path,
    const std::string& interface,
    const std::string& property,
    uint32_t* out,
    std::string* error) {
    if (!bus_ || !out) return false;
    sd_bus_error err = SD_BUS_ERROR_NULL;
    uint32_t val = 0;
    int r = sd_bus_get_property_trivial(
        bus_, destination.c_str(), path.c_str(), interface.c_str(), property.c_str(), &err, 'u', &val);
    if (r < 0) {
        if (error) *error = err.message ? err.message : strerror(-r);
        sd_bus_error_free(&err);
        return false;
    }
    *out = val;
    return true;
}

bool Bus::call_string_array(
    const std::string& destination,
    const std::string& path,
    const std::string& interface,
    const std::string& member,
    const std::vector<std::string>& strings,
    std::string* error) {
    if (!bus_) return false;

    sd_bus_message* m = nullptr;
    int r = sd_bus_message_new_method_call(
        bus_, &m, destination.c_str(), path.c_str(), interface.c_str(), member.c_str());
    if (r < 0) {
        if (error) *error = strerror(-r);
        return false;
    }

    r = sd_bus_message_open_container(m, 'a', "s");
    if (r >= 0) {
        for (const auto& s : strings) {
            r = sd_bus_message_append_basic(m, 's', s.c_str());
            if (r < 0) break;
        }
        if (r >= 0) {
            r = sd_bus_message_close_container(m);
        }
    }

    if (r < 0) {
        if (error) *error = strerror(-r);
        sd_bus_message_unref(m);
        return false;
    }

    sd_bus_error err = SD_BUS_ERROR_NULL;
    sd_bus_message* reply = nullptr;
    r = sd_bus_call(bus_, m, 0, &err, &reply);
    sd_bus_message_unref(m);

    if (r < 0) {
        if (error) *error = err.message ? err.message : strerror(-r);
        sd_bus_error_free(&err);
        return false;
    }

    if (reply) {
        sd_bus_message_unref(reply);
    }
    return true;
}

}  // namespace broseat::dbus
