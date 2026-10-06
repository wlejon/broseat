#include "seat/logind_backend.h"
#include "common/utils.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstring>
#include <utility>

namespace broseat {

LogindBackend::LogindBackend(
    std::unique_ptr<dbus::Bus> bus,
    std::string session_path,
    std::string seat_name)
    : bus_(std::move(bus)),
      session_path_(std::move(session_path)),
      seat_name_(std::move(seat_name)) {}

LogindBackend::~LogindBackend() {
    release_control();
}

std::unique_ptr<LogindBackend> LogindBackend::open(const SeatConfig& config, std::string* error) {
    auto bus = dbus::Bus::open_system(error);
    if (!bus) return nullptr;

    std::string session_path;
    if (!config.session_id.empty()) {
        sd_bus_message* m = nullptr;
        sd_bus_message_new_method_call(
            bus->raw(), &m,
            "org.freedesktop.login1",
            "/org/freedesktop/login1",
            "org.freedesktop.login1.Manager",
            "GetSession");
        sd_bus_message_append(m, "s", config.session_id.c_str());
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

    // The seat is the session's own (Session.Seat); a session that is not on
    // a seat (ssh, a service) has no devices to take, so there is nothing to
    // control.
    std::string session_seat;
    {
        sd_bus_message* m = nullptr;
        int r = sd_bus_get_property(bus->raw(), "org.freedesktop.login1", session_path.c_str(),
                                    "org.freedesktop.login1.Session", "Seat", nullptr, &m, "(so)");
        if (r >= 0 && m) {
            const char* seat_id = nullptr;
            const char* seat_path = nullptr;
            if (sd_bus_message_read(m, "(so)", &seat_id, &seat_path) >= 0 && seat_id) {
                session_seat = seat_id;
            }
            sd_bus_message_unref(m);
        }
    }
    if (session_seat.empty()) {
        if (error) *error = "logind session " + session_path + " is not attached to a seat";
        return nullptr;
    }
    std::string seat_name = config.seat_name.empty() ? session_seat : config.seat_name;
    if (seat_name != session_seat) {
        if (error) *error = "logind session " + session_path + " is on " + session_seat + ", not " + seat_name;
        return nullptr;
    }

    auto backend = std::unique_ptr<LogindBackend>(
        new LogindBackend(std::move(bus), session_path, seat_name));

    if (!backend->take_control(error)) {
        return nullptr;
    }

    backend->bus_->get_property_bool(
        "org.freedesktop.login1",
        backend->session_path_,
        "org.freedesktop.login1.Session",
        "Active",
        &backend->is_active_);

    backend->setup_matches();
    return backend;
}

bool LogindBackend::take_control(std::string* error) {
    if (!bus_) return false;
    sd_bus_message* m = nullptr;
    int r = sd_bus_message_new_method_call(
        bus_->raw(), &m,
        "org.freedesktop.login1",
        session_path_.c_str(),
        "org.freedesktop.login1.Session",
        "TakeControl");
    if (r < 0) {
        if (error) *error = strerror(-r);
        return false;
    }
    sd_bus_message_append(m, "b", 0);

    sd_bus_error err = SD_BUS_ERROR_NULL;
    sd_bus_message* reply = nullptr;
    r = sd_bus_call(bus_->raw(), m, 0, &err, &reply);
    sd_bus_message_unref(m);
    if (r < 0) {
        if (error) *error = err.message ? err.message : strerror(-r);
        sd_bus_error_free(&err);
        return false;
    }
    if (reply) sd_bus_message_unref(reply);
    has_control_ = true;
    return true;
}

void LogindBackend::release_control() {
    if (has_control_ && bus_) {
        sd_bus_message* m = nullptr;
        int r = sd_bus_message_new_method_call(
            bus_->raw(), &m,
            "org.freedesktop.login1",
            session_path_.c_str(),
            "org.freedesktop.login1.Session",
            "ReleaseControl");
        if (r >= 0) {
            sd_bus_call(bus_->raw(), m, 0, nullptr, nullptr);
            sd_bus_message_unref(m);
        }
        has_control_ = false;
    }
}

void LogindBackend::setup_matches() {
    if (!bus_) return;

    std::string rule_pause = "type='signal',sender='org.freedesktop.login1',path='" +
        session_path_ + "',interface='org.freedesktop.login1.Session',member='PauseDevice'";
    auto slot1 = bus_->add_match(rule_pause, [this](sd_bus_message* m) {
        uint32_t maj = 0;
        uint32_t min = 0;
        const char* type = nullptr;
        if (sd_bus_message_read(m, "uus", &maj, &min, &type) >= 0) {
            on_pause_device(maj, min, type ? type : "");
        }
    });
    if (slot1.is_valid()) match_slots_.push_back(std::move(slot1));

    std::string rule_resume = "type='signal',sender='org.freedesktop.login1',path='" +
        session_path_ + "',interface='org.freedesktop.login1.Session',member='ResumeDevice'";
    auto slot2 = bus_->add_match(rule_resume, [this](sd_bus_message* m) {
        uint32_t maj = 0;
        uint32_t min = 0;
        int fd = -1;
        if (sd_bus_message_read(m, "uuh", &maj, &min, &fd) >= 0) {
            on_resume_device(maj, min, fd);
        }
    });
    if (slot2.is_valid()) match_slots_.push_back(std::move(slot2));

    std::string rule_props = "type='signal',sender='org.freedesktop.login1',path='" +
        session_path_ + "',interface='org.freedesktop.DBus.Properties',member='PropertiesChanged'";
    auto slot3 = bus_->add_match(rule_props, [this](sd_bus_message* /*m*/) {
        bool active = false;
        if (bus_->get_property_bool("org.freedesktop.login1", session_path_,
                                    "org.freedesktop.login1.Session", "Active", &active)) {
            if (active != is_active_) {
                is_active_ = active;
                notify_event(SeatActiveChanged{active});
            }
        }
    });
    if (slot3.is_valid()) match_slots_.push_back(std::move(slot3));
}

void LogindBackend::on_pause_device(uint32_t major, uint32_t minor, const std::string& type) {
    std::lock_guard<std::mutex> lock(mutex_);
    DeviceKey key{major, minor};
    auto it = id_by_key_.find(key);
    if (it != id_by_key_.end()) {
        int dev_id = it->second;
        bool force = (type == "force" || type == "gone");
        notify_event(SeatDevicePaused{dev_id, force});
    }
}

void LogindBackend::on_resume_device(uint32_t major, uint32_t minor, int fd) {
    std::lock_guard<std::mutex> lock(mutex_);
    DeviceKey key{major, minor};
    auto it = id_by_key_.find(key);
    if (it != id_by_key_.end()) {
        int dev_id = it->second;
        notify_event(SeatDeviceResumed{dev_id, fd});
    }
}

std::unique_ptr<SeatDevice> LogindBackend::open_device(const std::string& path, std::string* error) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!bus_) {
        if (error) *error = "bus not available";
        return nullptr;
    }

    struct stat st {};
    if (stat(path.c_str(), &st) != 0) {
        if (error) *error = strerror(errno);
        return nullptr;
    }
    if (!S_ISCHR(st.st_mode)) {
        if (error) *error = "not a character device";
        return nullptr;
    }

    uint32_t maj = major(st.st_rdev);
    uint32_t min = minor(st.st_rdev);

    sd_bus_message* m = nullptr;
    int r = sd_bus_message_new_method_call(
        bus_->raw(), &m,
        "org.freedesktop.login1",
        session_path_.c_str(),
        "org.freedesktop.login1.Session",
        "TakeDevice");
    if (r < 0) {
        if (error) *error = strerror(-r);
        return nullptr;
    }
    sd_bus_message_append(m, "uu", maj, min);

    sd_bus_error err = SD_BUS_ERROR_NULL;
    sd_bus_message* reply = nullptr;
    r = sd_bus_call(bus_->raw(), m, 0, &err, &reply);
    sd_bus_message_unref(m);
    if (r < 0) {
        if (error) *error = err.message ? err.message : strerror(-r);
        sd_bus_error_free(&err);
        return nullptr;
    }

    int received_fd = -1;
    int inactive = 0;
    r = sd_bus_message_read(reply, "hb", &received_fd, &inactive);
    if (r < 0) {
        if (error) *error = strerror(-r);
        sd_bus_message_unref(reply);
        return nullptr;
    }

    int owned_fd = fcntl(received_fd, F_DUPFD_CLOEXEC, 0);
    sd_bus_message_unref(reply);

    if (owned_fd < 0) {
        if (error) *error = strerror(errno);
        return nullptr;
    }

    int id = next_device_id_++;
    DeviceKey key{maj, min};
    DeviceType type = detect_device_type(path);
    devices_by_id_[id] = DeviceRecord{id, key, path, type};
    id_by_key_[key] = id;

    return std::make_unique<SeatDevice>(this, id, owned_fd, path, type);
}

bool LogindBackend::close_device(int device_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = devices_by_id_.find(device_id);
    if (it == devices_by_id_.end()) return false;

    DeviceKey key = it->second.key;
    devices_by_id_.erase(it);
    id_by_key_.erase(key);

    if (bus_) {
        sd_bus_message* m = nullptr;
        int r = sd_bus_message_new_method_call(
            bus_->raw(), &m,
            "org.freedesktop.login1",
            session_path_.c_str(),
            "org.freedesktop.login1.Session",
            "ReleaseDevice");
        if (r >= 0) {
            sd_bus_message_append(m, "uu", key.major, key.minor);
            sd_bus_call(bus_->raw(), m, 0, nullptr, nullptr);
            sd_bus_message_unref(m);
        }
    }
    return true;
}

int LogindBackend::switch_vt(int vt_number) {
    if (!bus_ || vt_number <= 0) return -1;
    sd_bus_message* m = nullptr;
    int r = sd_bus_message_new_method_call(
        bus_->raw(), &m,
        "org.freedesktop.login1",
        session_path_.c_str(),
        "org.freedesktop.login1.Session",
        "SwitchTo");
    if (r < 0) return -1;
    sd_bus_message_append(m, "u", static_cast<uint32_t>(vt_number));

    sd_bus_error err = SD_BUS_ERROR_NULL;
    sd_bus_message* reply = nullptr;
    r = sd_bus_call(bus_->raw(), m, 0, &err, &reply);
    sd_bus_message_unref(m);
    if (reply) sd_bus_message_unref(reply);
    sd_bus_error_free(&err);

    if (r >= 0) {
        notify_event(VtSwitched{vt_number});
        return 0;
    }
    return -1;
}

int LogindBackend::poll_fd() const noexcept {
    return bus_ ? bus_->get_fd() : -1;
}

int LogindBackend::dispatch(int timeout_ms) {
    if (!bus_) return -1;
    if (timeout_ms > 0) {
        bus_->wait(static_cast<uint64_t>(timeout_ms) * 1000ULL);
    }
    return bus_->process();
}

void LogindBackend::acknowledge_device_pause(int device_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = devices_by_id_.find(device_id);
    if (it == devices_by_id_.end() || !bus_) return;

    DeviceKey key = it->second.key;
    sd_bus_message* m = nullptr;
    int r = sd_bus_message_new_method_call(
        bus_->raw(), &m,
        "org.freedesktop.login1",
        session_path_.c_str(),
        "org.freedesktop.login1.Session",
        "PauseDeviceComplete");
    if (r >= 0) {
        sd_bus_message_append(m, "uu", key.major, key.minor);
        sd_bus_call(bus_->raw(), m, 0, nullptr, nullptr);
        sd_bus_message_unref(m);
    }
}

void LogindBackend::notify_event(const Event& event) {
    event_queue_.push(event);
    if (callback_) {
        callback_(event);
    }
}

}  // namespace broseat
