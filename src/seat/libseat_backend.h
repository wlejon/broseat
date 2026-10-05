#pragma once

#include "broseat/event_queue.h"
#include "broseat/seat.h"

extern "C" {
#include <libseat.h>
}

#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace broseat {

class LibseatBackend : public Seat {
public:
    static std::unique_ptr<LibseatBackend> open(const SeatConfig& config, std::string* error = nullptr);
    ~LibseatBackend() override;

    SeatBackendType backend_type() const noexcept override { return SeatBackendType::Libseat; }
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
    explicit LibseatBackend(struct libseat* seat, std::string seat_name);

    static void on_enable_seat(struct libseat* seat, void* userdata);
    static void on_disable_seat(struct libseat* seat, void* userdata);

    void notify_event(const Event& event);

    struct libseat* seat_ = nullptr;
    std::string seat_name_;
    bool is_active_ = false;
    EventQueue event_queue_;
    std::function<void(const Event&)> callback_;
    mutable std::mutex mutex_;
};

}  // namespace broseat
