#pragma once

#include "embed/embed.h"
#include "broseat/autostart.h"
#include "broseat/inhibit.h"
#include "broseat/session.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace broseat::api {

namespace ev = bronze::embed;
using Value = bronze::Value;

Value ensureBroSeat();

void installSessionOnto(Value seatObj);
void installInhibitOnto(Value seatObj);
void installAutostartOnto(Value seatObj);

void drainSeatEvents();
void clearActiveInhibitors();

void addSeatEventListener(const std::string& event, Value callback);
void removeSeatEventListener(const std::string& event, Value callback);

std::shared_ptr<broseat::SessionManager> activeSessionManager();
std::shared_ptr<broseat::InhibitManager> activeInhibitManager();
std::vector<std::filesystem::path> activeAutostartSearchPaths();
broseat::AutostartFilter activeAutostartFilter();

} // namespace broseat::api
