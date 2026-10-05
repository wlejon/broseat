#include "broseat/seat.h"

#include <cassert>
#include <iostream>
#include <unistd.h>

int main() {
    using namespace broseat;

    std::cout << "Testing real seat backend creation (Auto / Libseat / Logind)...\n";

    // 1. Test real Auto creation
    SeatConfig auto_cfg;
    auto_cfg.backend = SeatBackendType::Auto;
    std::string auto_err;
    auto seat = Seat::create(auto_cfg, &auto_err);

    if (!seat) {
        // In a running desktop environment (like KDE Plasma/KWin), the running compositor
        // already holds exclusive control of seat0 via logind TakeControl.
        // Therefore, secondary unprivileged processes cannot take control of the same seat.
        std::cout << "Notice: Real seat control could not be acquired: " << auto_err << "\n";
        std::cout << "Skipping hardware seat manipulation (seat already owned or unprivileged).\n";
        // CTest skip code
        return 77;
    }

    // If we acquired seat control:
    std::cout << "Acquired seat: " << seat->seat_name()
              << " via backend: " << seat_backend_name(seat->backend_type()) << "\n";
    assert(!seat->seat_name().empty());

    // Test device opening on real seat if active
    if (seat->is_active()) {
        std::string dev_err;
        auto dev = seat->open_device("/dev/dri/card0", &dev_err);
        if (dev) {
            assert(dev->is_valid());
            assert(dev->fd() >= 0);
            assert(dev->type() == DeviceType::DrmCard);

            // Test move semantics
            int id = dev->device_id();
            int fd = dev->fd();
            SeatDevice moved = std::move(*dev);
            assert(moved.is_valid());
            assert(moved.device_id() == id);
            assert(moved.fd() == fd);
            assert(!dev->is_valid());

            moved.close();
            assert(!moved.is_valid());
        }
    }

    std::cout << "test_seat PASSED\n";
    return 0;
}
