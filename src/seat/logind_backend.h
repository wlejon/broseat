#pragma once

#include "broseat/event_queue.h"
#include "broseat/seat.h"
#include "dbus/bus.h"

#include <map>
#include <memory>
#include <mutex>
#include <string>

namespace broseat {

class LogindBackend : public Seat {
public:
    static std::unique_ptr<LogindBackend> open(const SeatConfig& config, std::string* error = nullptr);
    ~LogindBackend() override;

    SeatBackendType backend_type() const noexcept override { return SeatBackendType::Logind; }
    const std::string& seat_name() const noexcept override { return seat_name_; }
    bool is_active() const noexcept override { return is_active_; }

    std::unique_ptr<SeatDevice> open_device(const std::string& path, std::string* error = nullptr) override;
    bool close_device(int device_id) override;

    int switch_vt(int vt_number) override;

    int poll_fd() const noexcept override;
    int dispatch(int timeout_ms = 0) override;

    EventQueue& event_queue() noexcept override { return event_queue_; }
    void set_event_callback(std::function<void(const Event&)> cb) override { callback_ = std::move(cb); }

    void acknowledge_device_pause(int device_id) override;

private:
    struct DeviceKey {
        uint32_t major = 0;
        uint32_t minor = 0;
        bool operator<(const DeviceKey& o) const {
            if (major != o.major) return major < o.major;
            return minor < o.minor;
        }
    };

    struct DeviceRecord {
        int id = -1;
        DeviceKey key;
        std::string path;
        DeviceType type = DeviceType::Other;
    };

    explicit LogindBackend(std::unique_ptr<dbus::Bus> bus, std::string session_path, std::string seat_name);

    bool take_control(std::string* error = nullptr);
    void release_control();
    void setup_matches();
    void on_pause_device(uint32_t major, uint32_t minor, const std::string& type);
    void on_resume_device(uint32_t major, uint32_t minor, int fd);
    void notify_event(const Event& event);

    std::unique_ptr<dbus::Bus> bus_;
    std::string session_path_;
    std::string seat_name_;
    bool is_active_ = false;
    bool has_control_ = false;

    int next_device_id_ = 1;
    std::map<int, DeviceRecord> devices_by_id_;
    std::map<DeviceKey, int> id_by_key_;

    std::vector<dbus::Slot> match_slots_;
    EventQueue event_queue_;
    std::function<void(const Event&)> callback_;
    mutable std::mutex mutex_;
};

}  // namespace broseat
