#include "autostart/autostart_desktop.h"
#include "common/utils.h"

#include <fstream>
#include <sstream>

namespace broseat {

namespace {

bool parse_boolean(std::string_view val) {
    auto v = utils::trim(val);
    return utils::iequals(v, "true") || v == "1";
}

}  // namespace

bool parse_desktop_entry(
    const std::filesystem::path& file_path,
    AutostartEntry* entry,
    std::string* error) {
    if (!entry) return false;

    std::ifstream file(file_path);
    if (!file.is_open()) {
        if (error) *error = "Failed to open file: " + file_path.string();
        return false;
    }

    AutostartEntry result;
    result.id = file_path.stem().string();
    result.file_path = file_path;

    std::string line;
    std::string current_group;
    bool found_desktop_entry = false;

    while (std::getline(file, line)) {
        std::string_view sv = utils::trim(line);
        if (sv.empty() || sv.front() == '#') {
            continue;
        }

        if (sv.front() == '[' && sv.back() == ']') {
            current_group = std::string(sv.substr(1, sv.size() - 2));
            if (current_group == "Desktop Entry") {
                found_desktop_entry = true;
            }
            continue;
        }

        if (current_group != "Desktop Entry") {
            continue;
        }

        size_t eq = sv.find('=');
        if (eq == std::string_view::npos) {
            continue;
        }

        std::string_view key_full = utils::trim(sv.substr(0, eq));
        std::string_view val = utils::trim(sv.substr(eq + 1));

        // Strip locale suffix e.g. Name[de] -> key "Name"
        std::string_view key = key_full;
        bool is_localized = false;
        size_t bracket = key_full.find('[');
        if (bracket != std::string_view::npos) {
            key = utils::trim(key_full.substr(0, bracket));
            is_localized = true;
        }

        if (key == "Name") {
            if (!is_localized || result.name.empty()) {
                result.name = std::string(val);
            }
        } else if (key == "Comment") {
            if (!is_localized || result.comment.empty()) {
                result.comment = std::string(val);
            }
        } else if (key == "Exec") {
            result.exec = std::string(val);
        } else if (key == "TryExec") {
            result.try_exec = std::string(val);
        } else if (key == "Icon") {
            result.icon = std::string(val);
        } else if (key == "Path") {
            result.working_dir = std::string(val);
        } else if (key == "Type") {
            result.type = std::string(val);
        } else if (key == "Hidden") {
            result.hidden = parse_boolean(val);
        } else if (key == "NoDisplay") {
            result.no_display = parse_boolean(val);
        } else if (key == "Terminal") {
            result.terminal = parse_boolean(val);
        } else if (key == "OnlyShowIn") {
            result.only_show_in = utils::split(val, ';');
        } else if (key == "NotShowIn") {
            result.not_show_in = utils::split(val, ';');
        } else if (key == "AutostartCondition") {
            result.autostart_condition = std::string(val);
        } else if (key == "X-GNOME-Autostart-Phase" || key == "X-KDE-autostart-phase") {
            result.phase = parse_autostart_phase(val);
        }
    }

    if (!found_desktop_entry) {
        if (error) *error = "No [Desktop Entry] group found in " + file_path.string();
        return false;
    }

    *entry = std::move(result);
    return true;
}

}  // namespace broseat
