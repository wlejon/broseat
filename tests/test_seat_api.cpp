#include "../src/api/api.h"
#include "embed/embed.h"
#include "eval/eval.h"
#include "broseat/autostart.h"
#include "broseat/inhibit.h"
#include "broseat/session.h"
#include "broseat/types.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::cerr << "CHECK failed: " #cond " (line " << __LINE__ << ")" \
                      << std::endl;                                        \
            std::exit(1);                                                  \
        }                                                                  \
    } while (0)

int main() {
    namespace ev = bronze::embed;
    using namespace bronze::eval;

    std::cout << "Starting broseat JavaScript API test..." << std::endl;

    // 1. Install bro.seat into Bronze realm
    broseat::api::installSeat();

    auto g = ev::globalValue("bro");
    CHECK(g.found);
    CHECK(ev::isObject(g.value));

    ev::Persistent seat(ev::getProperty(g.value, "seat"));
    CHECK(ev::isObject(seat.get()));
    std::cout << "  Mounted bro.seat successfully." << std::endl;

    // Verify all required methods exist
    const char* methods[] = {
        "getSessionState", "lock", "unlock", "switchVt",
        "inhibit", "uninhibit", "listInhibitors",
        "listAutostart", "runAutostart",
        "on", "off", "addEventListener", "removeEventListener"
    };
    for (const char* m : methods) {
        auto fn = ev::getProperty(seat.get(), m);
        CHECK(ev::isFunction(fn));
        std::cout << "  Found bro.seat." << m << std::endl;
    }

    // 2. Test getSessionState()
    std::cout << "Testing getSessionState()..." << std::endl;
    {
        auto r = evalScript(
            "(function() {\n"
            "  const state = bro.seat.getSessionState();\n"
            "  if (typeof state !== 'object' || state === null) return false;\n"
            "  if (typeof state.active !== 'boolean') return false;\n"
            "  if (typeof state.locked !== 'boolean') return false;\n"
            "  if (typeof state.vt !== 'number') return false;\n"
            "  if (typeof state.id !== 'string') return false;\n"
            "  if (typeof state.user !== 'string') return false;\n"
            "  if (typeof state.seat !== 'string') return false;\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!r.thrown);
        CHECK(ev::isBool(r.value) && ev::toBool(r.value));
        std::cout << "  getSessionState() [PASS]" << std::endl;
    }

    // 3. Test lock() and unlock()
    std::cout << "Testing lock() and unlock()..." << std::endl;
    {
        auto r = evalScript(
            "(function() {\n"
            "  const lockRet = bro.seat.lock();\n"
            "  if (typeof lockRet !== 'boolean') return false;\n"
            "  const unlockRet = bro.seat.unlock();\n"
            "  if (typeof unlockRet !== 'boolean') return false;\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!r.thrown);
        CHECK(ev::isBool(r.value) && ev::toBool(r.value));
        std::cout << "  lock() and unlock() [PASS]" << std::endl;
    }

    // 4. Test switchVt()
    std::cout << "Testing switchVt()..." << std::endl;
    {
        auto r = evalScript(
            "(function() {\n"
            "  if (bro.seat.switchVt(0) !== false) return false;\n"
            "  if (bro.seat.switchVt(-1) !== false) return false;\n"
            "  const r1 = bro.seat.switchVt(7);\n"
            "  if (typeof r1 !== 'boolean') return false;\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!r.thrown);
        CHECK(ev::isBool(r.value) && ev::toBool(r.value));
        std::cout << "  switchVt() [PASS]" << std::endl;
    }

    // 5. Test inhibit(), uninhibit(), listInhibitors()
    std::cout << "Testing inhibit(), uninhibit(), listInhibitors()..." << std::endl;
    {
        auto rInhibit = evalScript(
            "(function() {\n"
            "  const id = bro.seat.inhibit('idle', 'test-idle-inhibit');\n"
            "  globalThis._inhibitId = id;\n"
            "  return typeof id === 'number';\n"
            "})()\n"
        );
        CHECK(!rInhibit.thrown);
        CHECK(ev::isBool(rInhibit.value) && ev::toBool(rInhibit.value));

        auto rList = evalScript(
            "(function() {\n"
            "  const list = bro.seat.listInhibitors();\n"
            "  if (!Array.isArray(list)) return false;\n"
            "  for (const item of list) {\n"
            "    if (typeof item.id !== 'number') return false;\n"
            "    if (typeof item.type !== 'string') return false;\n"
            "    if (typeof item.reason !== 'string') return false;\n"
            "  }\n"
            "  if (globalThis._inhibitId > 0) {\n"
            "    const found = list.some(x => x.id === globalThis._inhibitId && x.type === 'idle');\n"
            "    if (!found) return false;\n"
            "  }\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!rList.thrown);
        CHECK(ev::isBool(rList.value) && ev::toBool(rList.value));

        auto rUninhibit = evalScript(
            "(function() {\n"
            "  if (globalThis._inhibitId > 0) {\n"
            "    const ok = bro.seat.uninhibit(globalThis._inhibitId);\n"
            "    if (!ok) return false;\n"
            "    const second = bro.seat.uninhibit(globalThis._inhibitId);\n"
            "    if (second !== false) return false;\n"
            "  } else {\n"
            "    if (bro.seat.uninhibit(999999) !== false) return false;\n"
            "  }\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!rUninhibit.thrown);
        CHECK(ev::isBool(rUninhibit.value) && ev::toBool(rUninhibit.value));
        std::cout << "  inhibit(), uninhibit(), listInhibitors() [PASS]" << std::endl;
    }

    // 6. Test Autostart (listAutostart and runAutostart)
    std::cout << "Testing listAutostart() and runAutostart()..." << std::endl;
    auto tmp_dir = std::filesystem::temp_directory_path() /
                   ("broseat_api_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(tmp_dir);

    auto test_desktop_file = tmp_dir / "seat-test-app.desktop";
    {
        std::ofstream ofs(test_desktop_file);
        ofs << "[Desktop Entry]\n"
            << "Type=Application\n"
            << "Name=Seat Test Application\n"
            << "Comment=Test autostart launcher\n"
            << "Exec=/bin/echo \"seat-autostart-ok\"\n"
            << "Icon=utilities-terminal\n"
            << "Hidden=false\n";
    }

    auto test_hidden_file = tmp_dir / "seat-hidden-app.desktop";
    {
        std::ofstream ofs(test_hidden_file);
        ofs << "[Desktop Entry]\n"
            << "Type=Application\n"
            << "Name=Hidden Application\n"
            << "Comment=Hidden app\n"
            << "Exec=/bin/echo \"hidden\"\n"
            << "Hidden=true\n";
    }

    broseat::api::setAutostartSearchPaths({tmp_dir});

    {
        auto rListAuto = evalScript(
            "(function() {\n"
            "  const list = bro.seat.listAutostart();\n"
            "  if (!Array.isArray(list)) return false;\n"
            "  if (list.length < 2) return false;\n"
            "  const app1 = list.find(x => x.id === 'seat-test-app');\n"
            "  if (!app1) return false;\n"
            "  if (app1.name !== 'Seat Test Application') return false;\n"
            "  if (!app1.exec.includes('/bin/echo')) return false;\n"
            "  if (app1.enabled !== true) return false;\n"
            "  const hidden = list.find(x => x.id === 'seat-hidden-app');\n"
            "  if (!hidden || hidden.enabled !== false) return false;\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!rListAuto.thrown);
        CHECK(ev::isBool(rListAuto.value) && ev::toBool(rListAuto.value));
        std::cout << "  listAutostart() [PASS]" << std::endl;

        // Test runAutostart() -> Promise<Array<{ id, pid, success }>>
        auto rRunAuto = evalScript(
            "(function() {\n"
            "  globalThis._autostartResults = null;\n"
            "  globalThis._autostartError = null;\n"
            "  const p = bro.seat.runAutostart();\n"
            "  if (!p || typeof p.then !== 'function') return false;\n"
            "  p.then((results) => {\n"
            "    globalThis._autostartResults = results;\n"
            "  }).catch((err) => {\n"
            "    globalThis._autostartError = String(err);\n"
            "  });\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!rRunAuto.thrown);
        CHECK(ev::isBool(rRunAuto.value) && ev::toBool(rRunAuto.value));

        ev::drainMicrotasks();

        auto rCheckRun = evalScript(
            "(function() {\n"
            "  if (globalThis._autostartError) return false;\n"
            "  const results = globalThis._autostartResults;\n"
            "  if (!Array.isArray(results)) return false;\n"
            "  const appRes = results.find(r => r.id === 'seat-test-app');\n"
            "  if (!appRes) return false;\n"
            "  if (typeof appRes.pid !== 'number') return false;\n"
            "  if (typeof appRes.success !== 'boolean') return false;\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!rCheckRun.thrown);
        CHECK(ev::isBool(rCheckRun.value) && ev::toBool(rCheckRun.value));
        std::cout << "  runAutostart() [PASS]" << std::endl;
    }

    // 7. Test event listeners and tickSeatAsync()
    std::cout << "Testing event listeners and tickSeatAsync()..." << std::endl;
    {
        auto rInitEvents = evalScript(
            "(function() {\n"
            "  globalThis._lockEvents = [];\n"
            "  globalThis._allEvents = [];\n"
            "  globalThis._onLockCb = (e) => globalThis._lockEvents.push(e);\n"
            "  bro.seat.on('lock', globalThis._onLockCb);\n"
            "  bro.seat.addEventListener('*', (e) => globalThis._allEvents.push(e));\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!rInitEvents.thrown);

        auto sessMgr = broseat::api::getSessionManager();
        if (sessMgr) {
            sessMgr->event_queue().push(broseat::SessionLockedChanged{true});
            sessMgr->event_queue().push(broseat::SeatActiveChanged{true});
        }

        // Before tick, events should not be in JS arrays yet
        auto rBeforeTick = evalScript("globalThis._lockEvents.length === 0;");
        CHECK(!rBeforeTick.thrown && ev::toBool(rBeforeTick.value));

        // Pump events
        broseat::api::tickSeatAsync();

        if (sessMgr) {
            auto rAfterTick = evalScript(
                "(function() {\n"
                "  if (globalThis._lockEvents.length < 1) return false;\n"
                "  if (globalThis._lockEvents[0].type !== 'lock') return false;\n"
                "  if (globalThis._lockEvents[0].locked !== true) return false;\n"
                "  if (globalThis._allEvents.length < 2) return false;\n"
                "  return true;\n"
                "})()\n"
            );
            CHECK(!rAfterTick.thrown);
            CHECK(ev::isBool(rAfterTick.value) && ev::toBool(rAfterTick.value));

            // Test unregistering
            evalScript("bro.seat.off('lock', globalThis._onLockCb);");
            sessMgr->event_queue().push(broseat::SessionLockedChanged{false});
            broseat::api::tickSeatAsync();

            auto rAfterOff = evalScript("globalThis._lockEvents.length === 1;");
            CHECK(!rAfterOff.thrown && ev::toBool(rAfterOff.value));
        }
        std::cout << "  event listeners and tickSeatAsync() [PASS]" << std::endl;
    }

    // 8. Test shutdownSeatAsync()
    std::cout << "Testing shutdownSeatAsync()..." << std::endl;
    {
        evalScript("globalThis._inhibitId2 = bro.seat.inhibit('sleep', 'shutdown-test');");
        broseat::api::shutdownSeatAsync();
        auto rCheckShutdown = evalScript(
            "(function() {\n"
            "  if (globalThis._inhibitId2 > 0) {\n"
            "    const ok = bro.seat.uninhibit(globalThis._inhibitId2);\n"
            "    if (ok !== false) return false;\n"
            "  }\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!rCheckShutdown.thrown && ev::toBool(rCheckShutdown.value));
        std::cout << "  shutdownSeatAsync() [PASS]" << std::endl;
    }

    // 9. GC stress testing loop: verify stability under allocations
    std::cout << "Running GC stress simulation loop..." << std::endl;
    {
        auto rStress = evalScript(
            "(function() {\n"
            "  for (let i = 0; i < 200; ++i) {\n"
            "    const st = bro.seat.getSessionState();\n"
            "    const inh = bro.seat.listInhibitors();\n"
            "    const auto = bro.seat.listAutostart();\n"
            "    const cb = (e) => {};\n"
            "    bro.seat.on('lock', cb);\n"
            "    bro.seat.off('lock', cb);\n"
            "  }\n"
            "  return true;\n"
            "})()\n"
        );
        CHECK(!rStress.thrown && ev::toBool(rStress.value));
        broseat::api::tickSeatAsync();
        std::cout << "  GC stress loop [PASS]" << std::endl;
    }

    // Cleanup temporary files
    std::filesystem::remove_all(tmp_dir);

    std::cout << "All broseat JavaScript API tests PASSED!" << std::endl;
    return 0;
}
