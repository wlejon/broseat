#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace broseat {

enum class SeatBackendType {
    Auto,
    Libseat,
    Logind,
    Mock
};

enum class DeviceType {
    DrmCard,
    DrmRender,
    Evdev,
    Other
};

enum class SessionState {
    Active,
    Online,
    Closing,
    Unknown
};

enum class InhibitMode {
    Block,
    Delay
};

enum class AutostartPhase {
    EarlySetup,
    Initialization,
    Application
};

enum class LaunchMode {
    Auto,
    SystemdTransient,
    SystemdRun,
    ForkExec
};

struct InhibitorInfo {
    std::string what;
    std::string who;
    std::string why;
    std::string mode;
    uint32_t uid = 0;
    uint32_t pid = 0;

    bool operator==(const InhibitorInfo&) const = default;
};

struct AutostartEntry {
    std::string id;
    std::string name;
    std::string comment;
    std::string exec;
    std::string try_exec;
    std::string icon;
    std::string working_dir;
    std::string autostart_condition;
    std::string type = "Application";
    std::vector<std::string> only_show_in;
    std::vector<std::string> not_show_in;
    bool hidden = false;
    bool no_display = false;
    bool terminal = false;
    AutostartPhase phase = AutostartPhase::Application;
    std::filesystem::path file_path;

    bool operator==(const AutostartEntry&) const = default;
};

struct LaunchResult {
    bool success = false;
    uint32_t pid = 0;
    std::string unit_name;
    std::string error;
};

struct SessionInfo {
    std::string id;
    std::string seat;
    std::string user;
    uint32_t vt = 0;
    bool active = false;
    bool locked = false;
    SessionState state = SessionState::Unknown;

    bool operator==(const SessionInfo&) const = default;
};

std::string_view seat_backend_name(SeatBackendType backend);
SeatBackendType parse_seat_backend(std::string_view name);

std::string_view device_type_name(DeviceType type);
DeviceType detect_device_type(const std::string& path);

std::string_view session_state_name(SessionState state);
SessionState parse_session_state(std::string_view name);

std::string_view inhibit_mode_name(InhibitMode mode);
InhibitMode parse_inhibit_mode(std::string_view name);

std::string_view autostart_phase_name(AutostartPhase phase);
AutostartPhase parse_autostart_phase(std::string_view name);

std::string_view launch_mode_name(LaunchMode mode);

}  // namespace broseat
