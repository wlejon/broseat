#include "host_seat_internal.h"
#include "arg_reader.h"
#include "object_builder.h"
#include "broseat/inhibit.h"
#include "broseat/types.h"

#include <mutex>
#include <string>
#include <unordered_map>

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

namespace broseat::api {

namespace {

// The holder an inhibitor this process took is reported under. Windows has
// no POSIX user id, so there it is 0, InhibitorInfo::uid's default.
double selfUid() {
#if defined(_WIN32)
    return 0;
#else
    return static_cast<double>(getuid());
#endif
}

double selfPid() {
#if defined(_WIN32)
    return static_cast<double>(_getpid());
#else
    return static_cast<double>(getpid());
#endif
}

struct ActiveInhibitor {
    uint32_t id = 0;
    std::string type;
    std::string reason;
    std::unique_ptr<InhibitorLock> lock;
};

std::mutex g_inhibit_mu;
uint32_t g_next_inhibit_id = 1;
std::unordered_map<uint32_t, ActiveInhibitor> g_active_inhibitors;

} // namespace

bool hasLocalInhibitor(const std::string& what) {
    std::lock_guard lock(g_inhibit_mu);
    for (const auto& [id, entry] : g_active_inhibitors) {
        size_t start = 0;
        while (start <= entry.type.size()) {
            size_t colon = entry.type.find(':', start);
            if (colon == std::string::npos) colon = entry.type.size();
            if (entry.type.compare(start, colon - start, what) == 0 && colon - start == what.size())
                return true;
            start = colon + 1;
        }
    }
    return false;
}

void clearActiveInhibitors() {
    std::lock_guard lock(g_inhibit_mu);
    for (auto& [id, entry] : g_active_inhibitors) {
        if (entry.lock) {
            entry.lock->release();
        }
    }
    g_active_inhibitors.clear();
}

void installInhibitOnto(Value seatVal) {
    ObjectBuilder seat(seatVal);

    // bro.seat.inhibit(type, reason) -> number
    seat.def("inhibit", 2, [](Value, std::span<const Value> args) -> Value {
        if (args.empty()) return ev::fromDouble(0.0);

        ev::Persistent arg0(args[0]);
        ev::Persistent arg1(args.size() > 1 ? args[1] : ev::undefined());

        std::string type = ev::isString(arg0.get()) ? ev::toUtf8(arg0.get()) : "";
        std::string reason = ev::isString(arg1.get()) ? ev::toUtf8(arg1.get()) : "broseat";

        if (type.empty()) return ev::fromDouble(0.0);

        auto mgr = activeInhibitManager();
        if (!mgr) return ev::fromDouble(0.0);

        std::string err;
        auto lockPtr = mgr->inhibit(type, "broseat", reason, InhibitMode::Block, &err);
        if (!lockPtr || !lockPtr->is_held()) {
            return ev::fromDouble(0.0);
        }

        std::lock_guard lock(g_inhibit_mu);
        uint32_t id = g_next_inhibit_id++;
        g_active_inhibitors[id] = ActiveInhibitor{
            .id = id,
            .type = type,
            .reason = reason,
            .lock = std::move(lockPtr)
        };

        return ev::fromDouble(static_cast<double>(id));
    });

    // bro.seat.uninhibit(inhibitId) -> boolean
    seat.def("uninhibit", 1, [](Value, std::span<const Value> args) -> Value {
        if (args.empty()) return ev::fromBool(false);

        ev::Persistent arg0(args[0]);
        if (!ev::isNumber(arg0.get())) return ev::fromBool(false);
        uint32_t id = static_cast<uint32_t>(ev::toDouble(arg0.get()));

        std::lock_guard lock(g_inhibit_mu);
        auto it = g_active_inhibitors.find(id);
        if (it == g_active_inhibitors.end()) {
            return ev::fromBool(false);
        }

        if (it->second.lock) {
            it->second.lock->release();
        }
        g_active_inhibitors.erase(it);
        return ev::fromBool(true);
    });

    // bro.seat.listInhibitors() -> Array<{ id: number, type: string, reason: string }>
    seat.def("listInhibitors", 0, [](Value, std::span<const Value>) -> Value {
        auto mgr = activeInhibitManager();
        std::vector<InhibitorInfo> list;
        if (mgr) {
            std::string err;
            list = mgr->list_inhibitors(&err);
        }

        std::unordered_map<uint32_t, ActiveInhibitor> activeCopy;
        {
            std::lock_guard lock(g_inhibit_mu);
            for (const auto& [k, v] : g_active_inhibitors) {
                activeCopy[k] = ActiveInhibitor{
                    .id = v.id,
                    .type = v.type,
                    .reason = v.reason,
                    .lock = nullptr
                };
            }
        }

        if (list.empty() && !activeCopy.empty()) {
            ev::Persistent arr(ev::makeArray(static_cast<uint32_t>(activeCopy.size())));
            uint32_t idx = 0;
            for (const auto& [id, act] : activeCopy) {
                ObjectBuilder b;
                b.set("id", static_cast<double>(act.id));
                b.set("type", act.type);
                b.set("reason", act.reason);
                b.set("who", "broseat");
                b.set("mode", "block");
                b.set("uid", selfUid());
                b.set("pid", selfPid());
                ev::setElement(arr.get(), idx++, b.build());
            }
            return arr.get();
        }

        ev::Persistent arr(ev::makeArray(static_cast<uint32_t>(list.size())));
        for (uint32_t i = 0; i < list.size(); ++i) {
            const auto& item = list[i];
            ObjectBuilder b;

            uint32_t assignedId = 0;
            for (const auto& [actId, act] : activeCopy) {
                if (act.type == item.what && act.reason == item.why && item.pid == static_cast<uint32_t>(selfPid())) {
                    assignedId = actId;
                    break;
                }
            }
            if (assignedId == 0) {
                assignedId = static_cast<uint32_t>(i + 1000);
            }

            b.set("id", static_cast<double>(assignedId));
            b.set("type", item.what);
            b.set("reason", item.why);
            b.set("who", item.who);
            b.set("mode", item.mode);
            b.set("uid", static_cast<double>(item.uid));
            b.set("pid", static_cast<double>(item.pid));

            ev::setElement(arr.get(), i, b.build());
        }

        return arr.get();
    });
}

} // namespace broseat::api
