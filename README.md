# broseat

[![CI](https://github.com/wlejon/broseat/actions/workflows/ci.yml/badge.svg)](https://github.com/wlejon/broseat/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

Seat, session, and process lifecycle for Linux desktop environments in C++20: the
part of a desktop shell or Wayland compositor that interfaces with `logind`, `libseat`,
and the `systemd` user manager. Handles device access through the seat, VT switching,
session lock state, user manager environment and units, power/idle inhibitors,
idle tracking, and XDG autostart.

Part of the **[bro ecosystem](https://github.com/wlejon/bro/blob/main/docs/ecosystem.md)**.
On Linux, `broseat` links the sibling library **[brodbus](https://github.com/wlejon/brodbus)**
for D-Bus messaging with logind and systemd user services.

## Platform Support

Stated honestly, what was verified where:

broseat is a Linux library: the services it manages (seats, logind sessions,
inhibitors, systemd user units, XDG autostart launch) are native to Linux. It compiles
clean stubs on **Windows and macOS** without requiring Linux packages, allowing cross-platform
hosts to link it unconditionally:

- **Portable parts work everywhere**: Data types, thread-safe `EventQueue`, `.desktop` file
  parsing, autostart conditions evaluation, `Exec=` expansion, desktop file discovery with
  XDG directory precedence, and `IdleAggregator`.
- **Honest refusal stubs**: Every entry point requiring Linux system services returns clean,
  explicit errors rather than mock objects:
  - `Seat::create()`, `SessionManager::create()`, and `InhibitManager::create()` return `nullptr`
    and populate `*error`.
  - Environment and unit management functions return `false` with `*error`.
  - `AutostartManager::launch()` returns a failed `LaunchResult` explaining that process launching
    is unavailable on that platform.
- **No mock backends**: All public APIs operate against real system interfaces. There are no
  synthetic mock backends or dummy test setters.

| Area | Linux | Windows & macOS |
| :--- | :--- | :--- |
| **Seat & Devices** | `libseat` (seatd or logind backend), falling back to logind directly (`TakeControl`, `TakeDevice`, `PauseDevice`/`ResumeDevice`, `SwitchTo`) | Unavailable (`nullptr`) |
| **Session Tracking** | `org.freedesktop.login1.Session`: id, user, seat, VT, active state, `LockedHint`, `Lock`/`Unlock` signals | Unavailable (`nullptr`) |
| **User Manager** | `SetEnvironment`/`UnsetEnvironment` (falling back to `systemctl --user import-environment`), start/stop/restart/reset-failed units, `is_unit_active` | Unavailable (`false`) |
| **Inhibitors** | `org.freedesktop.login1.Manager.Inhibit` (block and delay) via RAII `InhibitorLock`, `ListInhibitors` | Unavailable (`nullptr`) |
| **Idle Aggregation** | `IdleAggregator` combining multiple idle timeouts | Same |
| **Autostart** | XDG discovery, conditions, launch as a transient service, `systemd-run --user --scope`, or fork/exec | Discovery and conditions; launch unavailable |

### Verified Configurations

- **Linux**: Ubuntu 24.04 LTS (x86_64), GCC 14, Clang 18, with `libsystemd-dev`, `libseat-dev`, and `brodbus`.
- **Windows**: Windows Server 2022 (x64), MSVC 2022 (v143), compiling portable models and unavailable stubs.
- **macOS**: macOS 15 Sequoia (arm64), Apple Clang 16, compiling portable models and unavailable stubs.

## API Overview

```
include/broseat/
  broseat.h       Umbrella header
  types.h         Enums, InhibitorInfo, AutostartEntry, LaunchResult, SessionInfo
  events.h        Event variant: SeatActiveChanged, SeatDevicePaused/Resumed,
                  SessionLockedChanged, SessionStateChanged, VtSwitched,
                  IdleStateChanged, InhibitorAdded/Removed, AutostartEntryLaunched
  event_queue.h   MessageQueue<T> / EventQueue (push, drain, wait_for, wake hook)
  seat.h          Seat (create, open_device, switch_vt, dispatch), SeatDevice (RAII fd)
  session.h       export_environment, unset_environment, start/stop/restart/reset_failed_unit,
                  is_unit_active, SessionManager
  inhibit.h       InhibitManager, InhibitorLock (RAII), IdleAggregator
  autostart.h     AutostartManager: search paths, parse, discover, should_autostart, launch
```

### Usage Example

```cpp
#include <broseat/broseat.h>
#include <iostream>

int main() {
    std::string err;

    // 1. Take seat control (requires a free VT or seatd permissions)
    auto seat = broseat::Seat::create({}, &err);
    if (!seat) {
        std::cerr << "Seat unavailable: " << err << "\n";
    } else {
        auto card = seat->open_device("/dev/dri/card0", &err);
    }

    // 2. Export environment to systemd user session
    broseat::export_environment({
        {"WAYLAND_DISPLAY", "wayland-1"},
        {"XDG_CURRENT_DESKTOP", "BRO"}
    }, &err);

    // 3. Start a graphical session target unit
    broseat::start_unit(std::string(broseat::kGraphicalSessionTarget), "replace", &err);

    // 4. Inhibit system idle while playing media
    auto inhibit = broseat::InhibitManager::create(&err);
    if (inhibit) {
        auto lock = inhibit->inhibit("idle", "media-player", "Playing video",
                                     broseat::InhibitMode::Block, &err);
    }

    // 5. Discover and launch autostart desktop applications
    broseat::AutostartFilter filter{.current_desktop = "BRO"};
    auto entries = broseat::AutostartManager::discover(
        broseat::AutostartManager::default_search_paths(), filter);

    for (const auto& res : broseat::AutostartManager::launch_all(entries)) {
        if (!res.success) {
            std::cerr << "Autostart failed for " << res.entry.name << ": " << res.error << "\n";
        }
    }
}
```

## Building

### Dependencies

On Linux, `broseat` requires **[brodbus](https://github.com/wlejon/brodbus)**. There are no submodules: brodbus (and bronze, for the JavaScript API) is a `bro_dependency()` pin in `CMakeLists.txt`, resolved through `cmake/bro_deps.cmake` in this order:
1. An existing `brodbus` target in the CMake project;
2. A working tree beside the top-level project (`../brodbus`), or `-DFETCHCONTENT_SOURCE_DIR_BRODBUS=<path>`;
3. The pinned commit, fetched from GitHub at configure, so a plain `git clone` builds.

### Consuming `broseat` in CMake

Downstream consumers link against the `broseat::broseat` alias target:

```cmake
add_subdirectory(path/to/broseat)   # or bro_dependency(broseat ...)
target_link_libraries(my_app PRIVATE broseat::broseat)
```

### Build Commands

#### Linux (GCC 12+ or Clang, Ninja)

```bash
# Debian / Ubuntu
sudo apt install cmake ninja-build pkg-config libsystemd-dev libseat-dev
# Arch Linux
sudo pacman -S cmake ninja pkgconf systemd-libs seatd

cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel 4
ctest --test-dir build-release --output-on-failure
```

#### Windows (MSVC 2022)

```powershell
cmake -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

#### macOS (Apple Clang 15+, Ninja)

```bash
cmake -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release --parallel 4
ctest --test-dir build-release --output-on-failure
```

### Build Options

- `-DBROSEAT_BUILD_TESTS=ON|OFF` (default ON when top-level): build test suites.
- `-DBROSEAT_COVERAGE=ON|OFF` (default OFF): instrument GCC/Clang with gcov for code coverage.
- `-DBROSEAT_ENABLE_API=ON|OFF` (default ON when top-level): build standalone Bronze JavaScript API (`broseat_api`; bronze from `../bronze` or the pinned commit).

## Tests

The test suite runs real system operations without test mocks. Failures are reported across all build types (no `assert()` reliance):

| Test | Platforms | Oracle & Coverage |
| :--- | :--- | :--- |
| `test_types` | Everywhere | Pure type transformations and enum stringification |
| `test_event_queue` | Everywhere | Thread-safe `MessageQueue` push, drain, timeout, and wake hooks |
| `test_autostart` | Everywhere (launches on Linux) | Desktop file parsing and conditions against temporary files; on Linux checks fork/exec exit code, transient systemd service, and `systemd-run --scope` execution |
| `test_idle` | Everywhere | `IdleAggregator` tick-based aggregation and state transitions |
| `test_unavailable` | Windows, macOS | Verifies that every Linux-specific entry point cleanly refuses with an explanatory error |
| `test_session` | Linux | Validates `systemctl --user show-environment`, checks active unit state, and queries `SessionManager` |
| `test_inhibit` | Linux | Queries `systemd-inhibit --list`, creates an inhibitor lock, verifies it appears in the system list, and verifies removal on release |
| `test_dbus` | Linux | Runs a private `dbus-daemon` session, matches D-Bus signals, and reads properties via `brodbus` |
| `test_seat` | Linux | Takes the seat when running from a session with seat access (a free VT or running `seatd`); verifies backend refusal reporting otherwise |
| `broseat_test_api` | Everywhere (with Bronze) | Standalone Bronze JavaScript API bindings with garbage collection stress testing |

### What Skips on CI and Why

- **`test_seat`**: Controlling a seat (`Seat::create`) requires either an active virtual terminal (VT) that is not already managed by another display server, or direct access to a `seatd` daemon socket. In headless CI containers or shared graphical sessions, `test_seat` checks that the refusal reason is reported honestly, and then exits with code `77` (Skip).
- **`test_session` / `test_inhibit`**: In minimal CI containers without an active `systemd --user` instance or `dbus-daemon`, session and inhibitor tests will skip when the user session bus cannot be contacted.

## License

[MIT](LICENSE)
