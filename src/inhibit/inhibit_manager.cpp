#include "broseat/inhibit.h"
#include "dbus/bus.h"

#include <fcntl.h>
#include <unistd.h>

#include <cstring>
#include <mutex>
#include <utility>

namespace broseat {

InhibitorLock::InhibitorLock(int fd, std::string what, std::string who, std::string why, InhibitMode mode)
    : fd_(fd), what_(std::move(what)), who_(std::move(who)), why_(std::move(why)), mode_(mode) {}

InhibitorLock::~InhibitorLock() {
    release();
}

InhibitorLock::InhibitorLock(InhibitorLock&& other) noexcept
    : fd_(other.fd_),
      what_(std::move(other.what_)),
      who_(std::move(other.who_)),
      why_(std::move(other.why_)),
      mode_(other.mode_) {
    other.fd_ = -1;
}

InhibitorLock& InhibitorLock::operator=(InhibitorLock&& other) noexcept {
    if (this != &other) {
        release();
        fd_ = other.fd_;
        what_ = std::move(other.what_);
        who_ = std::move(other.who_);
        why_ = std::move(other.why_);
        mode_ = other.mode_;
        other.fd_ = -1;
    }
    return *this;
}

void InhibitorLock::release() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

class InhibitManagerImpl : public InhibitManager {
public:
    explicit InhibitManagerImpl(std::unique_ptr<dbus::Bus> bus) : bus_(std::move(bus)) {}

    std::unique_ptr<InhibitorLock> inhibit(
        const std::string& what,
        const std::string& who,
        const std::string& why,
        InhibitMode mode,
        std::string* error) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!bus_) {
            if (error) *error = "bus not available";
            return nullptr;
        }

        sd_bus_message* m = nullptr;
        int r = sd_bus_message_new_method_call(
            bus_->raw(), &m,
            "org.freedesktop.login1",
            "/org/freedesktop/login1",
            "org.freedesktop.login1.Manager",
            "Inhibit");
        if (r < 0) {
            if (error) *error = strerror(-r);
            return nullptr;
        }

        std::string mode_str = std::string(inhibit_mode_name(mode));
        sd_bus_message_append(m, "ssss", what.c_str(), who.c_str(), why.c_str(), mode_str.c_str());

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
        r = sd_bus_message_read(reply, "h", &received_fd);
        if (r < 0 || received_fd < 0) {
            if (error) *error = "failed to read inhibitor fd";
            sd_bus_message_unref(reply);
            return nullptr;
        }

        int owned_fd = fcntl(received_fd, F_DUPFD_CLOEXEC, 0);
        sd_bus_message_unref(reply);

        if (owned_fd < 0) {
            if (error) *error = strerror(errno);
            return nullptr;
        }

        InhibitorInfo info{
            .what = what,
            .who = who,
            .why = why,
            .mode = mode_str,
            .uid = static_cast<uint32_t>(getuid()),
            .pid = static_cast<uint32_t>(getpid())
        };
        notify_event(InhibitorAdded{info});

        return std::make_unique<InhibitorLock>(owned_fd, what, who, why, mode);
    }

    std::vector<InhibitorInfo> list_inhibitors(std::string* error) override {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<InhibitorInfo> results;
        if (!bus_) return results;

        sd_bus_message* m = nullptr;
        int r = sd_bus_message_new_method_call(
            bus_->raw(), &m,
            "org.freedesktop.login1",
            "/org/freedesktop/login1",
            "org.freedesktop.login1.Manager",
            "ListInhibitors");
        if (r < 0) {
            if (error) *error = strerror(-r);
            return results;
        }

        sd_bus_error err = SD_BUS_ERROR_NULL;
        sd_bus_message* reply = nullptr;
        r = sd_bus_call(bus_->raw(), m, 0, &err, &reply);
        sd_bus_message_unref(m);

        if (r < 0) {
            if (error) *error = err.message ? err.message : strerror(-r);
            sd_bus_error_free(&err);
            return results;
        }

        r = sd_bus_message_enter_container(reply, 'a', "(ssssuu)");
        if (r >= 0) {
            while ((r = sd_bus_message_enter_container(reply, 'r', "ssssuu")) > 0) {
                const char* what = nullptr;
                const char* who = nullptr;
                const char* why = nullptr;
                const char* mode = nullptr;
                uint32_t uid = 0;
                uint32_t pid = 0;

                if (sd_bus_message_read(reply, "ssssuu", &what, &who, &why, &mode, &uid, &pid) >= 0) {
                    results.push_back(InhibitorInfo{
                        .what = what ? what : "",
                        .who = who ? who : "",
                        .why = why ? why : "",
                        .mode = mode ? mode : "",
                        .uid = uid,
                        .pid = pid
                    });
                }
                sd_bus_message_exit_container(reply);
            }
            sd_bus_message_exit_container(reply);
        }

        sd_bus_message_unref(reply);
        return results;
    }

    bool is_inhibited(const std::string& what) override {
        auto list = list_inhibitors(nullptr);
        for (const auto& inh : list) {
            if (inh.what.find(what) != std::string::npos) {
                return true;
            }
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
    void notify_event(const Event& event) {
        event_queue_.push(event);
        if (callback_) {
            callback_(event);
        }
    }

    std::unique_ptr<dbus::Bus> bus_;
    EventQueue event_queue_;
    std::function<void(const Event&)> callback_;
    mutable std::mutex mutex_;
};

std::unique_ptr<InhibitManager> InhibitManager::create(std::string* error) {
    auto bus = dbus::Bus::open_system(error);
    if (!bus) return nullptr;
    return std::make_unique<InhibitManagerImpl>(std::move(bus));
}

}  // namespace broseat
