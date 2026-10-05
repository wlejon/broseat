#pragma once

#include "broseat/event_queue.h"
#include "broseat/seat.h"

#include <map>
#include <mutex>
#include <string>

namespace broseat {

class MockSeatBackend : public Seat {
public:
    explicit MockSeatBackend(std::string seat_name = "seat0");
    ~MockSeatBackend() override;

    SeatBackendType backend_type() const noexcept override { return SeatBackendType::Mock; }
    const std::string& seat_name() const noexcept override { return seat_name_; }
    bool is_active() const noexcept override { return is_active_; }

    std::unique_ptr<SeatDevice> open_device(const std::string& path, std::string* error = nullptr) override;
    bool close_device(int device_id) override;

    int switch_vt(int vt_number) override;

    int poll_fd() const noexcept override { return event_fd_; }
    int dispatch(int timeout_ms = 0) override;

    EventQueue& event_queue() noexcept override { return event_queue_; }
    void set_event_callback(std::function<void(const Event&)> cb) override { callback_ = std::move(cb); }

    void acknowledge_device_pause(int device_id) override;

    // Test simulation helpers
    void set_active(bool active);
    void simulate_pause_device(int device_id, bool force);
    void simulate_resume_device(int device_id, int fd);
    int current_vt() const noexcept { return current_vt_; }
    bool was_pause_acknowledged(int device_id) const;

private:
    void notify_event(const Event& event);

    std::string seat_name_;
    bool is_active_ = true;
    int current_vt_ = 1;
    int next_device_id_ = 1;
    int event_fd_ = -1;

    struct DeviceRecord {
        int id = -1;
        int fd = -1;
        std::string path;
        DeviceType type = DeviceType::Other;
    };

    mutable std::mutex mutex_;
    std::map<int, DeviceRecord> devices_;
    std::map<int, bool> acknowledged_pauses_;
    EventQueue event_queue_;
    std::function<void(const Event&)> callback_;
};

}  // namespace broseat
