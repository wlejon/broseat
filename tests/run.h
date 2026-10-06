// Runs an oracle command through /bin/sh and captures its stdout. Linux tests
// only: the oracles are the system's own tools (systemctl, loginctl,
// systemd-inhibit, busctl).
#pragma once

#include <cstdio>
#include <string>
#include <sys/wait.h>

namespace bstest {

struct RunResult {
    int status = -1;   // exit status, or -1 when the command could not run
    std::string out;   // stdout
    bool ok() const { return status == 0; }
};

inline RunResult run(const std::string& cmd) {
    RunResult r;
    FILE* p = ::popen((cmd + " 2>/dev/null").c_str(), "r");
    if (!p) return r;
    char buf[4096];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof buf, p)) > 0) r.out.append(buf, n);
    int st = ::pclose(p);
    r.status = (st != -1 && WIFEXITED(st)) ? WEXITSTATUS(st) : -1;
    return r;
}

inline std::string trimmed(std::string s) {
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ')) s.pop_back();
    return s;
}

// "Key=Value" lines (loginctl show-*, systemctl show) -> value of key, or "".
inline std::string prop(const std::string& text, const std::string& key) {
    std::string needle = key + "=";
    size_t pos = 0;
    while (pos < text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string::npos) end = text.size();
        if (text.compare(pos, needle.size(), needle) == 0) {
            return text.substr(pos + needle.size(), end - pos - needle.size());
        }
        pos = end + 1;
    }
    return {};
}

}  // namespace bstest
