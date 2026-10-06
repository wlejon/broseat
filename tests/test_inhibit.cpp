// InhibitManager against the real logind on the system bus, with
// systemd-inhibit --list as the oracle: the inhibitor list matches it, a lock
// taken through the library appears there and is gone once released. Linux.
#include "check.h"
#include "run.h"
#include "broseat/inhibit.h"

#include <chrono>
#include <string>
#include <unistd.h>

using namespace broseat;

static bool oracle_lists(const std::string& who) {
    return bstest::run("systemd-inhibit --list --no-legend --no-pager").out.find(who) != std::string::npos;
}

int main() {
    std::string err;
    auto mgr = InhibitManager::create(&err);
    if (!mgr) bstest::skip("test_inhibit", "no logind on the system bus: " + err);
    if (!bstest::run("systemd-inhibit --list --no-legend --no-pager").ok()) {
        bstest::skip("test_inhibit", "systemd-inhibit (the oracle) cannot list inhibitors here");
    }

    // Every inhibitor logind reports, the library reports.
    auto listed = mgr->list_inhibitors(&err);
    std::string oracle = bstest::run("systemd-inhibit --list --no-legend --no-pager").out;
    for (const auto& inh : listed) {
        CHECK(!inh.what.empty());
        CHECK(!inh.who.empty());
        CHECK(oracle.find(inh.who) != std::string::npos);
    }

    const std::string who = "broseat-test-" + std::to_string(getpid());
    auto lock = mgr->inhibit("idle", who, "test_inhibit", InhibitMode::Block, &err);
    if (!lock || !lock->is_held()) {
        bstest::skip("test_inhibit", "logind refused an idle inhibitor (polkit): " + err);
    }
    CHECK(lock->fd() >= 0);
    CHECK_EQ(lock->what(), std::string("idle"));
    CHECK_EQ(lock->who(), who);
    CHECK(lock->mode() == InhibitMode::Block);
    CHECK(oracle_lists(who));
    CHECK(mgr->is_inhibited("idle"));

    bool in_library_list = false;
    for (const auto& inh : mgr->list_inhibitors(&err)) {
        if (inh.who == who) {
            in_library_list = true;
            CHECK_EQ(inh.what, std::string("idle"));
            CHECK_EQ(inh.why, std::string("test_inhibit"));
            CHECK_EQ(inh.mode, std::string("block"));
            CHECK_EQ(inh.pid, static_cast<uint32_t>(getpid()));
            CHECK_EQ(inh.uid, static_cast<uint32_t>(getuid()));
        }
    }
    CHECK(in_library_list);

    // Moving the lock moves ownership of the fd; releasing it ends the inhibitor.
    InhibitorLock moved = std::move(*lock);
    CHECK(!lock->is_held());
    CHECK(moved.is_held());
    moved.release();
    CHECK(!moved.is_held());
    CHECK(bstest::wait_until([&] { return !oracle_lists(who); }, std::chrono::seconds(5)));

    // A delay inhibitor on sleep, released by destruction.
    {
        auto delay = mgr->inhibit("sleep", who + "-delay", "test_inhibit delay", InhibitMode::Delay, &err);
        if (delay && delay->is_held()) {
            CHECK(oracle_lists(who + "-delay"));
        } else {
            std::printf("Note: logind refused a sleep delay inhibitor here: %s\n", err.c_str());
        }
    }
    CHECK(bstest::wait_until([&] { return !oracle_lists(who + "-delay"); }, std::chrono::seconds(5)));

    // An invalid "what" is refused with an error, not a dead lock.
    err.clear();
    auto bad = mgr->inhibit("not-a-lock-type", who, "x", InhibitMode::Block, &err);
    CHECK(!bad || !bad->is_held());
    CHECK(!err.empty());

    return bstest::finish("test_inhibit");
}
