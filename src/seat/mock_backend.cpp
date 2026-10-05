#include "seat/mock_backend.h"

#include <fcntl.h>
#include <sys/eventfd.h>
#include <unistd.h>
#include <utility>

namespace broseat {

MockSeatBackend::MockSeatBackend(std::string seat_name)
    : seat_name_(std::move(seat_name)),
      event_fd_(eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)) {}

MockSeatBackend::~MockSeatBackend() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& [id, dev] : devices_) {
        if (dev.fd >= 0) {
            ::close(dev.fd);
        }
    }
    devices_.clear();
    if (event_fd_ >= 0) {
        ::close(event_fd_);
        event_fd_ = -1;
    }
}

std::unique_ptr<SeatDevice> MockSeatBackend::open_device(const std::string& path, std::string* error) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!is_active_) {
        if (error) *error = "seat is not active";
        return nullptr;
    }

    int dev_fd = ::open("/dev/null", O_RDWR | O_CLOEXEC);
    if (dev_fd < 0) {
        if (error) *error = "failed to create mock device fd";
        return nullptr;
    }

    int id = next_device_id_++;
    DeviceType type = detect_device_type(path);
    devices_[id] = DeviceRecord{id, dev_fd, path, type};

    return std::make_unique<SeatDevice>(this, id, dev_fd, path, type);
}

bool MockSeatBackend::close_device(int device_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = devices_.find(device_id);
    if (it == devices_.end()) {
        return false;
    }
    devices_.erase(it);
    return true;
}

int MockSeatBackend::switch_vt(int vt_number) {
    if (vt_number <= 0) return -1;
    current_vt_ = vt_number;
    notify_event(VtSwitched{vt_number});
    return 0;
}

int MockSeatBackend::dispatch(int /*timeout_ms*/) {
    if (event_fd_ >= 0) {
        uint64_t val = 0;
        [[maybe_unused]] ssize_t s = ::read(event_fd_, &val, sizeof(val));
    }
    return 0;
}

void MockSeatBackend::acknowledge_device_pause(int device_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    acknowledged_pauses_[device_id] = true;
}

void MockSeatBackend::set_active(bool active) {
    if (is_active_ != active) {
        is_active_ = active;
        notify_event(SeatActiveChanged{active});
    }
}

void MockSeatBackend::simulate_pause_device(int device_id, bool force) {
    acknowledged_pauses_[device_id] = false;
    notify_event(SeatDevicePaused{device_id, force});
}

void MockSeatBackend::simulate_resume_device(int device_id, int fd) {
    notify_event(SeatDeviceResumed{device_id, fd});
}

bool MockSeatBackend::was_pause_acknowledged(int device_id) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = acknowledged_pauses_.find(device_id);
    return it != acknowledged_pauses_.end() && it->second;
}

void MockSeatBackend::notify_event(const Event& event) {
    event_queue_.push(event);
    if (callback_) {
        callback_(event);
    }
    if (event_fd_ >= 0) {
        uint64_t val = 1;
        [[maybe_unused]] ssize_t s = ::write(event_fd_, &val, sizeof(val));
    }
}

}  // namespace broseat
