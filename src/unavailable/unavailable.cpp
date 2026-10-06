// broseat off Linux. Seats, logind sessions and inhibitors, the systemd user
// manager and XDG autostart launching are Linux session services: Windows and
// macOS have none of them. The portable parts (types, the event queue, desktop
// file parsing and condition evaluation, IdleAggregator) are built everywhere;
// every entry point that would talk to a Linux service lands here and says so.
// Factories return nullptr with the reason, calls return false / a failed
// LaunchResult with the reason. Nothing pretends to work.
#include "autostart/autostart_launcher.h"
#include "broseat/inhibit.h"
#include "broseat/seat.h"
#include "broseat/session.h"

#include <string>

namespace broseat {

namespace {

std::string reason(const char* what) {
#if defined(_WIN32)
    const char* os = "Windows";
#elif defined(__APPLE__)
    const char* os = "macOS";
#else
    const char* os = "this platform";
#endif
    return std::string(what) + " is a Linux session service (logind / systemd); " + os + " has none";
}

bool fail(std::string* error, const char* what) {
    if (error) *error = reason(what);
    return false;
}

}  // namespace

std::unique_ptr<Seat> Seat::create(const SeatConfig&, std::string* error) {
    fail(error, "Seat (libseat / logind device access)");
    return nullptr;
}

std::unique_ptr<SessionManager> SessionManager::create(std::string* error) {
    fail(error, "SessionManager (org.freedesktop.login1.Session)");
    return nullptr;
}

std::unique_ptr<InhibitManager> InhibitManager::create(std::string* error) {
    fail(error, "InhibitManager (logind inhibitors)");
    return nullptr;
}

bool export_environment(const std::vector<std::pair<std::string, std::string>>&, std::string* error) {
    return fail(error, "export_environment (the systemd user manager's environment)");
}

bool export_environment(const std::vector<std::string>&, std::string* error) {
    return fail(error, "export_environment (the systemd user manager's environment)");
}

bool unset_environment(const std::vector<std::string>&, std::string* error) {
    return fail(error, "unset_environment (the systemd user manager's environment)");
}

bool start_unit(const std::string&, const std::string&, std::string* error) {
    return fail(error, "start_unit (systemd user units)");
}

bool stop_unit(const std::string&, const std::string&, std::string* error) {
    return fail(error, "stop_unit (systemd user units)");
}

bool restart_unit(const std::string&, const std::string&, std::string* error) {
    return fail(error, "restart_unit (systemd user units)");
}

bool reset_failed_unit(const std::string&, std::string* error) {
    return fail(error, "reset_failed_unit (systemd user units)");
}

bool is_unit_active(const std::string&, std::string* error) {
    return fail(error, "is_unit_active (systemd user units)");
}

LaunchResult launch_entry(const AutostartEntry&, LaunchMode) {
    LaunchResult r;
    r.success = false;
    r.error = reason("XDG autostart launching");
    return r;
}

}  // namespace broseat
