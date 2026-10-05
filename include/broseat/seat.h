#pragma once

#include "broseat/event_queue.h"
#include "broseat/events.h"
#include "broseat/types.h"

#include <functional>
#include <memory>
#include <string>

namespace broseat {

class Seat;

class SeatDevice {
public:
    SeatDevice() = default;
    SeatDevice(Seat* seat, int device_id, int fd, std::string path, DeviceType type);
    ~SeatDevice();

    SeatDevice(const SeatDevice&) = delete;
    SeatDevice& operator=(const SeatDevice&) = delete;

    SeatDevice(SeatDevice&& other) noexcept;
    SeatDevice& operator=(SeatDevice&& other) noexcept;

    int device_id() const noexcept { return device_id_; }
    int fd() const noexcept { return fd_; }
    const std::string& path() const noexcept { return path_; }
    DeviceType type() const noexcept { return type_; }
    bool is_valid() const noexcept { return fd_ >= 0 && device_id_ >= 0; }

    void close();
    int release_fd() noexcept;

private:
    Seat* seat_ = nullptr;
    int device_id_ = -1;
    int fd_ = -1;
    std::string path_;
    DeviceType type_ = DeviceType::Other;
};

struct SeatConfig {
    SeatBackendType backend = SeatBackendType::Auto;
    std::string seat_name;
    std::string session_id;
};

class Seat {
public:
    static std::unique_ptr<Seat> create(const SeatConfig& config = {}, std::string* error = nullptr);
    virtual ~Seat() = default;

    virtual SeatBackendType backend_type() const noexcept = 0;
    virtual const std::string& seat_name() const noexcept = 0;
    virtual bool is_active() const noexcept = 0;

    virtual std::unique_ptr<SeatDevice> open_device(const std::string& path, std::string* error = nullptr) = 0;
    virtual bool close_device(int device_id) = 0;

    virtual int switch_vt(int vt_number) = 0;

    virtual int poll_fd() const noexcept = 0;
    virtual int dispatch(int timeout_ms = 0) = 0;

    virtual EventQueue& event_queue() noexcept = 0;
    virtual void set_event_callback(std::function<void(const Event&)> cb) = 0;

    virtual void acknowledge_device_pause(int device_id) = 0;
};

}  // namespace broseat
