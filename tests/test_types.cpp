#include "broseat/types.h"
#include "common/utils.h"

#include <cassert>
#include <iostream>

int main() {
    using namespace broseat;

    // Test seat backend names and parsing
    assert(seat_backend_name(SeatBackendType::Auto) == "auto");
    assert(seat_backend_name(SeatBackendType::Libseat) == "libseat");
    assert(seat_backend_name(SeatBackendType::Logind) == "logind");

    assert(parse_seat_backend("auto") == SeatBackendType::Auto);
    assert(parse_seat_backend("libseat") == SeatBackendType::Libseat);
    assert(parse_seat_backend("logind") == SeatBackendType::Logind);
    assert(parse_seat_backend("unknown") == SeatBackendType::Auto);

    // Test device type names and detection
    assert(device_type_name(DeviceType::DrmCard) == "drm-card");
    assert(device_type_name(DeviceType::DrmRender) == "drm-render");
    assert(device_type_name(DeviceType::Evdev) == "evdev");
    assert(device_type_name(DeviceType::Other) == "other");

    assert(detect_device_type("/dev/dri/card0") == DeviceType::DrmCard);
    assert(detect_device_type("/dev/dri/card1") == DeviceType::DrmCard);
    assert(detect_device_type("/dev/dri/renderD128") == DeviceType::DrmRender);
    assert(detect_device_type("/dev/input/event0") == DeviceType::Evdev);
    assert(detect_device_type("/dev/tty0") == DeviceType::Other);

    // Test session state
    assert(session_state_name(SessionState::Active) == "active");
    assert(session_state_name(SessionState::Online) == "online");
    assert(session_state_name(SessionState::Closing) == "closing");
    assert(parse_session_state("active") == SessionState::Active);
    assert(parse_session_state("online") == SessionState::Online);
    assert(parse_session_state("closing") == SessionState::Closing);
    assert(parse_session_state("unknown") == SessionState::Unknown);

    // Test inhibit modes
    assert(inhibit_mode_name(InhibitMode::Block) == "block");
    assert(inhibit_mode_name(InhibitMode::Delay) == "delay");
    assert(parse_inhibit_mode("block") == InhibitMode::Block);
    assert(parse_inhibit_mode("delay") == InhibitMode::Delay);

    // Test autostart phases
    assert(autostart_phase_name(AutostartPhase::EarlySetup) == "EarlySetup");
    assert(autostart_phase_name(AutostartPhase::Initialization) == "Initialization");
    assert(autostart_phase_name(AutostartPhase::Application) == "Application");
    assert(parse_autostart_phase("EarlySetup") == AutostartPhase::EarlySetup);
    assert(parse_autostart_phase("earlysetup") == AutostartPhase::EarlySetup);
    assert(parse_autostart_phase("Initialization") == AutostartPhase::Initialization);
    assert(parse_autostart_phase("Application") == AutostartPhase::Application);

    // Test utils
    assert(utils::trim("  hello world  \t\n") == "hello world");
    assert(utils::trim("") == "");

    auto parts = utils::split("foo;bar;baz;", ';');
    assert(parts.size() == 3);
    assert(parts[0] == "foo");
    assert(parts[1] == "bar");
    assert(parts[2] == "baz");

    assert(utils::iequals("GNOME", "gnome"));
    assert(utils::iequals("bRo", "BRo"));
    assert(!utils::iequals("KDE", "XFCE"));

    assert(utils::starts_with("prefix_test", "prefix_"));
    assert(!utils::starts_with("prefix_test", "other"));

    assert(utils::sanitize_unit_name("org.test.App@1") == "org_test_App-1");

    auto ls_path = utils::find_in_path("ls");
    assert(ls_path.has_value());

    std::cout << "test_types PASSED\n";
    return 0;
}
