# broseat

[![CI](https://github.com/wlejon/broseat/actions/workflows/ci.yml/badge.svg)](https://github.com/wlejon/broseat/actions/workflows/ci.yml)

Seat, session and process lifecycle for a Linux desktop session, in C++20: the
part of a desktop shell or Wayland compositor that talks to logind, libseat
and the systemd user manager. Device access through the seat, VT switching,
session lock state, the user manager's environment and units, inhibitors,
idle tracking and XDG autostart. A standalone library with its own CMake and
ctest: no dependency on bro or bronze, no siblings, nothing vendored.

## Platform support

broseat is a Linux library: everything it manages (seats, logind sessions and
inhibitors, systemd user units, XDG autostart) exists only there. It still
configures and builds on **Windows and macOS** with no Linux packages, so a
cross-platform host can link it unconditionally:

- The portable parts work everywhere: the types and their names,
  `EventQueue`, `.desktop` parsing, the autostart conditions, `Exec=`
  expansion, discovery with directory priority, and `IdleAggregator`.
- Every entry point that needs a Linux service refuses with the reason:
  `Seat::create`, `SessionManager::create` and `InhibitManager::create`
  return nullptr and fill `*error`; the environment and unit functions return
  false with `*error`; `AutostartManager::launch` returns a failed
  `LaunchResult` whose `error` says why. Nothing returns an object that only
  looks like it works.

| Area | Linux | Windows, macOS |
|------|-------|----------------|
| Seat and devices | libseat (seatd or its logind backend), else logind directly (`TakeControl`, `TakeDevice`, `PauseDevice`/`ResumeDevice`, `SwitchTo`) | unavailable |
| Session | `org.freedesktop.login1.Session`: id, user, seat, VT, active, state, `LockedHint`, `Lock`/`Unlock` signals | unavailable |
| User manager | `SetEnvironment`/`UnsetEnvironment` (falling back to `systemctl --user import-environment`), start/stop/restart/reset-failed, `is_unit_active` | unavailable |
| Inhibitors | `org.freedesktop.login1.Manager.Inhibit` (block and delay) as RAII `InhibitorLock`, `ListInhibitors` | unavailable |
| Idle | `IdleAggregator` | same |
| Autostart | discovery, conditions, launch as a transient service, a `systemd-run --user --scope`, or fork/exec | discovery and conditions; launch unavailable |

## Model

Like the other bro system libraries, each manager owns its connection and
pushes value snapshots (`std::variant` events in `events.h`) into an
`EventQueue`; the host drains it on its own loop. Managers that talk to D-Bus
expose `poll_fd()` and `dispatch()` so the host can fold them into its own
poll loop. There are no mocks or test setters in the public API.

```
include/broseat/
  broseat.h       umbrella header
  types.h         enums, InhibitorInfo, AutostartEntry, LaunchResult, SessionInfo
  events.h        Event: SeatActiveChanged, SeatDevicePaused/Resumed, SessionLockedChanged,
                  SessionStateChanged, VtSwitched, IdleStateChanged, InhibitorAdded/Removed,
                  AutostartEntryLaunched
  event_queue.h   MessageQueue<T> / EventQueue (push, drain, wait_for, wake hook)
  seat.h          Seat (create, open_device, switch_vt, dispatch), SeatDevice (RAII fd)
  session.h       export_environment, unset_environment, start/stop/restart/reset_failed_unit,
                  is_unit_active, SessionManager
  inhibit.h       InhibitManager, InhibitorLock (RAII), IdleAggregator
  autostart.h     AutostartManager: search paths, parse, discover, should_autostart, launch
```

```cpp
#include <broseat/broseat.h>

std::string err;
auto seat = broseat::Seat::create({}, &err);        // libseat, then logind
if (!seat) { /* err says why: no seat, already controlled, not Linux */ }
auto card = seat->open_device("/dev/dri/card0", &err);

broseat::export_environment({{"WAYLAND_DISPLAY", "wayland-1"}, {"XDG_CURRENT_DESKTOP", "BRO"}}, &err);
broseat::start_unit(std::string(broseat::kGraphicalSessionTarget), "replace", &err);

auto inhibit = broseat::InhibitManager::create(&err);
auto lock = inhibit->inhibit("idle", "my-player", "Playing video", broseat::InhibitMode::Block, &err);

broseat::AutostartFilter filter{.current_desktop = "BRO"};
auto entries = broseat::AutostartManager::discover(broseat::AutostartManager::default_search_paths(), filter);
for (auto& r : broseat::AutostartManager::launch_all(entries))
    if (!r.success) std::fprintf(stderr, "%s\n", r.error.c_str());
```

A seat can be controlled only from a session that is on a seat and that no
other compositor controls (a free VT, or a seatd the user may use). From an
ssh login or a service, `Seat::create` refuses and says the session is not on
a seat.

## Building

Linux needs `pkg-config`, `libsystemd` (sd-bus) and `libseat`
(`libsystemd-dev libseat-dev` on Debian/Ubuntu, `systemd-libs seatd` on Arch).
Windows and macOS need nothing beyond the compiler.

```bash
# Linux / macOS
cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
ctest --test-dir build-release --output-on-failure

# Windows (MSVC, Visual Studio generator)
cmake -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

`-DBROSEAT_COVERAGE=ON` instruments a GCC/Clang build for gcov.

## Tests

Real ctests: no `assert()` (`tests/check.h` counts failures in every
configuration). Exit 77 is a skip, used only when a service or privilege is
absent, and the test prints the reason. Anything that writes cleans up after
itself (a temporary directory, a transient unit, environment variables it
added).

| Test | Where | Oracle |
|------|-------|--------|
| test_types, test_event_queue, test_idle | everywhere | pure logic |
| test_autostart | everywhere; launches on Linux | parsing and conditions on files it writes; on Linux fork/exec (the child's exit status and working directory), a transient service and a `systemd-run` scope checked with `systemctl --user is-active` / `show` and a marker file the command writes |
| test_unavailable | Windows, macOS | every Linux entry point refuses with a reason |
| test_session | Linux | `systemctl --user show-environment` / `is-active`, a `systemd-run --user` unit stopped and restarted through the library, `loginctl show-session` against `SessionManager` |
| test_inhibit | Linux | `systemd-inhibit --list`: the library's list matches, a lock appears and disappears with release |
| test_dbus | Linux | a private `dbus-daemon` with `dbus-send` signals for matches; `busctl get-property` on logind and the user manager |
| test_seat | Linux | takes the seat when the session can (a free VT); elsewhere it checks each backend's refusal names a reason, then skips |
