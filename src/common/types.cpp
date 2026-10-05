#include "broseat/types.h"

#include <algorithm>
#include <string_view>

namespace broseat {

std::string_view seat_backend_name(SeatBackendType backend) {
    switch (backend) {
        case SeatBackendType::Auto: return "auto";
        case SeatBackendType::Libseat: return "libseat";
        case SeatBackendType::Logind: return "logind";
        case SeatBackendType::Mock: return "mock";
    }
    return "unknown";
}

SeatBackendType parse_seat_backend(std::string_view name) {
    if (name == "libseat") return SeatBackendType::Libseat;
    if (name == "logind") return SeatBackendType::Logind;
    if (name == "mock") return SeatBackendType::Mock;
    return SeatBackendType::Auto;
}

std::string_view device_type_name(DeviceType type) {
    switch (type) {
        case DeviceType::DrmCard: return "drm-card";
        case DeviceType::DrmRender: return "drm-render";
        case DeviceType::Evdev: return "evdev";
        case DeviceType::Other: return "other";
    }
    return "unknown";
}

DeviceType detect_device_type(const std::string& path) {
    if (path.rfind("/dev/dri/card", 0) == 0) {
        return DeviceType::DrmCard;
    }
    if (path.rfind("/dev/dri/renderD", 0) == 0) {
        return DeviceType::DrmRender;
    }
    if (path.rfind("/dev/input/event", 0) == 0) {
        return DeviceType::Evdev;
    }
    return DeviceType::Other;
}

std::string_view session_state_name(SessionState state) {
    switch (state) {
        case SessionState::Active: return "active";
        case SessionState::Online: return "online";
        case SessionState::Closing: return "closing";
        case SessionState::Unknown: return "unknown";
    }
    return "unknown";
}

SessionState parse_session_state(std::string_view name) {
    if (name == "active") return SessionState::Active;
    if (name == "online") return SessionState::Online;
    if (name == "closing") return SessionState::Closing;
    return SessionState::Unknown;
}

std::string_view inhibit_mode_name(InhibitMode mode) {
    switch (mode) {
        case InhibitMode::Block: return "block";
        case InhibitMode::Delay: return "delay";
    }
    return "block";
}

InhibitMode parse_inhibit_mode(std::string_view name) {
    if (name == "delay") return InhibitMode::Delay;
    return InhibitMode::Block;
}

std::string_view autostart_phase_name(AutostartPhase phase) {
    switch (phase) {
        case AutostartPhase::EarlySetup: return "EarlySetup";
        case AutostartPhase::Initialization: return "Initialization";
        case AutostartPhase::Application: return "Application";
    }
    return "Application";
}

AutostartPhase parse_autostart_phase(std::string_view name) {
    if (name == "EarlySetup" || name == "earlysetup" || name == "EarlyInitialization") {
        return AutostartPhase::EarlySetup;
    }
    if (name == "Initialization" || name == "initialization" || name == "Phase1") {
        return AutostartPhase::Initialization;
    }
    return AutostartPhase::Application;
}

std::string_view launch_mode_name(LaunchMode mode) {
    switch (mode) {
        case LaunchMode::Auto: return "auto";
        case LaunchMode::SystemdTransient: return "systemd-transient";
        case LaunchMode::SystemdRun: return "systemd-run";
        case LaunchMode::ForkExec: return "fork-exec";
    }
    return "unknown";
}

}  // namespace broseat
