// Enum names and parsers, and the string/path helpers the autostart code uses.
// Portable: runs on every platform.
#include "check.h"
#include "broseat/types.h"
#include "common/utils.h"

#include <string>

using namespace broseat;

static void test_enums() {
    CHECK(seat_backend_name(SeatBackendType::Auto) == "auto");
    CHECK(seat_backend_name(SeatBackendType::Libseat) == "libseat");
    CHECK(seat_backend_name(SeatBackendType::Logind) == "logind");
    CHECK(parse_seat_backend("auto") == SeatBackendType::Auto);
    CHECK(parse_seat_backend("libseat") == SeatBackendType::Libseat);
    CHECK(parse_seat_backend("logind") == SeatBackendType::Logind);
    CHECK(parse_seat_backend("unknown") == SeatBackendType::Auto);

    CHECK(device_type_name(DeviceType::DrmCard) == "drm-card");
    CHECK(device_type_name(DeviceType::DrmRender) == "drm-render");
    CHECK(device_type_name(DeviceType::Evdev) == "evdev");
    CHECK(device_type_name(DeviceType::Other) == "other");
    CHECK(detect_device_type("/dev/dri/card0") == DeviceType::DrmCard);
    CHECK(detect_device_type("/dev/dri/card1") == DeviceType::DrmCard);
    CHECK(detect_device_type("/dev/dri/renderD128") == DeviceType::DrmRender);
    CHECK(detect_device_type("/dev/input/event0") == DeviceType::Evdev);
    CHECK(detect_device_type("/dev/tty0") == DeviceType::Other);

    CHECK(session_state_name(SessionState::Active) == "active");
    CHECK(session_state_name(SessionState::Online) == "online");
    CHECK(session_state_name(SessionState::Closing) == "closing");
    CHECK(parse_session_state("active") == SessionState::Active);
    CHECK(parse_session_state("online") == SessionState::Online);
    CHECK(parse_session_state("closing") == SessionState::Closing);
    CHECK(parse_session_state("unknown") == SessionState::Unknown);

    CHECK(inhibit_mode_name(InhibitMode::Block) == "block");
    CHECK(inhibit_mode_name(InhibitMode::Delay) == "delay");
    CHECK(parse_inhibit_mode("block") == InhibitMode::Block);
    CHECK(parse_inhibit_mode("delay") == InhibitMode::Delay);

    CHECK(autostart_phase_name(AutostartPhase::EarlySetup) == "EarlySetup");
    CHECK(autostart_phase_name(AutostartPhase::Initialization) == "Initialization");
    CHECK(autostart_phase_name(AutostartPhase::Application) == "Application");
    CHECK(parse_autostart_phase("EarlySetup") == AutostartPhase::EarlySetup);
    CHECK(parse_autostart_phase("earlysetup") == AutostartPhase::EarlySetup);
    CHECK(parse_autostart_phase("Initialization") == AutostartPhase::Initialization);
    CHECK(parse_autostart_phase("Application") == AutostartPhase::Application);
}

static void test_utils() {
    CHECK(utils::trim("  hello world  \t\n") == "hello world");
    CHECK(utils::trim("") == "");

    auto parts = utils::split("foo;bar;baz;", ';');
    REQUIRE(parts.size() == 3);
    CHECK_EQ(parts[0], std::string("foo"));
    CHECK_EQ(parts[1], std::string("bar"));
    CHECK_EQ(parts[2], std::string("baz"));
    CHECK_EQ(utils::split("a;;b", ';', false).size(), size_t(3));

    CHECK(utils::iequals("GNOME", "gnome"));
    CHECK(utils::iequals("bRo", "BRo"));
    CHECK(!utils::iequals("KDE", "XFCE"));
    CHECK(utils::starts_with("prefix_test", "prefix_"));
    CHECK(!utils::starts_with("prefix_test", "other"));
    CHECK_EQ(utils::sanitize_unit_name("org.test.App@1"), std::string("org_test_App-1"));
    CHECK_EQ(utils::sanitize_unit_name(""), std::string("app"));

    // A program every machine of each kind has on PATH, and one nobody has.
#if defined(_WIN32)
    auto found = utils::find_in_path("cmd.exe");
#else
    auto found = utils::find_in_path("sh");
#endif
    CHECK(found.has_value());
    CHECK(!utils::find_in_path("broseat_no_such_binary_4711").has_value());

    CHECK(utils::set_env("BROSEAT_TEST_TYPES", "v1"));
    CHECK_EQ(utils::get_env("BROSEAT_TEST_TYPES"), std::string("v1"));
    CHECK_EQ(utils::get_env("BROSEAT_TEST_UNSET_4711", "dflt"), std::string("dflt"));
}

int main() {
    test_enums();
    test_utils();
    return bstest::finish("test_types");
}
