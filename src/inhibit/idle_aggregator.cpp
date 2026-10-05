#include "broseat/inhibit.h"

namespace broseat {

IdleAggregator::IdleAggregator(std::chrono::milliseconds idle_timeout)
    : idle_timeout_(idle_timeout),
      last_activity_(std::chrono::steady_clock::now()) {}

void IdleAggregator::report_activity(std::chrono::steady_clock::time_point now) {
    last_activity_ = now;
    if (is_idle_) {
        is_idle_ = false;
        Event ev = IdleStateChanged{ .idle = false, .idle_time = std::chrono::milliseconds(0) };
        event_queue_.push(ev);
        if (callback_) {
            callback_(ev);
        }
    }
}

void IdleAggregator::set_idle_timeout(std::chrono::milliseconds timeout) {
    idle_timeout_ = timeout;
}

std::chrono::milliseconds IdleAggregator::current_idle_time(
    std::chrono::steady_clock::time_point now) const {
    if (now <= last_activity_) {
        return std::chrono::milliseconds(0);
    }
    return std::chrono::duration_cast<std::chrono::milliseconds>(now - last_activity_);
}

void IdleAggregator::set_inhibited(bool inhibited) {
    if (is_inhibited_ == inhibited) return;
    is_inhibited_ = inhibited;

    if (is_inhibited_ && is_idle_) {
        is_idle_ = false;
        Event ev = IdleStateChanged{
            .idle = false,
            .idle_time = current_idle_time()
        };
        event_queue_.push(ev);
        if (callback_) {
            callback_(ev);
        }
    }
}

bool IdleAggregator::check_idle(std::chrono::steady_clock::time_point now) {
    if (is_inhibited_) {
        return false;
    }

    auto elapsed = current_idle_time(now);
    if (!is_idle_ && elapsed >= idle_timeout_) {
        is_idle_ = true;
        Event ev = IdleStateChanged{
            .idle = true,
            .idle_time = elapsed
        };
        event_queue_.push(ev);
        if (callback_) {
            callback_(ev);
        }
        return true;
    }

    return is_idle_;
}

}  // namespace broseat
