# broseat

`broseat` is a standalone Linux Session, Seat, and Process Lifecycle management library written in C++20.
It provides modern RAII interfaces and an asynchronous event-queue architecture for Wayland compositors and desktop components without requiring any desktop shell or display server dependencies.

## Key Capabilities

1. **Seat & Device Access**:
   - `libseat` integration (`libseat.h`) with automatic fallback to direct `systemd-logind` D-Bus management (`org.freedesktop.login1.Manager` / `Session`).
   - In-memory mock seat backend for deterministic testing in headless/CI environments.
   - Opening and closing restricted character devices (DRM card/render nodes, evdev input nodes) with RAII `SeatDevice` handles.
   - Virtual terminal switching (`switch_vt()`), session active/inactive switch events, and device pause/resume handshake protocol.

2. **Systemd User-Session Integration**:
   - Environment export to the systemd user instance (`SetEnvironment` / `ImportEnvironment` via D-Bus with fallback to `systemctl --user import-environment`).
   - Systemd target and unit lifecycle management (`graphical-session.target`, `graphical-session-pre.target`, compositor session units).
   - Session lifecycle monitoring via `org.freedesktop.login1.Session` (`Lock` / `Unlock` signals, `Active` state tracking).

3. **XDG Autostart**:
   - Discovery and parsing of `.desktop` files in `$XDG_CONFIG_HOME/autostart`, `/etc/xdg/autostart`, and custom search paths with directory priority overriding.
   - Condition evaluation conforming to the XDG Desktop Entry Specification (`OnlyShowIn`, `NotShowIn`, `Hidden`, `TryExec`, `Exec`, `AutostartCondition`, startup phase).
   - Launching autostart entries as transient systemd services (`org.freedesktop.systemd1.Manager.StartTransientUnit`), systemd scopes (`systemd-run --user --scope`), or clean `fork()`/`exec()` fallback.

4. **Idle & Inhibit**:
   - Inhibitor management via `org.freedesktop.login1.Manager.Inhibit` (idle, sleep, shutdown) with RAII `InhibitorLock`.
   - Querying active system inhibitors.
   - `IdleAggregator` for tracking user activity, detecting idle timeouts, and publishing idle state changes while honoring active inhibitors.

5. **Event-Queue Architecture**:
   - Multi-producer, single-consumer `MessageQueue<Event>` / `EventQueue` model.
   - Type-safe `std::variant` events (`SeatActiveChanged`, `SeatDevicePaused`, `SeatDeviceResumed`, `SessionLockedChanged`, `SessionStateChanged`, `VtSwitched`, `IdleStateChanged`, `InhibitorAdded`, `InhibitorRemoved`, `AutostartEntryLaunched`).

---

## Directory Structure

```
broseat/
├── CMakeLists.txt
├── README.md
├── include/
│   └── broseat/
│       ├── autostart.h       # XDG Autostart discovery, parsing, and execution
│       ├── broseat.h         # Umbrella header
│       ├── event_queue.h     # MessageQueue<T> and EventQueue
│       ├── events.h          # std::variant Event vocabulary
│       ├── inhibit.h         # InhibitorLock, InhibitManager, and IdleAggregator
│       ├── seat.h            # Seat, SeatDevice, SeatConfig
│       ├── session.h         # Systemd user environment, units, and SessionManager
│       └── types.h           # Core types, enums, converters
├── src/
│   ├── autostart/            # Desktop file parsing, condition evaluation, launching
│   ├── common/               # String helpers, path utilities, type mappings
│   ├── dbus/                 # RAII sd-bus connection, message handling, and matching
│   ├── inhibit/              # Logind inhibitor client and idle state tracking
│   ├── seat/                 # Libseat, Logind, and Mock seat backend implementations
│   └── session/              # Environment export, unit management, and session tracker
└── tests/
    ├── CMakeLists.txt
    ├── test_autostart.cpp
    ├── test_dbus.cpp
    ├── test_event_queue.cpp
    ├── test_idle_inhibit.cpp
    ├── test_seat.cpp
    ├── test_session.cpp
    └── test_types.cpp
```

---

## Building & Testing

### Requirements
- C++20 compiler (GCC 11+ or Clang 13+)
- CMake 3.24+
- `pkg-config`
- `libsystemd` (sd-bus)
- `libseat`

### Build

```bash
cmake -B build -S .
cmake --build build -j 2
```

### Run Tests

```bash
ctest --test-dir build --output-on-failure
```

Tests run serially and cleanly in headless environments. Real hardware access tests gracefully skip (exit code 77) if running without seat privileges or under restricted CI environments.

---

## Example Usage

### 1. Opening a Seat and Device

```cpp
#include <broseat/seat.h>
#include <iostream>

using namespace broseat;

int main() {
    SeatConfig config{ .backend = SeatBackendType::Auto };
    std::string err;
    auto seat = Seat::create(config, &err);
    if (!seat) {
        std::cerr << "Failed to open seat: " << err << "\n";
        return 1;
    }

    std::cout << "Seat backend: " << seat_backend_name(seat->backend_type()) << "\n";
    std::cout << "Seat name: " << seat->seat_name() << "\n";

    auto dev = seat->open_device("/dev/dri/card0", &err);
    if (dev) {
        std::cout << "Opened card0 with fd=" << dev->fd() << "\n";
    }
    return 0;
}
```

### 2. Exporting Environment to Systemd User Session

```cpp
#include <broseat/session.h>

broseat::export_environment({
    {"WAYLAND_DISPLAY", "wayland-0"},
    {"XDG_CURRENT_DESKTOP", "BRO"}
});

broseat::start_unit("graphical-session.target");
```

### 3. Running Autostart Applications

```cpp
#include <broseat/autostart.h>

using namespace broseat;

AutostartFilter filter{ .current_desktop = "BRO" };
auto entries = AutostartManager::discover(AutostartManager::default_search_paths(), filter);
auto results = AutostartManager::launch_all(entries, LaunchMode::Auto);
```

### 4. Holding an Inhibitor

```cpp
#include <broseat/inhibit.h>

using namespace broseat;

auto mgr = InhibitManager::create();
if (mgr) {
    auto lock = mgr->inhibit("idle", "my-player", "Playing video", InhibitMode::Block);
    // lock holds the inhibitor until destroyed or lock->release() is called
}
```
