#pragma once

#include "broseat/types.h"

#include <filesystem>
#include <string>
#include <vector>

namespace broseat {

struct AutostartFilter {
    std::string current_desktop = "BRO";
    AutostartPhase phase = AutostartPhase::Application;
    bool include_hidden = false;
};

class AutostartManager {
public:
    static std::vector<std::filesystem::path> default_search_paths();

    static bool parse_desktop_file(const std::filesystem::path& file_path, AutostartEntry* entry, std::string* error = nullptr);

    static std::vector<AutostartEntry> discover(
        const std::vector<std::filesystem::path>& search_paths = default_search_paths(),
        const AutostartFilter& filter = {});

    static bool should_autostart(const AutostartEntry& entry, const AutostartFilter& filter, std::string* reason = nullptr);

    static std::vector<std::string> expand_exec_arguments(const std::string& exec_line);

    static LaunchResult launch(const AutostartEntry& entry, LaunchMode mode = LaunchMode::Auto);

    static std::vector<LaunchResult> launch_all(
        const std::vector<AutostartEntry>& entries,
        LaunchMode mode = LaunchMode::Auto);
};

}  // namespace broseat
