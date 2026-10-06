#include "api.h"
#include "host_seat_internal.h"
#include "broseat/autostart.h"
#include "broseat/inhibit.h"
#include "broseat/session.h"

#include <mutex>
#include <optional>

namespace broseat::api {

namespace {

std::mutex g_services_mu;

std::shared_ptr<broseat::SessionManager> g_custom_session_manager;
std::shared_ptr<broseat::SessionManager> g_default_session_manager;

std::shared_ptr<broseat::InhibitManager> g_custom_inhibit_manager;
std::shared_ptr<broseat::InhibitManager> g_default_inhibit_manager;

std::optional<std::vector<std::filesystem::path>> g_custom_autostart_paths;
std::optional<broseat::AutostartFilter> g_custom_autostart_filter;

} // namespace

std::shared_ptr<broseat::SessionManager> activeSessionManager() {
    std::lock_guard lock(g_services_mu);
    if (g_custom_session_manager) return g_custom_session_manager;
    if (!g_default_session_manager) {
        std::string err;
        g_default_session_manager = broseat::SessionManager::create(&err);
    }
    return g_default_session_manager;
}

void setSessionManager(std::shared_ptr<broseat::SessionManager> mgr) {
    std::lock_guard lock(g_services_mu);
    g_custom_session_manager = std::move(mgr);
}

std::shared_ptr<broseat::SessionManager> getSessionManager() {
    return activeSessionManager();
}

std::shared_ptr<broseat::InhibitManager> activeInhibitManager() {
    std::lock_guard lock(g_services_mu);
    if (g_custom_inhibit_manager) return g_custom_inhibit_manager;
    if (!g_default_inhibit_manager) {
        std::string err;
        g_default_inhibit_manager = broseat::InhibitManager::create(&err);
    }
    return g_default_inhibit_manager;
}

void setInhibitManager(std::shared_ptr<broseat::InhibitManager> mgr) {
    std::lock_guard lock(g_services_mu);
    g_custom_inhibit_manager = std::move(mgr);
}

std::shared_ptr<broseat::InhibitManager> getInhibitManager() {
    return activeInhibitManager();
}

std::vector<std::filesystem::path> activeAutostartSearchPaths() {
    std::lock_guard lock(g_services_mu);
    if (g_custom_autostart_paths) return *g_custom_autostart_paths;
    return broseat::AutostartManager::default_search_paths();
}

void setAutostartSearchPaths(std::vector<std::filesystem::path> paths) {
    std::lock_guard lock(g_services_mu);
    g_custom_autostart_paths = std::move(paths);
}

std::vector<std::filesystem::path> getAutostartSearchPaths() {
    return activeAutostartSearchPaths();
}

broseat::AutostartFilter activeAutostartFilter() {
    std::lock_guard lock(g_services_mu);
    if (g_custom_autostart_filter) return *g_custom_autostart_filter;
    return broseat::AutostartFilter{};
}

void setAutostartFilter(broseat::AutostartFilter filter) {
    std::lock_guard lock(g_services_mu);
    g_custom_autostart_filter = std::move(filter);
}

broseat::AutostartFilter getAutostartFilter() {
    return activeAutostartFilter();
}

Value ensureBroSeat() {
    ev::Persistent globalThisVal;
    auto gt = ev::globalValue("globalThis");
    if (gt.found && ev::isObject(gt.value)) {
        globalThisVal.set(gt.value);
    }

    ev::Persistent broP;
    auto bro = ev::globalValue("bro");
    if (bro.found && ev::isObject(bro.value)) broP.set(bro.value);
    if (!ev::isObject(broP.get()) && ev::isObject(globalThisVal.get())) {
        Value candidate = ev::getProperty(globalThisVal.get(), "bro");
        if (ev::isObject(candidate)) broP.set(candidate);
    }
    if (!ev::isObject(broP.get())) {
        broP.set(ev::createObject());
        ev::registerGlobal("bro", broP.get());
        if (ev::isObject(globalThisVal.get())) {
            globalThisVal.set(ev::setProperty(globalThisVal.get(), "bro", broP.get()));
        }
    }

    ev::Persistent seatP(ev::getProperty(broP.get(), "seat"));
    if (!ev::isObject(seatP.get())) {
        seatP.set(ev::createObject());
        broP.set(ev::setProperty(broP.get(), "seat", seatP.get()));
    }
    return seatP.get();
}

void installSeat() {
    ev::Persistent seatObj(ensureBroSeat());
    installSessionOnto(seatObj.get());
    installInhibitOnto(seatObj.get());
    installAutostartOnto(seatObj.get());
}

void tickSeatAsync() {
    drainSeatEvents();
}

void shutdownSeatAsync() {
    clearActiveInhibitors();
}

} // namespace broseat::api
