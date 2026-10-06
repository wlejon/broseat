// Exec= line tokenizing (Desktop Entry Specification quoting and field
// codes). Pure string handling, built on every platform.
#include "autostart/autostart_launcher.h"

#include <cctype>
#include <string>
#include <vector>

namespace broseat {

std::vector<std::string> expand_exec_line(const std::string& exec_line) {
    std::vector<std::string> tokens;
    std::string current;
    bool in_double_quote = false;
    bool in_single_quote = false;
    bool escape = false;

    for (size_t i = 0; i < exec_line.size(); ++i) {
        char c = exec_line[i];

        if (escape) {
            current.push_back(c);
            escape = false;
            continue;
        }

        if (c == '\\' && !in_single_quote) {
            escape = true;
            continue;
        }

        if (c == '"' && !in_single_quote) {
            in_double_quote = !in_double_quote;
            continue;
        }

        if (c == '\'' && !in_double_quote) {
            in_single_quote = !in_single_quote;
            continue;
        }

        if (std::isspace(static_cast<unsigned char>(c)) && !in_double_quote && !in_single_quote) {
            if (!current.empty()) {
                tokens.push_back(std::move(current));
                current.clear();
            }
            continue;
        }

        if (c == '%' && !in_single_quote && i + 1 < exec_line.size()) {
            char next = exec_line[i + 1];
            // Desktop Entry Specification field codes
            if (next == '%') {
                current.push_back('%');
                ++i;
                continue;
            } else if (next == 'f' || next == 'F' || next == 'u' || next == 'U' ||
                       next == 'd' || next == 'D' || next == 'n' || next == 'N' ||
                       next == 'v' || next == 'm' || next == 'i' || next == 'c' ||
                       next == 'k') {
                // Drop field code for autostart
                ++i;
                continue;
            }
        }

        current.push_back(c);
    }

    if (!current.empty()) {
        tokens.push_back(std::move(current));
    }

    return tokens;
}

}  // namespace broseat
