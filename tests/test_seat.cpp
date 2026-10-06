// Seat control through libseat and logind, for real. Taking control of a seat
// needs a session no compositor already controls (a free VT, or a seatd the
// user may use), so on a desktop that is running a compositor, and on CI
// runners with no seat at all, the device part skips and says why. Whatever
// the machine, a refused backend must say why. Linux.
#include "check.h"
#include "broseat/seat.h"

#include <string>

using namespace broseat;

int main() {
    std::string libseat_err, logind_err;
    SeatConfig libseat_cfg;
    libseat_cfg.backend = SeatBackendType::Libseat;
    SeatConfig logind_cfg;
    logind_cfg.backend = SeatBackendType::Logind;
    auto libseat = Seat::create(libseat_cfg, &libseat_err);
    if (!libseat) CHECK(!libseat_err.empty());
    auto logind = Seat::create(logind_cfg, &logind_err);
    if (!logind) CHECK(!logind_err.empty());
    if (libseat) CHECK(libseat->backend_type() == SeatBackendType::Libseat);
    if (logind) CHECK(logind->backend_type() == SeatBackendType::Logind);
    libseat.reset();
    logind.reset();

    std::string err;
    auto seat = Seat::create(SeatConfig{}, &err);
    if (!seat) {
        CHECK(!err.empty());
        if (bstest::failures() > 0) return bstest::finish("test_seat");
        bstest::skip("test_seat", "no seat this process may control (a compositor owns it, or there is "
                                  "no seat): " + err);
    }

    std::printf("Acquired seat %s via %s (active: %d)\n", seat->seat_name().c_str(),
                std::string(seat_backend_name(seat->backend_type())).c_str(), seat->is_active());
    CHECK(!seat->seat_name().empty());
    CHECK(seat->poll_fd() >= 0);
    CHECK(seat->dispatch(0) >= 0);

    if (!seat->is_active()) {
        std::printf("Note: the seat is not active; device opening was not checked\n");
        return bstest::finish("test_seat");
    }

    std::string dev_err;
    auto dev = seat->open_device("/dev/dri/card0", &dev_err);
    if (!dev) {
        std::printf("Note: /dev/dri/card0 could not be opened: %s\n", dev_err.c_str());
        return bstest::finish("test_seat");
    }
    CHECK(dev->is_valid());
    CHECK(dev->fd() >= 0);
    CHECK(dev->type() == DeviceType::DrmCard);

    int id = dev->device_id();
    int fd = dev->fd();
    SeatDevice moved = std::move(*dev);
    CHECK(moved.is_valid());
    CHECK_EQ(moved.device_id(), id);
    CHECK_EQ(moved.fd(), fd);
    CHECK(!dev->is_valid());
    moved.close();
    CHECK(!moved.is_valid());

    return bstest::finish("test_seat");
}
