#include "broseat/session.h"
#include "common/utils.h"
#include "dbus/bus.h"

#include <cstring>
#include <mutex>
#include <utility>

namespace broseat {

class SessionManagerImpl : public SessionManager {
public:
    explicit SessionManagerImpl(std::unique_ptr<dbus::Bus> bus, std::string session_path)
        : bus_(std::move(bus)), session_path_(std::move(session_path)) {
        init();
    }

    SessionInfo current_session_info() override {
        std::lock_guard<std::mutex> lock(mutex_);
        SessionInfo info;
        if (!bus_) return info;

        bus_->get_property_string("org.freedesktop.login1", session_path_,
                                  "org.freedesktop.login1.Session", "Id", &info.id);
        bus_->get_property_string("org.freedesktop.login1", session_path_,
                                  "org.freedesktop.login1.Session", "Name", &info.user);
        bus_->get_property_uint32("org.freedesktop.login1", session_path_,
                                  "org.freedesktop.login1.Session", "VTNr", &info.vt);
        bus_->get_property_bool("org.freedesktop.login1", session_path_,
                                "org.freedesktop.login1.Session", "Active", &info.active);
        bus_->get_property_bool("org.freedesktop.login1", session_path_,
                                "org.freedesktop.login1.Session", "LockedHint", &info.locked);

        std::string state_str;
        bus_->get_property_string("org.freedesktop.login1", session_path_,
                                  "org.freedesktop.login1.Session", "State", &state_str);
        info.state = parse_session_state(state_str);

        // Read Seat tuple (so)
        sd_bus_message* m = nullptr;
        int r = sd_bus_get_property(
            bus_->raw(), "org.freedesktop.login1", session_path_.c_str(),
            "org.freedesktop.login1.Session", "Seat", nullptr, &m, "(so)");
        if (r >= 0 && m) {
            const char* seat_id = nullptr;
            const char* seat_path = nullptr;
            if (sd_bus_message_read(m, "(so)", &seat_id, &seat_path) >= 0 && seat_id) {
                info.seat = seat_id;
            }
            sd_bus_message_unref(m);
        }

        locked_ = info.locked;
        return info;
    }

    bool is_locked() const override {
        return locked_;
    }

    bool lock_session() override {
        return invoke_session_method("Lock");
    }

    bool unlock_session() override {
        return invoke_session_method("Unlock");
    }

    bool switch_vt(uint32_t vt_number) override {
        if (!bus_ || vt_number == 0) return false;
        sd_bus_message* m = nullptr;
        int r = sd_bus_message_new_method_call(
            bus_->raw(), &m,
            "org.freedesktop.login1",
            session_path_.c_str(),
            "org.freedesktop.login1.Session",
            "SwitchTo");
        if (r < 0) return false;
        sd_bus_message_append(m, "u", vt_number);

        sd_bus_error err = SD_BUS_ERROR_NULL;
        sd_bus_message* reply = nullptr;
        r = sd_bus_call(bus_->raw(), m, 0, &err, &reply);
        sd_bus_message_unref(m);
        if (reply) sd_bus_message_unref(reply);
        sd_bus_error_free(&err);
        if (r >= 0) {
            notify_event(VtSwitched{static_cast<int>(vt_number)});
            return true;
        }
        return false;
    }

    EventQueue& event_queue() noexcept override {
        return event_queue_;
    }

    void set_event_callback(std::function<void(const Event&)> cb) override {
        callback_ = std::move(cb);
    }

    int poll_fd() const noexcept override {
        return bus_ ? bus_->get_fd() : -1;
    }

    int dispatch(int timeout_ms) override {
        if (!bus_) return -1;
        if (timeout_ms > 0) {
            bus_->wait(static_cast<uint64_t>(timeout_ms) * 1000ULL);
        }
        return bus_->process();
    }

private:
    void init() {
        if (!bus_) return;

        bool locked = false;
        if (bus_->get_property_bool("org.freedesktop.login1", session_path_,
                                    "org.freedesktop.login1.Session", "LockedHint", &locked)) {
            locked_ = locked;
        }

        std::string rule_lock = "type='signal',sender='org.freedesktop.login1',path='" +
            session_path_ + "',interface='org.freedesktop.login1.Session',member='Lock'";
        auto s1 = bus_->add_match(rule_lock, [this](sd_bus_message* /*m*/) {
            locked_ = true;
            notify_event(SessionLockedChanged{true});
        });
        if (s1.is_valid()) match_slots_.push_back(std::move(s1));

        std::string rule_unlock = "type='signal',sender='org.freedesktop.login1',path='" +
            session_path_ + "',interface='org.freedesktop.login1.Session',member='Unlock'";
        auto s2 = bus_->add_match(rule_unlock, [this](sd_bus_message* /*m*/) {
            locked_ = false;
            notify_event(SessionLockedChanged{false});
        });
        if (s2.is_valid()) match_slots_.push_back(std::move(s2));

        std::string rule_props = "type='signal',sender='org.freedesktop.login1',path='" +
            session_path_ + "',interface='org.freedesktop.DBus.Properties',member='PropertiesChanged'";
        auto s3 = bus_->add_match(rule_props, [this](sd_bus_message* /*m*/) {
            bool locked = false;
            if (bus_->get_property_bool("org.freedesktop.login1", session_path_,
                                        "org.freedesktop.login1.Session", "LockedHint", &locked)) {
                if (locked != locked_) {
                    locked_ = locked;
                    notify_event(SessionLockedChanged{locked});
                }
            }
        });
        if (s3.is_valid()) match_slots_.push_back(std::move(s3));
    }

    bool invoke_session_method(const char* member) {
        if (!bus_) return false;
        sd_bus_message* m = nullptr;
        int r = sd_bus_message_new_method_call(
            bus_->raw(), &m,
            "org.freedesktop.login1",
            session_path_.c_str(),
            "org.freedesktop.login1.Session",
            member);
        if (r < 0) return false;

        sd_bus_error err = SD_BUS_ERROR_NULL;
        sd_bus_message* reply = nullptr;
        r = sd_bus_call(bus_->raw(), m, 0, &err, &reply);
        sd_bus_message_unref(m);
        if (reply) sd_bus_message_unref(reply);
        sd_bus_error_free(&err);
        return r >= 0;
    }

    void notify_event(const Event& event) {
        event_queue_.push(event);
        if (callback_) {
            callback_(event);
        }
    }

    std::unique_ptr<dbus::Bus> bus_;
    std::string session_path_;
    bool locked_ = false;
    std::vector<dbus::Slot> match_slots_;
    EventQueue event_queue_;
    std::function<void(const Event&)> callback_;
    mutable std::mutex mutex_;
};

std::unique_ptr<SessionManager> SessionManager::create(std::string* error) {
    auto bus = dbus::Bus::open_system(error);
    if (!bus) return nullptr;

    std::string session_path;
    std::string xdg_session = utils::get_env("XDG_SESSION_ID");
    if (!xdg_session.empty()) {
        sd_bus_message* m = nullptr;
        sd_bus_message_new_method_call(
            bus->raw(), &m,
            "org.freedesktop.login1",
            "/org/freedesktop/login1",
            "org.freedesktop.login1.Manager",
            "GetSession");
        sd_bus_message_append(m, "s", xdg_session.c_str());
        sd_bus_error err = SD_BUS_ERROR_NULL;
        sd_bus_message* reply = nullptr;
        int r = sd_bus_call(bus->raw(), m, 0, &err, &reply);
        sd_bus_message_unref(m);
        if (r >= 0 && reply) {
            const char* path = nullptr;
            if (sd_bus_message_read(reply, "o", &path) >= 0 && path) {
                session_path = path;
            }
            sd_bus_message_unref(reply);
        }
        sd_bus_error_free(&err);
    }

    if (session_path.empty()) {
        sd_bus_message* m = nullptr;
        sd_bus_message_new_method_call(
            bus->raw(), &m,
            "org.freedesktop.login1",
            "/org/freedesktop/login1",
            "org.freedesktop.login1.Manager",
            "GetSessionByPID");
        sd_bus_message_append(m, "u", 0);
        sd_bus_error err = SD_BUS_ERROR_NULL;
        sd_bus_message* reply = nullptr;
        int r = sd_bus_call(bus->raw(), m, 0, &err, &reply);
        sd_bus_message_unref(m);
        if (r >= 0 && reply) {
            const char* path = nullptr;
            if (sd_bus_message_read(reply, "o", &path) >= 0 && path) {
                session_path = path;
            }
            sd_bus_message_unref(reply);
        }
        sd_bus_error_free(&err);
    }

    if (session_path.empty()) {
        session_path = "/org/freedesktop/login1/session/auto";
    }

    return std::make_unique<SessionManagerImpl>(std::move(bus), session_path);
}

}  // namespace broseat
