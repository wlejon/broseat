#pragma once

#include "broseat/types.h"

#include <chrono>
#include <string>
#include <variant>

namespace broseat {

struct SeatActiveChanged {
    bool active = false;
    bool operator==(const SeatActiveChanged&) const = default;
};

struct SeatDevicePaused {
    int device_id = -1;
    bool force = false;
    bool operator==(const SeatDevicePaused&) const = default;
};

struct SeatDeviceResumed {
    int device_id = -1;
    int fd = -1;
    bool operator==(const SeatDeviceResumed&) const = default;
};

struct SessionLockedChanged {
    bool locked = false;
    bool operator==(const SessionLockedChanged&) const = default;
};

struct SessionStateChanged {
    SessionState state = SessionState::Unknown;
    bool operator==(const SessionStateChanged&) const = default;
};

struct VtSwitched {
    int vt_number = 0;
    bool operator==(const VtSwitched&) const = default;
};

struct IdleStateChanged {
    bool idle = false;
    std::chrono::milliseconds idle_time{0};
    bool operator==(const IdleStateChanged&) const = default;
};

struct InhibitorAdded {
    InhibitorInfo info;
    bool operator==(const InhibitorAdded&) const = default;
};

struct InhibitorRemoved {
    std::string who;
    std::string what;
    bool operator==(const InhibitorRemoved&) const = default;
};

struct AutostartEntryLaunched {
    std::string entry_id;
    uint32_t pid = 0;
    bool success = false;
    std::string error;
    bool operator==(const AutostartEntryLaunched&) const = default;
};

using Event = std::variant<
    SeatActiveChanged,
    SeatDevicePaused,
    SeatDeviceResumed,
    SessionLockedChanged,
    SessionStateChanged,
    VtSwitched,
    IdleStateChanged,
    InhibitorAdded,
    InhibitorRemoved,
    AutostartEntryLaunched
>;

}  // namespace broseat
