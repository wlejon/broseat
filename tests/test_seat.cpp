#include "broseat/seat.h"
#include "seat/mock_backend.h"

#include <cassert>
#include <iostream>

int main() {
    using namespace broseat;

    // 1. Direct MockSeatBackend test
    auto mock = std::make_unique<MockSeatBackend>("seat0");
    assert(mock->backend_type() == SeatBackendType::Mock);
    assert(mock->seat_name() == "seat0");
    assert(mock->is_active());

    // Test device opening
    std::string err;
    auto dev1 = mock->open_device("/dev/dri/card0", &err);
    assert(dev1 != nullptr);
    assert(dev1->is_valid());
    assert(dev1->device_id() == 1);
    assert(dev1->fd() >= 0);
    assert(dev1->type() == DeviceType::DrmCard);
    assert(dev1->path() == "/dev/dri/card0");

    auto dev2 = mock->open_device("/dev/input/event3", &err);
    assert(dev2 != nullptr);
    assert(dev2->is_valid());
    assert(dev2->device_id() == 2);
    assert(dev2->type() == DeviceType::Evdev);

    // Test device move semantics
    int old_id = dev1->device_id();
    int old_fd = dev1->fd();
    SeatDevice dev1_moved = std::move(*dev1);
    assert(dev1_moved.is_valid());
    assert(dev1_moved.device_id() == old_id);
    assert(dev1_moved.fd() == old_fd);
    assert(!dev1->is_valid());

    // Test callback & event queue
    int event_count = 0;
    mock->set_event_callback([&event_count](const Event& /*e*/) {
        event_count++;
    });

    // Test VT switch
    int r = mock->switch_vt(3);
    assert(r == 0);
    assert(mock->current_vt() == 3);
    assert(event_count == 1);

    auto events = mock->event_queue().drain();
    assert(events.size() == 1);
    assert(std::holds_alternative<VtSwitched>(events[0]));
    assert(std::get<VtSwitched>(events[0]).vt_number == 3);

    // Test simulate pause and acknowledge
    mock->simulate_pause_device(1, false);
    assert(event_count == 2);
    assert(!mock->was_pause_acknowledged(1));
    mock->acknowledge_device_pause(1);
    assert(mock->was_pause_acknowledged(1));

    // Test simulate resume
    mock->simulate_resume_device(1, dev1_moved.fd());
    assert(event_count == 3);

    // Test active state change
    mock->set_active(false);
    assert(!mock->is_active());
    assert(event_count == 4);

    // Cannot open device when inactive
    auto dev_fail = mock->open_device("/dev/dri/card0", &err);
    assert(dev_fail == nullptr);
    assert(!err.empty());

    mock->set_active(true);
    assert(mock->is_active());
    assert(event_count == 5);

    // Test poll_fd and dispatch
    assert(mock->poll_fd() >= 0);
    assert(mock->dispatch(0) == 0);

    // Test closing device
    dev1_moved.close();
    assert(!dev1_moved.is_valid());
    dev2.reset();

    // 2. Factory creation with Mock backend
    SeatConfig cfg;
    cfg.backend = SeatBackendType::Mock;
    cfg.seat_name = "seat-test";
    auto seat = Seat::create(cfg);
    assert(seat != nullptr);
    assert(seat->backend_type() == SeatBackendType::Mock);
    assert(seat->seat_name() == "seat-test");

    // 3. Factory creation with Auto / Libseat
    SeatConfig auto_cfg;
    auto_cfg.backend = SeatBackendType::Auto;
    std::string auto_err;
    auto auto_seat = Seat::create(auto_cfg, &auto_err);
    if (!auto_seat) {
        std::cout << "Auto seat not available (expected in non-seat or sandboxed environment): "
                  << auto_err << "\n";
    } else {
        std::cout << "Auto seat backend created successfully: "
                  << seat_backend_name(auto_seat->backend_type()) << "\n";
    }

    std::cout << "test_seat PASSED\n";
    return 0;
}
