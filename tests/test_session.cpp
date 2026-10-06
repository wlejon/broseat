// The systemd user manager and the logind session, against the system's own
// tools: environment export checked with `systemctl --user show-environment`,
// unit state with `systemctl --user is-active`, a transient unit stopped
// through the library, and SessionManager's view of the caller's session with
// `loginctl show-session`. Linux.
#include "check.h"
#include "run.h"
#include "broseat/session.h"
#include "common/utils.h"

#include <chrono>
#include <string>
#include <unistd.h>

using namespace broseat;

namespace {

bool env_has(const std::string& assignment) {
    std::string out = bstest::run("systemctl --user show-environment").out;
    return ("\n" + out).find("\n" + assignment + "\n") != std::string::npos;
}

std::string is_active(const std::string& unit) {
    return bstest::trimmed(bstest::run("systemctl --user is-active '" + unit + "'").out);
}

void test_environment(const std::string& tag) {
    std::string err;
    const std::string name = "BROSEAT_TEST_" + tag;
    const std::string value = "bar " + tag;
    REQUIRE(export_environment({{name, value}}, &err));
    CHECK_EQ(utils::get_env(name.c_str()), value);
    CHECK(env_has(name + "=" + value) || env_has(name + "=$'" + value + "'") ||
          env_has(name + "=\"" + value + "\""));

    // Export by name takes the process environment's value.
    const std::string name2 = name + "_B";
    REQUIRE(utils::set_env(name2, "v2"));
    REQUIRE(export_environment(std::vector<std::string>{name2}, &err));
    CHECK(env_has(name2 + "=v2"));

    REQUIRE(unset_environment({name, name2}, &err));
    CHECK(bstest::run("systemctl --user show-environment").out.find(name) == std::string::npos);
}

void test_units(const std::string& tag) {
    std::string err;
    CHECK_EQ(is_unit_active("init.scope", &err), is_active("init.scope") == "active");
    const std::string missing = "broseat-no-such-unit-" + tag + ".service";
    CHECK(!is_unit_active(missing, &err));
    err.clear();
    CHECK(!start_unit(missing, "replace", &err));
    CHECK(!err.empty());

    // A transient unit made with systemd-run, stopped and restarted through the library.
    const std::string unit = "broseat-test-" + tag + ".service";
    auto made = bstest::run("systemd-run --user --quiet --unit='" + unit + "' --property=RemainAfterExit=no sleep 120");
    if (!made.ok()) {
        std::printf("Note: systemd-run --user could not create a transient unit; stop/restart were not checked\n");
        return;
    }
    CHECK(bstest::wait_until([&] { return is_active(unit) == "active"; }, std::chrono::seconds(10)));
    CHECK(is_unit_active(unit, &err));
    err.clear();
    CHECK(restart_unit(unit, "replace", &err));
    CHECK(bstest::wait_until([&] { return is_active(unit) == "active"; }, std::chrono::seconds(10)));
    err.clear();
    CHECK(stop_unit(unit, "replace", &err));
    CHECK(bstest::wait_until([&] { return is_active(unit) != "active"; }, std::chrono::seconds(10)));
    CHECK(!is_unit_active(unit, &err));
    reset_failed_unit(unit, &err);
}

void test_session_manager() {
    std::string err;
    auto mgr = SessionManager::create(&err);
    if (!mgr) {
        std::printf("Note: SessionManager not created (no system bus): %s\n", err.c_str());
        return;
    }
    CHECK(mgr->poll_fd() >= 0);
    SessionInfo info = mgr->current_session_info();

    std::string sid = utils::get_env("XDG_SESSION_ID");
    auto show = bstest::run("loginctl show-session " + (sid.empty() ? std::string("self") : sid) +
                            " -p Id -p Name -p VTNr -p Active -p State -p LockedHint -p Seat");
    if (!show.ok()) {
        // Not in a logind session (a CI runner's service): nothing to describe.
        std::printf("Note: the test runs outside any logind session; only the empty answer was checked\n");
        CHECK(info.id.empty());
        return;
    }
    CHECK_EQ(info.id, bstest::prop(show.out, "Id"));
    CHECK_EQ(info.user, bstest::prop(show.out, "Name"));
    CHECK_EQ(std::to_string(info.vt), bstest::prop(show.out, "VTNr"));
    CHECK_EQ(info.active, bstest::prop(show.out, "Active") == "yes");
    CHECK_EQ(info.locked, bstest::prop(show.out, "LockedHint") == "yes");
    CHECK_EQ(std::string(session_state_name(info.state)), bstest::prop(show.out, "State"));
    CHECK_EQ(info.seat, bstest::prop(show.out, "Seat"));
    CHECK_EQ(mgr->is_locked(), info.locked);
    CHECK(mgr->dispatch(0) >= 0);
}

}  // namespace

int main() {
    if (!bstest::run("systemctl --user show-environment").ok()) {
        bstest::skip("test_session", "no systemd user manager reachable (systemctl --user fails)");
    }
    const std::string tag = std::to_string(getpid());
    test_environment(tag);
    test_units(tag);
    test_session_manager();
    return bstest::finish("test_session");
}
