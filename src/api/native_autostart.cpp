#include "host_seat_internal.h"
#include "arg_reader.h"
#include "object_builder.h"
#include "broseat/autostart.h"
#include "broseat/types.h"

#include <vector>

namespace broseat::api {

void installAutostartOnto(Value seatVal) {
    ObjectBuilder seat(seatVal);

    // bro.seat.listAutostart() -> Array<{ id: string, name: string, exec: string, enabled: boolean }>
    seat.def("listAutostart", 0, [](Value, std::span<const Value>) -> Value {
        auto paths = activeAutostartSearchPaths();
        auto filter = activeAutostartFilter();

        AutostartFilter discoverFilter = filter;
        discoverFilter.include_hidden = true;

        auto entries = AutostartManager::discover(paths, discoverFilter);
        ev::Persistent arr(ev::makeArray(static_cast<uint32_t>(entries.size())));
        for (uint32_t i = 0; i < entries.size(); ++i) {
            const auto& e = entries[i];
            bool enabled = !e.hidden && AutostartManager::should_autostart(e, filter);

            ObjectBuilder b;
            b.set("id", e.id);
            b.set("name", e.name);
            b.set("exec", e.exec);
            b.set("enabled", enabled);
            if (!e.comment.empty()) b.set("comment", e.comment);
            if (!e.icon.empty()) b.set("icon", e.icon);

            ev::setElement(arr.get(), i, b.build());
        }

        return arr.get();
    });

    // bro.seat.runAutostart() -> Promise<Array<{ id: string, pid: number, success: boolean }>>
    seat.def("runAutostart", 0, [](Value, std::span<const Value> args) -> Value {
        ev::Persistent promiseP(ev::createPromise());

        std::vector<ev::Persistent> pinnedArgs;
        pinnedArgs.reserve(args.size());
        for (const auto& a : args) pinnedArgs.emplace_back(a);

        auto paths = activeAutostartSearchPaths();
        auto filter = activeAutostartFilter();

        std::vector<AutostartEntry> entriesToLaunch;
        if (!pinnedArgs.empty() && ev::isString(pinnedArgs[0].get())) {
            std::string targetId = ev::toUtf8(pinnedArgs[0].get());
            auto allEntries = AutostartManager::discover(paths, filter);
            for (const auto& e : allEntries) {
                if (e.id == targetId) {
                    entriesToLaunch.push_back(e);
                    break;
                }
            }
        } else {
            entriesToLaunch = AutostartManager::discover(paths, filter);
        }

        auto results = AutostartManager::launch_all(entriesToLaunch);

        ev::Persistent arr(ev::makeArray(static_cast<uint32_t>(results.size())));
        for (uint32_t i = 0; i < results.size(); ++i) {
            ObjectBuilder b;
            b.set("id", i < entriesToLaunch.size() ? entriesToLaunch[i].id : "");
            b.set("pid", static_cast<double>(results[i].pid));
            b.set("success", results[i].success);
            if (!results[i].error.empty()) b.set("error", results[i].error);
            if (!results[i].unit_name.empty()) b.set("unitName", results[i].unit_name);

            ev::setElement(arr.get(), i, b.build());
        }

        ev::resolvePromise(promiseP.get(), arr.get());
        return promiseP.get();
    });
}

} // namespace broseat::api
