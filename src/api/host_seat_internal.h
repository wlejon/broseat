#pragma once

#include "embed/embed.h"
#include "broseat/autostart.h"
#include "broseat/inhibit.h"
#include "broseat/session.h"

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace broseat::api {

namespace ev = bronze::embed;
using Value = bronze::Value;

Value ensureBroSeat();

void installSessionOnto(Value seatObj);
void installInhibitOnto(Value seatObj);
void installAutostartOnto(Value seatObj);
void installIdleOnto(Value seatObj);

void drainSeatEvents();
// Calls bro.seat's listeners for `event` (and its on<event> property).
void dispatchSeatEvent(std::string_view event, Value payload);
// Whether this process holds an inhibitor through bro.seat.inhibit whose
// type list (colon-separated, logind style) names `what`.
bool hasLocalInhibitor(const std::string& what);
// Drops the idle timer (realm teardown).
void resetIdleTimer();
void clearActiveInhibitors();

void addSeatEventListener(const std::string& event, Value callback);
void removeSeatEventListener(const std::string& event, Value callback);

std::shared_ptr<broseat::SessionManager> activeSessionManager();
std::shared_ptr<broseat::InhibitManager> activeInhibitManager();
std::vector<std::filesystem::path> activeAutostartSearchPaths();
broseat::AutostartFilter activeAutostartFilter();

} // namespace broseat::api
