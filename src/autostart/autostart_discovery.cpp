#include "autostart/autostart_condition.h"
#include "autostart/autostart_desktop.h"
#include "broseat/autostart.h"
#include "common/utils.h"

#include <filesystem>
#include <map>
#include <string>

namespace broseat {

std::vector<std::filesystem::path> AutostartManager::default_search_paths() {
    std::vector<std::filesystem::path> paths;

    std::string config_home = utils::get_env("XDG_CONFIG_HOME");
    if (config_home.empty()) {
        std::string home = utils::get_env("HOME");
        if (!home.empty()) {
            config_home = home + "/.config";
        }
    }
    if (!config_home.empty()) {
        paths.push_back(std::filesystem::path(config_home) / "autostart");
    }

    std::string config_dirs = utils::get_env("XDG_CONFIG_DIRS", "/etc/xdg");
    for (const auto& dir : utils::split(config_dirs, ':')) {
        paths.push_back(std::filesystem::path(dir) / "autostart");
    }

    return paths;
}

std::vector<AutostartEntry> AutostartManager::discover(
    const std::vector<std::filesystem::path>& search_paths,
    const AutostartFilter& filter) {
    // Map filename -> entry to allow higher-priority search paths to override
    std::map<std::string, AutostartEntry> discovered;
    std::vector<std::string> order;

    for (const auto& dir : search_paths) {
        std::error_code ec;
        if (!std::filesystem::exists(dir, ec) || !std::filesystem::is_directory(dir, ec)) {
            continue;
        }

        for (const auto& item : std::filesystem::directory_iterator(dir, ec)) {
            if (ec) break;
            if (!item.is_regular_file()) continue;

            const auto& path = item.path();
            if (path.extension() != ".desktop") continue;

            std::string filename = path.filename().string();
            // Precedence: only take the first occurrence of a given desktop file
            if (discovered.find(filename) == discovered.end()) {
                AutostartEntry entry;
                if (parse_desktop_entry(path, &entry)) {
                    discovered[filename] = std::move(entry);
                    order.push_back(filename);
                }
            }
        }
    }

    std::vector<AutostartEntry> result;
    for (const auto& filename : order) {
        const auto& entry = discovered[filename];
        if (should_autostart(entry, filter)) {
            result.push_back(entry);
        }
    }

    return result;
}

}  // namespace broseat
