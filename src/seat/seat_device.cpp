#include "broseat/seat.h"

#include <unistd.h>
#include <utility>

namespace broseat {

SeatDevice::SeatDevice(Seat* seat, int device_id, int fd, std::string path, DeviceType type)
    : seat_(seat), device_id_(device_id), fd_(fd), path_(std::move(path)), type_(type) {}

SeatDevice::~SeatDevice() {
    close();
}

SeatDevice::SeatDevice(SeatDevice&& other) noexcept
    : seat_(other.seat_),
      device_id_(other.device_id_),
      fd_(other.fd_),
      path_(std::move(other.path_)),
      type_(other.type_) {
    other.seat_ = nullptr;
    other.device_id_ = -1;
    other.fd_ = -1;
}

SeatDevice& SeatDevice::operator=(SeatDevice&& other) noexcept {
    if (this != &other) {
        close();
        seat_ = other.seat_;
        device_id_ = other.device_id_;
        fd_ = other.fd_;
        path_ = std::move(other.path_);
        type_ = other.type_;

        other.seat_ = nullptr;
        other.device_id_ = -1;
        other.fd_ = -1;
    }
    return *this;
}

void SeatDevice::close() {
    if (seat_ && device_id_ >= 0) {
        seat_->close_device(device_id_);
        seat_ = nullptr;
        device_id_ = -1;
    }
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

int SeatDevice::release_fd() noexcept {
    int ret = fd_;
    fd_ = -1;
    return ret;
}

}  // namespace broseat
