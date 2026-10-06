// The sd-bus wrapper: a private dbus-daemon for connection, match and signal
// delivery (signals sent with dbus-send), and property reads from the real
// system and user managers compared with busctl. Linux.
#include "check.h"
#include "run.h"
#include "dbus/bus.h"

#include <chrono>
#include <csignal>
#include <cstring>
#include <string>
#include <sys/wait.h>
#include <unistd.h>

using namespace broseat::dbus;

namespace {

// dbus-daemon --session on a private socket, killed on scope exit.
struct PrivateBus {
    pid_t pid = -1;
    std::string address;

    PrivateBus() {
        int fds[2];
        if (pipe(fds) != 0) return;
        pid = fork();
        if (pid == 0) {
            dup2(fds[1], STDOUT_FILENO);
            close(fds[0]);
            close(fds[1]);
            execlp("dbus-daemon", "dbus-daemon", "--session", "--nofork", "--nopidfile",
                   "--print-address=1", static_cast<char*>(nullptr));
            _exit(127);
        }
        close(fds[1]);
        char buf[512];
        ssize_t n = read(fds[0], buf, sizeof buf - 1);
        close(fds[0]);
        if (n > 0) {
            buf[n] = 0;
            address = bstest::trimmed(buf);
        }
    }
    ~PrivateBus() {
        if (pid > 0) {
            kill(pid, SIGTERM);
            waitpid(pid, nullptr, 0);
        }
    }
    bool ok() const { return !address.empty(); }
};

int g_parts_run = 0;

void test_private_bus() {
    PrivateBus daemon;
    if (!daemon.ok()) {
        std::printf("Note: dbus-daemon could not start; the private-bus checks did not run\n");
        return;
    }
    ++g_parts_run;
    std::string err;
    auto bus = Bus::open_address(daemon.address, &err);
    REQUIRE(bus);
    CHECK(bus->is_valid());
    CHECK(bus->get_fd() >= 0);

    int hits = 0;
    std::string seen_arg;
    auto slot = bus->add_match("type='signal',interface='org.bro.Test',member='Ping'",
                               [&](sd_bus_message* m) {
                                   const char* s = nullptr;
                                   if (sd_bus_message_read(m, "s", &s) >= 0 && s) seen_arg = s;
                                   ++hits;
                               },
                               &err);
    REQUIRE(slot.is_valid());
    // Flush the AddMatch round trip before the signal is sent.
    while (bus->process() > 0) {
    }

    auto sent = bstest::run("DBUS_SESSION_BUS_ADDRESS='" + daemon.address +
                            "' dbus-send --session --type=signal / org.bro.Test.Ping string:hello");
    if (!sent.ok()) {
        std::printf("Note: dbus-send is not available; signal delivery was not checked\n");
    } else {
        bool got = bstest::wait_until(
            [&] {
                bus->wait(50000);
                while (bus->process() > 0) {
                }
                return hits > 0;
            },
            std::chrono::seconds(5));
        CHECK(got);
        CHECK_EQ(seen_arg, std::string("hello"));
    }

    // A reset slot no longer delivers.
    slot.reset();
    CHECK(!slot.is_valid());
    int before = hits;
    bstest::run("DBUS_SESSION_BUS_ADDRESS='" + daemon.address +
                "' dbus-send --session --type=signal / org.bro.Test.Ping string:again");
    for (int i = 0; i < 10; ++i) {
        bus->wait(20000);
        while (bus->process() > 0) {
        }
    }
    CHECK_EQ(hits, before);

    // Moves transfer the connection.
    Bus moved = std::move(*bus);
    CHECK(moved.is_valid());
    CHECK(!bus->is_valid());

    // A property read of something nobody owns fails with a reason.
    std::string out;
    err.clear();
    CHECK(!moved.get_property_string("org.bro.Nobody", "/", "org.bro.Nobody", "X", &out, &err));
    CHECK(!err.empty());
}

void test_system_properties() {
    std::string err;
    auto sys = Bus::open_system(&err);
    if (!sys) {
        std::printf("Note: no system bus: %s\n", err.c_str());
        return;
    }
    auto oracle = bstest::run("busctl get-property org.freedesktop.login1 /org/freedesktop/login1 "
                              "org.freedesktop.login1.Manager NAutoVTs");
    if (!oracle.ok()) {
        std::printf("Note: logind is not on the system bus; property reads were not compared\n");
        return;
    }
    ++g_parts_run;
    uint32_t n = 0;
    REQUIRE(sys->get_property_uint32("org.freedesktop.login1", "/org/freedesktop/login1",
                                     "org.freedesktop.login1.Manager", "NAutoVTs", &n, &err));
    CHECK_EQ("u " + std::to_string(n), bstest::trimmed(oracle.out));

    bool docked = false;
    auto docked_oracle = bstest::run("busctl get-property org.freedesktop.login1 /org/freedesktop/login1 "
                                     "org.freedesktop.login1.Manager Docked");
    REQUIRE(sys->get_property_bool("org.freedesktop.login1", "/org/freedesktop/login1",
                                   "org.freedesktop.login1.Manager", "Docked", &docked, &err));
    CHECK_EQ(std::string(docked ? "b true" : "b false"), bstest::trimmed(docked_oracle.out));
}

void test_user_properties() {
    std::string err;
    auto user = Bus::open_user(&err);
    if (!user) {
        std::printf("Note: no user bus: %s\n", err.c_str());
        return;
    }
    std::string version;
    REQUIRE(user->get_property_string("org.freedesktop.systemd1", "/org/freedesktop/systemd1",
                                      "org.freedesktop.systemd1.Manager", "Version", &version, &err));
    auto oracle = bstest::run("busctl --user get-property org.freedesktop.systemd1 /org/freedesktop/systemd1 "
                              "org.freedesktop.systemd1.Manager Version");
    if (oracle.ok()) CHECK_EQ("s \"" + version + "\"", bstest::trimmed(oracle.out));
    ++g_parts_run;
}

}  // namespace

int main() {
    test_private_bus();
    test_system_properties();
    test_user_properties();
    if (g_parts_run == 0 && bstest::failures() == 0) {
        bstest::skip("test_dbus", "no dbus-daemon, no system bus with logind and no user bus on this machine");
    }
    return bstest::finish("test_dbus");
}
