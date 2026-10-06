#pragma once

#include "broseat/autostart.h"
#include "broseat/inhibit.h"
#include "broseat/session.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace broseat::api {

/// Mounts `bro.seat` in the current Bronze realm.
void installSeat();

/// Pumps async session state changes and dispatches event listeners.
void tickSeatAsync();

/// Cleans up active inhibitors cleanly.
void shutdownSeatAsync();

/// Sets the session manager used by the API (defaults to SessionManager::create()).
void setSessionManager(std::shared_ptr<broseat::SessionManager> mgr);

/// Gets the session manager currently used by the API.
std::shared_ptr<broseat::SessionManager> getSessionManager();

/// Sets the inhibit manager used by the API (defaults to InhibitManager::create()).
void setInhibitManager(std::shared_ptr<broseat::InhibitManager> mgr);

/// Gets the inhibit manager currently used by the API.
std::shared_ptr<broseat::InhibitManager> getInhibitManager();

/// Sets the autostart search paths used by the API.
void setAutostartSearchPaths(std::vector<std::filesystem::path> paths);

/// Gets the autostart search paths currently used by the API.
std::vector<std::filesystem::path> getAutostartSearchPaths();

/// Sets the autostart filter used by the API.
void setAutostartFilter(broseat::AutostartFilter filter);

/// Gets the autostart filter currently used by the API.
broseat::AutostartFilter getAutostartFilter();

} // namespace broseat::api

using broseat::api::installSeat;
using broseat::api::tickSeatAsync;
using broseat::api::shutdownSeatAsync;
using broseat::api::setSessionManager;
using broseat::api::getSessionManager;
using broseat::api::setInhibitManager;
using broseat::api::getInhibitManager;
using broseat::api::setAutostartSearchPaths;
using broseat::api::getAutostartSearchPaths;
using broseat::api::setAutostartFilter;
using broseat::api::getAutostartFilter;
