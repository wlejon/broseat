#include "host_seat_internal.h"
#include "arg_reader.h"
#include "object_builder.h"
#include "broseat/session.h"
#include "broseat/types.h"

#include <algorithm>
#include <mutex>
#include <vector>

namespace broseat::api {

namespace {

struct Subscription {
    std::string event;
    std::shared_ptr<ev::Persistent> callback;
};

std::mutex g_sub_mu;
std::vector<Subscription> g_subscriptions;

void dispatchSeatEvent(std::string_view event, Value payload) {
    ev::Persistent payloadRoot(payload);

    std::vector<std::shared_ptr<ev::Persistent>> targets;
    {
        std::lock_guard lock(g_sub_mu);
        for (const auto& sub : g_subscriptions) {
            if (sub.event == event || sub.event == "*" || sub.event == "change") {
                if (sub.callback) {
                    targets.push_back(sub.callback);
                }
            }
        }
    }

    for (const auto& cb : targets) {
        if (!cb || !ev::isFunction(cb->get())) continue;
        const Value arg = payloadRoot.get();
        ev::catchThrow([&]() {
            return ev::call(cb->get(), ev::undefined(), std::span<const Value>(&arg, 1)).value;
        });
    }

    // Check optional property listener on bro.seat (e.g. onlock, onunlock, onactive)
    ev::Persistent seatObj(ensureBroSeat());
    if (ev::isObject(seatObj.get())) {
        std::string propName = "on" + std::string(event);
        ev::Persistent handler(ev::getProperty(seatObj.get(), propName));
        if (ev::isFunction(handler.get())) {
            const Value arg = payloadRoot.get();
            ev::catchThrow([&]() {
                return ev::call(handler.get(), seatObj.get(), std::span<const Value>(&arg, 1)).value;
            });
        }
    }
}

} // namespace

void addSeatEventListener(const std::string& event, Value callback) {
    if (!ev::isFunction(callback)) return;
    std::lock_guard lock(g_sub_mu);
    Subscription sub;
    sub.event = event;
    sub.callback = std::make_shared<ev::Persistent>(callback);
    g_subscriptions.push_back(std::move(sub));
}

void removeSeatEventListener(const std::string& event, Value callback) {
    if (!ev::isFunction(callback)) return;
    std::lock_guard lock(g_sub_mu);
    g_subscriptions.erase(
        std::remove_if(g_subscriptions.begin(), g_subscriptions.end(),
                       [&](const Subscription& sub) {
                           if (sub.event != event) return false;
                           return sub.callback && sub.callback->get() == callback;
                       }),
        g_subscriptions.end());
}

void drainSeatEvents() {
    auto mgr = activeSessionManager();
    if (!mgr) return;

    mgr->dispatch(0);
    auto events = mgr->event_queue().drain();

    for (const auto& evVariant : events) {
        std::visit([&](const auto& event) {
            using T = std::decay_t<decltype(event)>;
            if constexpr (std::is_same_v<T, SeatActiveChanged>) {
                ObjectBuilder b;
                b.set("type", "active");
                b.set("active", event.active);
                dispatchSeatEvent("active", b.build());
            } else if constexpr (std::is_same_v<T, SessionLockedChanged>) {
                ObjectBuilder b;
                std::string type = event.locked ? "lock" : "unlock";
                b.set("type", type);
                b.set("locked", event.locked);
                Value val = b.build();
                dispatchSeatEvent(type, val);
                dispatchSeatEvent("locked", val);
            } else if constexpr (std::is_same_v<T, SessionStateChanged>) {
                ObjectBuilder b;
                b.set("type", "state");
                b.set("state", std::string(session_state_name(event.state)));
                dispatchSeatEvent("state", b.build());
            } else if constexpr (std::is_same_v<T, VtSwitched>) {
                ObjectBuilder b;
                b.set("type", "vt");
                b.set("vt", static_cast<double>(event.vt_number));
                dispatchSeatEvent("vt", b.build());
            } else if constexpr (std::is_same_v<T, IdleStateChanged>) {
                ObjectBuilder b;
                b.set("type", "idle");
                b.set("idle", event.idle);
                b.set("idleTime", static_cast<double>(event.idle_time.count()));
                dispatchSeatEvent("idle", b.build());
            } else if constexpr (std::is_same_v<T, InhibitorAdded>) {
                ObjectBuilder b;
                b.set("type", "inhibitorAdded");
                b.set("what", event.info.what);
                b.set("who", event.info.who);
                b.set("why", event.info.why);
                b.set("mode", event.info.mode);
                b.set("uid", static_cast<double>(event.info.uid));
                b.set("pid", static_cast<double>(event.info.pid));
                dispatchSeatEvent("inhibitorAdded", b.build());
            } else if constexpr (std::is_same_v<T, InhibitorRemoved>) {
                ObjectBuilder b;
                b.set("type", "inhibitorRemoved");
                b.set("who", event.who);
                b.set("what", event.what);
                dispatchSeatEvent("inhibitorRemoved", b.build());
            } else if constexpr (std::is_same_v<T, AutostartEntryLaunched>) {
                ObjectBuilder b;
                b.set("type", "autostart");
                b.set("id", event.entry_id);
                b.set("pid", static_cast<double>(event.pid));
                b.set("success", event.success);
                if (!event.error.empty()) b.set("error", event.error);
                dispatchSeatEvent("autostart", b.build());
            }
        }, evVariant);
    }

    ev::drainMicrotasks();
}

void installSessionOnto(Value seatVal) {
    ObjectBuilder seat(seatVal);

    // bro.seat.getSessionState() -> { active, locked, vt, id, user, seat, state }
    seat.def("getSessionState", 0, [](Value, std::span<const Value>) -> Value {
        auto mgr = activeSessionManager();
        if (mgr) {
            SessionInfo info = mgr->current_session_info();
            ObjectBuilder b;
            b.set("active", info.active);
            b.set("locked", info.locked);
            b.set("vt", static_cast<double>(info.vt));
            b.set("id", info.id);
            b.set("user", info.user);
            b.set("seat", info.seat);
            b.set("state", std::string(session_state_name(info.state)));
            return b.build();
        }

        ObjectBuilder b;
        b.set("active", false);
        b.set("locked", false);
        b.set("vt", 0.0);
        b.set("id", "");
        b.set("user", "");
        b.set("seat", "");
        b.set("state", "unknown");
        return b.build();
    });

    // bro.seat.lock() -> boolean
    seat.def("lock", 0, [](Value, std::span<const Value>) -> Value {
        auto mgr = activeSessionManager();
        bool ok = mgr ? mgr->lock_session() : false;
        return ev::fromBool(ok);
    });

    // bro.seat.unlock() -> boolean
    seat.def("unlock", 0, [](Value, std::span<const Value>) -> Value {
        auto mgr = activeSessionManager();
        bool ok = mgr ? mgr->unlock_session() : false;
        return ev::fromBool(ok);
    });

    // bro.seat.switchVt(vtNumber) -> boolean
    seat.def("switchVt", 1, [](Value, std::span<const Value> args) -> Value {
        if (args.empty()) return ev::fromBool(false);
        ev::Persistent arg0(args[0]);
        if (!ev::isNumber(arg0.get())) return ev::fromBool(false);
        int vt = static_cast<int>(ev::toDouble(arg0.get()));
        if (vt <= 0) return ev::fromBool(false);

        auto mgr = activeSessionManager();
        bool ok = mgr ? mgr->switch_vt(static_cast<uint32_t>(vt)) : false;
        return ev::fromBool(ok);
    });

    // bro.seat.on(event, callback)
    seat.def("on", 2, [](Value self, std::span<const Value> args) -> Value {
        if (args.size() < 2) return self;
        ev::Persistent arg0(args[0]);
        ev::Persistent arg1(args[1]);
        if (!ev::isString(arg0.get()) || !ev::isFunction(arg1.get())) return self;
        addSeatEventListener(ev::toUtf8(arg0.get()), arg1.get());
        return self;
    });

    // bro.seat.off(event, callback)
    seat.def("off", 2, [](Value self, std::span<const Value> args) -> Value {
        if (args.size() < 2) return self;
        ev::Persistent arg0(args[0]);
        ev::Persistent arg1(args[1]);
        if (!ev::isString(arg0.get()) || !ev::isFunction(arg1.get())) return self;
        removeSeatEventListener(ev::toUtf8(arg0.get()), arg1.get());
        return self;
    });

    // bro.seat.addEventListener(event, callback)
    seat.def("addEventListener", 2, [](Value self, std::span<const Value> args) -> Value {
        if (args.size() < 2) return self;
        ev::Persistent arg0(args[0]);
        ev::Persistent arg1(args[1]);
        if (!ev::isString(arg0.get()) || !ev::isFunction(arg1.get())) return self;
        addSeatEventListener(ev::toUtf8(arg0.get()), arg1.get());
        return self;
    });

    // bro.seat.removeEventListener(event, callback)
    seat.def("removeEventListener", 2, [](Value self, std::span<const Value> args) -> Value {
        if (args.size() < 2) return self;
        ev::Persistent arg0(args[0]);
        ev::Persistent arg1(args[1]);
        if (!ev::isString(arg0.get()) || !ev::isFunction(arg1.get())) return self;
        removeSeatEventListener(ev::toUtf8(arg0.get()), arg1.get());
        return self;
    });
}

} // namespace broseat::api
