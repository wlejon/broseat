#include "seat/libseat_backend.h"

#include <cerrno>
#include <cstring>

namespace broseat {

LibseatBackend::LibseatBackend(struct libseat* seat, std::string seat_name)
    : seat_(seat), seat_name_(std::move(seat_name)) {}

LibseatBackend::~LibseatBackend() {
    if (seat_) {
        libseat_close_seat(seat_);
        seat_ = nullptr;
    }
}

std::unique_ptr<LibseatBackend> LibseatBackend::open(const SeatConfig& /*config*/, std::string* error) {
    auto backend = std::unique_ptr<LibseatBackend>(new LibseatBackend(nullptr, ""));

    static const struct libseat_seat_listener listener = {
        .enable_seat = &LibseatBackend::on_enable_seat,
        .disable_seat = &LibseatBackend::on_disable_seat,
    };

    struct libseat* seat = libseat_open_seat(&listener, backend.get());
    if (!seat) {
        if (error) {
            *error = strerror(errno);
        }
        return nullptr;
    }

    backend->seat_ = seat;
    const char* name = libseat_seat_name(seat);
    backend->seat_name_ = (name && name[0] != '\0') ? name : "seat0";

    return backend;
}

void LibseatBackend::on_enable_seat(struct libseat* /*seat*/, void* userdata) {
    auto* self = static_cast<LibseatBackend*>(userdata);
    if (self) {
        self->is_active_ = true;
        self->notify_event(SeatActiveChanged{true});
    }
}

void LibseatBackend::on_disable_seat(struct libseat* seat, void* userdata) {
    auto* self = static_cast<LibseatBackend*>(userdata);
    if (self) {
        self->is_active_ = false;
        self->notify_event(SeatActiveChanged{false});
        libseat_disable_seat(seat);
    }
}

std::unique_ptr<SeatDevice> LibseatBackend::open_device(const std::string& path, std::string* error) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!seat_) {
        if (error) *error = "libseat is not initialized";
        return nullptr;
    }

    int fd = -1;
    int dev_id = libseat_open_device(seat_, path.c_str(), &fd);
    if (dev_id < 0) {
        if (error) {
            *error = strerror(errno);
        }
        return nullptr;
    }

    return std::make_unique<SeatDevice>(this, dev_id, fd, path, detect_device_type(path));
}

bool LibseatBackend::close_device(int device_id) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!seat_ || device_id < 0) return false;
    return libseat_close_device(seat_, device_id) == 0;
}

int LibseatBackend::switch_vt(int vt_number) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!seat_ || vt_number <= 0) return -1;
    int r = libseat_switch_session(seat_, vt_number);
    if (r == 0) {
        notify_event(VtSwitched{vt_number});
    }
    return r;
}

int LibseatBackend::poll_fd() const noexcept {
    return seat_ ? libseat_get_fd(seat_) : -1;
}

int LibseatBackend::dispatch(int timeout_ms) {
    if (!seat_) return -1;
    return libseat_dispatch(seat_, timeout_ms);
}

void LibseatBackend::acknowledge_device_pause(int /*device_id*/) {
    // libseat handles device revoking internally during session disable handshake.
}

void LibseatBackend::notify_event(const Event& event) {
    event_queue_.push(event);
    if (callback_) {
        callback_(event);
    }
}

}  // namespace broseat
