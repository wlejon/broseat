// bro.seat's idle timer: setIdleTimeout / getIdleTimeout / getIdleState and
// the `idle` event, fed by the host (tickIdle) with its own clock and the time
// of the last user input it saw.
//
// The host is the one that knows about input: under a DRM session bro reads
// libinput itself, so no compositor or logind is in a position to tell it the
// user went quiet. Its clock is the host's too (a headless run's is virtual),
// so the timer is driven entirely from tickIdle and never reads a wall clock.

#include "api.h"
#include "host_seat_internal.h"
#include "object_builder.h"
#include "broseat/events.h"
#include "broseat/inhibit.h"

#include <chrono>
#include <memory>

namespace broseat::api {

namespace {

using Clock = std::chrono::steady_clock;

Clock::time_point at(double ms) {
    return Clock::time_point(std::chrono::duration_cast<Clock::duration>(
        std::chrono::duration<double, std::milli>(ms)));
}

// How often the session-wide (logind) idle inhibitors are asked for: a bus
// round trip, and an inhibitor taken by another program changes rarely.
constexpr double kSystemInhibitPollMs = 5000.0;

struct IdleTimer {
    double timeoutMs = 0.0;               // 0: no timer
    std::unique_ptr<IdleAggregator> agg;
    double nowMs = 0.0;                   // the host clock at the last tick
    double lastActivitySeen = -1.0;
    bool hostInhibited = false;
    bool systemInhibited = false;
    double lastSystemPollMs = -1.0;
    bool inhibited = false;
    bool localInhibited = false;
};

IdleTimer g_idle;

void dispatchIdleEvents() {
    if (!g_idle.agg) return;
    for (const auto& evVariant : g_idle.agg->event_queue().drain()) {
        if (const auto* e = std::get_if<IdleStateChanged>(&evVariant)) {
            ObjectBuilder b;
            b.set("type", "idle");
            b.set("idle", e->idle);
            b.set("idleTime", static_cast<double>(e->idle_time.count()));
            dispatchSeatEvent("idle", b.build());
        }
    }
}

bool localIdleInhibited() {
    return hasLocalInhibitor("idle");
}

} // namespace

void tickIdle(double nowMs, double lastActivityMs, bool hostInhibited) {
    g_idle.nowMs = nowMs;
    g_idle.hostInhibited = hostInhibited;
    if (!g_idle.agg) return;

    if (lastActivityMs > g_idle.lastActivitySeen) {
        g_idle.lastActivitySeen = lastActivityMs;
        g_idle.agg->report_activity(at(lastActivityMs));
    }

    // This process's own inhibitors are logind's too, so the session-wide
    // answer is asked again whenever they change, not only on the poll.
    const bool localInhibited = localIdleInhibited();
    const bool localChanged = localInhibited != g_idle.localInhibited;
    g_idle.localInhibited = localInhibited;
    if (localChanged || g_idle.lastSystemPollMs < 0.0 ||
        nowMs - g_idle.lastSystemPollMs >= kSystemInhibitPollMs) {
        g_idle.lastSystemPollMs = nowMs;
        auto mgr = activeInhibitManager();
        g_idle.systemInhibited = mgr && mgr->is_inhibited("idle");
    }

    const bool inhibited = hostInhibited || g_idle.systemInhibited || localInhibited;
    if (inhibited != g_idle.inhibited) {
        g_idle.inhibited = inhibited;
        g_idle.agg->set_inhibited(inhibited);
        // An inhibitor going away restarts the countdown: the screen does not
        // lock the moment a video ends because nobody touched the mouse while
        // it played.
        if (!inhibited) g_idle.agg->report_activity(at(nowMs));
    }

    g_idle.agg->check_idle(at(nowMs));
    dispatchIdleEvents();
}

void resetIdleTimer() {
    g_idle = IdleTimer{};
}

void installIdleOnto(Value seatVal) {
    ObjectBuilder seat(seatVal);

    // bro.seat.setIdleTimeout(ms) -> number (the timeout now in force).
    // 0 (or anything not a positive number) turns the timer off. The countdown
    // starts from the later of now and the last input.
    seat.def("setIdleTimeout", 1, [](Value, std::span<const Value> args) -> Value {
        double ms = 0.0;
        if (!args.empty()) {
            ev::Persistent arg0(args[0]);
            if (ev::isNumber(arg0.get())) ms = ev::toDouble(arg0.get());
        }
        if (!(ms > 0.0)) {
            // Turning the timer off while idle says so, so a listener that
            // blanked the screen is told to bring it back.
            if (g_idle.agg && g_idle.agg->is_idle()) {
                g_idle.agg->report_activity(at(g_idle.nowMs));
                dispatchIdleEvents();
            }
            g_idle.agg.reset();
            g_idle.timeoutMs = 0.0;
            return ev::fromDouble(0.0);
        }
        const auto timeout = std::chrono::milliseconds(static_cast<long long>(ms));
        if (!g_idle.agg) {
            g_idle.agg = std::make_unique<IdleAggregator>(timeout);
            g_idle.agg->report_activity(at(g_idle.nowMs > g_idle.lastActivitySeen ? g_idle.nowMs
                                                                                  : g_idle.lastActivitySeen));
            g_idle.inhibited = false;
            g_idle.lastSystemPollMs = -1.0;
        } else {
            g_idle.agg->set_idle_timeout(timeout);
        }
        g_idle.timeoutMs = ms;
        return ev::fromDouble(ms);
    });

    // bro.seat.getIdleTimeout() -> number (0: off)
    seat.def("getIdleTimeout", 0, [](Value, std::span<const Value>) -> Value {
        return ev::fromDouble(g_idle.timeoutMs);
    });

    // bro.seat.getIdleState() -> { idle, idleTime, timeout, inhibited }
    seat.def("getIdleState", 0, [](Value, std::span<const Value>) -> Value {
        ObjectBuilder b;
        const bool on = g_idle.agg != nullptr;
        b.set("idle", on && g_idle.agg->is_idle());
        b.set("idleTime", on ? static_cast<double>(g_idle.agg->current_idle_time(at(g_idle.nowMs)).count())
                             : 0.0);
        b.set("timeout", g_idle.timeoutMs);
        b.set("inhibited", on ? g_idle.inhibited
                              : (g_idle.hostInhibited || localIdleInhibited()));
        return b.build();
    });
}

} // namespace broseat::api
