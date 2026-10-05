#include "autostart/autostart_condition.h"
#include "autostart/autostart_desktop.h"
#include "autostart/autostart_launcher.h"
#include "broseat/autostart.h"

namespace broseat {

bool AutostartManager::parse_desktop_file(
    const std::filesystem::path& file_path,
    AutostartEntry* entry,
    std::string* error) {
    return parse_desktop_entry(file_path, entry, error);
}

bool AutostartManager::should_autostart(
    const AutostartEntry& entry,
    const AutostartFilter& filter,
    std::string* reason) {
    return evaluate_autostart_condition(entry, filter, reason);
}

std::vector<std::string> AutostartManager::expand_exec_arguments(const std::string& exec_line) {
    return expand_exec_line(exec_line);
}

LaunchResult AutostartManager::launch(const AutostartEntry& entry, LaunchMode mode) {
    return launch_entry(entry, mode);
}

std::vector<LaunchResult> AutostartManager::launch_all(
    const std::vector<AutostartEntry>& entries,
    LaunchMode mode) {
    std::vector<LaunchResult> results;
    results.reserve(entries.size());
    for (const auto& entry : entries) {
        results.push_back(launch(entry, mode));
    }
    return results;
}

}  // namespace broseat
