// Off Linux every seat, session, inhibitor, systemd and launch entry point
// must refuse with a reason instead of pretending: factories return nullptr
// with an error, calls return false with an error, launches fail with one.
// Built on Windows and macOS only.
#include "check.h"
#include "broseat/autostart.h"
#include "broseat/inhibit.h"
#include "broseat/seat.h"
#include "broseat/session.h"

#include <string>

using namespace broseat;

static void expect_reason(bool ok, const std::string& err, const char* what) {
    CHECK(!ok);
    CHECK(!err.empty());
    std::printf("%s: %s\n", what, err.c_str());
}

int main() {
    std::string err;
    CHECK(Seat::create({}, &err) == nullptr);
    expect_reason(false, err, "Seat::create");
    err.clear();
    CHECK(SessionManager::create(&err) == nullptr);
    expect_reason(false, err, "SessionManager::create");
    err.clear();
    CHECK(InhibitManager::create(&err) == nullptr);
    expect_reason(false, err, "InhibitManager::create");
    CHECK(Seat::create({}, nullptr) == nullptr);

    err.clear();
    const std::vector<std::pair<std::string, std::string>> vars{{"BROSEAT_X", "1"}};
    expect_reason(export_environment(vars, &err), err, "export_environment");
    err.clear();
    expect_reason(export_environment(std::vector<std::string>{"BROSEAT_X"}, &err), err, "export_environment");
    err.clear();
    expect_reason(unset_environment({"BROSEAT_X"}, &err), err, "unset_environment");
    err.clear();
    expect_reason(start_unit("x.service", "replace", &err), err, "start_unit");
    err.clear();
    expect_reason(stop_unit("x.service", "replace", &err), err, "stop_unit");
    err.clear();
    expect_reason(restart_unit("x.service", "replace", &err), err, "restart_unit");
    err.clear();
    expect_reason(reset_failed_unit("x.service", &err), err, "reset_failed_unit");
    err.clear();
    expect_reason(is_unit_active("x.service", &err), err, "is_unit_active");

    AutostartEntry e;
    e.id = "x";
    e.exec = "true";
    for (LaunchMode m : {LaunchMode::Auto, LaunchMode::ForkExec, LaunchMode::SystemdRun, LaunchMode::SystemdTransient}) {
        LaunchResult r = AutostartManager::launch(e, m);
        CHECK(!r.success);
        CHECK_EQ(r.pid, 0u);
        CHECK(!r.error.empty());
    }
    auto all = AutostartManager::launch_all({e, e});
    CHECK_EQ(all.size(), size_t(2));
    for (auto& r : all) CHECK(!r.success);

    // The RAII handles stay inert: nothing to close.
    InhibitorLock lock;
    CHECK(!lock.is_held());
    lock.release();
    SeatDevice dev;
    CHECK(!dev.is_valid());
    dev.close();

    return bstest::finish("test_unavailable");
}
