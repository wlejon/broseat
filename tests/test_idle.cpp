// IdleAggregator: activity, timeouts, inhibition and the events it publishes.
// Pure logic on caller-supplied time points. Portable: runs on every platform.
#include "check.h"
#include "broseat/inhibit.h"

#include <chrono>

using namespace std::chrono_literals;
using namespace broseat;

static bool drained_idle(IdleAggregator& agg, bool expected) {
    auto events = agg.event_queue().drain();
    if (events.size() != 1 || !std::holds_alternative<IdleStateChanged>(events[0])) return false;
    return std::get<IdleStateChanged>(events[0]).idle == expected;
}

int main() {
    IdleAggregator agg(100ms);
    CHECK(!agg.is_idle());
    CHECK(!agg.is_inhibited());
    CHECK(agg.idle_timeout() == 100ms);

    auto t0 = std::chrono::steady_clock::now();
    agg.report_activity(t0);
    CHECK(!agg.check_idle(t0 + 50ms));
    CHECK(!agg.is_idle());
    CHECK(agg.current_idle_time(t0 + 50ms) == 50ms);

    CHECK(agg.check_idle(t0 + 150ms));
    CHECK(agg.is_idle());
    CHECK(drained_idle(agg, true));

    // Activity ends idleness and says so once.
    agg.report_activity(t0 + 160ms);
    CHECK(!agg.is_idle());
    CHECK(drained_idle(agg, false));
    agg.report_activity(t0 + 170ms);
    CHECK(agg.event_queue().drain().empty());

    // An inhibitor holds idleness off; releasing it lets the timeout apply.
    agg.set_inhibited(true);
    CHECK(agg.is_inhibited());
    CHECK(!agg.check_idle(t0 + 500ms));
    CHECK(!agg.is_idle());
    agg.set_inhibited(false);
    CHECK(agg.check_idle(t0 + 500ms));
    CHECK(agg.is_idle());

    // The callback sees the same events as the queue.
    int calls = 0;
    agg.set_event_callback([&](const Event& e) {
        if (std::holds_alternative<IdleStateChanged>(e)) ++calls;
    });
    agg.event_queue().drain();
    agg.report_activity(t0 + 600ms);
    CHECK_EQ(calls, 1);

    // A longer timeout takes effect from the last activity.
    agg.set_idle_timeout(1000ms);
    CHECK(!agg.check_idle(t0 + 1500ms));
    CHECK(agg.check_idle(t0 + 1700ms));

    return bstest::finish("test_idle");
}
