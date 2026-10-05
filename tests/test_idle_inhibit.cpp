#include "broseat/inhibit.h"

#include <cassert>
#include <chrono>
#include <iostream>

using namespace std::chrono_literals;

int main() {
    using namespace broseat;

    // 1. Test IdleAggregator logic
    IdleAggregator agg(100ms);
    assert(!agg.is_idle());
    assert(!agg.is_inhibited());

    auto t0 = std::chrono::steady_clock::now();
    agg.report_activity(t0);

    // After 50ms (not yet timed out)
    assert(!agg.check_idle(t0 + 50ms));
    assert(!agg.is_idle());

    // After 150ms (timed out)
    assert(agg.check_idle(t0 + 150ms));
    assert(agg.is_idle());

    auto events = agg.event_queue().drain();
    assert(events.size() == 1);
    assert(std::holds_alternative<IdleStateChanged>(events[0]));
    assert(std::get<IdleStateChanged>(events[0]).idle == true);

    // Reporting activity resets idle
    agg.report_activity(t0 + 160ms);
    assert(!agg.is_idle());
    events = agg.event_queue().drain();
    assert(events.size() == 1);
    assert(std::holds_alternative<IdleStateChanged>(events[0]));
    assert(std::get<IdleStateChanged>(events[0]).idle == false);

    // Test inhibitor on IdleAggregator
    agg.set_inhibited(true);
    assert(agg.is_inhibited());
    // Should NOT go idle when inhibited
    assert(!agg.check_idle(t0 + 500ms));
    assert(!agg.is_idle());

    agg.set_inhibited(false);
    assert(agg.check_idle(t0 + 500ms));
    assert(agg.is_idle());

    // 2. Test InhibitManager
    std::string err;
    auto mgr = InhibitManager::create(&err);
    if (!mgr) {
        std::cout << "InhibitManager not available: " << err << "\n";
    } else {
        auto inhibitors = mgr->list_inhibitors(&err);
        std::cout << "Found " << inhibitors.size() << " active system inhibitors:\n";
        for (const auto& inh : inhibitors) {
            std::cout << "  - what=" << inh.what << ", who=" << inh.who
                      << ", why=" << inh.why << ", mode=" << inh.mode
                      << ", uid=" << inh.uid << ", pid=" << inh.pid << "\n";
        }

        // Test acquiring an inhibitor lock
        auto lock = mgr->inhibit("idle", "broseat-test", "Running test_idle_inhibit", InhibitMode::Block, &err);
        if (lock && lock->is_held()) {
            std::cout << "Inhibitor acquired successfully, fd=" << lock->fd() << "\n";
            assert(lock->is_held());
            assert(lock->what() == "idle");
            assert(lock->who() == "broseat-test");
            assert(mgr->is_inhibited("idle"));

            lock->release();
            assert(!lock->is_held());
            std::cout << "Inhibitor released.\n";
        } else {
            std::cout << "Could not acquire inhibitor (permission or environment): " << err << "\n";
        }
    }

    std::cout << "test_idle_inhibit PASSED\n";
    return 0;
}
