#include "autostart/autostart_condition.h"
#include "common/utils.h"

#include <vector>

namespace broseat {

namespace {

std::vector<std::string> get_desktop_tokens(const std::string& desktop_str) {
    std::string s = desktop_str;
    if (s.empty()) {
        s = utils::get_env("XDG_CURRENT_DESKTOP", "BRO");
    }
    return utils::split(s, ':');
}

bool match_desktop_list(const std::vector<std::string>& current_desktops, const std::vector<std::string>& list) {
    for (const auto& curr : current_desktops) {
        for (const auto& item : list) {
            if (utils::iequals(curr, item)) {
                return true;
            }
        }
    }
    return false;
}

}  // namespace

bool evaluate_autostart_condition(
    const AutostartEntry& entry,
    const AutostartFilter& filter,
    std::string* reason) {
    // 1. Type must be Application
    if (!entry.type.empty() && !utils::iequals(entry.type, "Application")) {
        if (reason) *reason = "Entry type is '" + entry.type + "', expected 'Application'";
        return false;
    }

    // 2. Hidden check
    if (entry.hidden && !filter.include_hidden) {
        if (reason) *reason = "Entry is marked as Hidden";
        return false;
    }

    // 3. Exec must not be empty
    if (entry.exec.empty()) {
        if (reason) *reason = "Exec line is empty";
        return false;
    }

    // 4. Phase check
    if (entry.phase != filter.phase) {
        if (reason) *reason = "Phase mismatch";
        return false;
    }

    std::vector<std::string> current_desktops = get_desktop_tokens(filter.current_desktop);

    // 5. OnlyShowIn check
    if (!entry.only_show_in.empty()) {
        if (!match_desktop_list(current_desktops, entry.only_show_in)) {
            if (reason) *reason = "Desktop not listed in OnlyShowIn";
            return false;
        }
    }

    // 6. NotShowIn check
    if (!entry.not_show_in.empty()) {
        if (match_desktop_list(current_desktops, entry.not_show_in)) {
            if (reason) *reason = "Desktop listed in NotShowIn";
            return false;
        }
    }

    // 7. TryExec check
    if (!entry.try_exec.empty()) {
        if (!utils::find_in_path(entry.try_exec)) {
            if (reason) *reason = "TryExec binary '" + entry.try_exec + "' not found or not executable";
            return false;
        }
    }

    // 8. AutostartCondition check
    if (!entry.autostart_condition.empty()) {
        auto tokens = utils::split(entry.autostart_condition, ' ');
        for (size_t i = 0; i < tokens.size(); ++i) {
            if (tokens[i] == "if-session" && i + 1 < tokens.size()) {
                std::string target_session = tokens[i + 1];
                bool found = false;
                for (const auto& d : current_desktops) {
                    if (utils::iequals(d, target_session)) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    if (reason) *reason = "AutostartCondition if-session " + target_session + " not satisfied";
                    return false;
                }
            } else if (tokens[i] == "unless-session" && i + 1 < tokens.size()) {
                std::string target_session = tokens[i + 1];
                for (const auto& d : current_desktops) {
                    if (utils::iequals(d, target_session)) {
                        if (reason) *reason = "AutostartCondition unless-session " + target_session + " matched";
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

}  // namespace broseat
