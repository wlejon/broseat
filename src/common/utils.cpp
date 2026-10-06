#include "common/utils.h"

#include <cctype>
#include <cstdlib>
#include <filesystem>

#if defined(_WIN32)
#include <io.h>
#else
#include <unistd.h>
#endif

namespace broseat::utils {

std::string_view trim(std::string_view str) {
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.front()))) {
        str.remove_prefix(1);
    }
    while (!str.empty() && std::isspace(static_cast<unsigned char>(str.back()))) {
        str.remove_suffix(1);
    }
    return str;
}

std::vector<std::string> split(std::string_view str, char delim, bool skip_empty) {
    std::vector<std::string> result;
    size_t start = 0;
    while (start < str.size()) {
        size_t end = str.find(delim, start);
        if (end == std::string_view::npos) {
            end = str.size();
        }
        std::string_view token = trim(str.substr(start, end - start));
        if (!token.empty() || !skip_empty) {
            result.emplace_back(token);
        }
        start = end + 1;
    }
    return result;
}

bool iequals(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

bool starts_with(std::string_view str, std::string_view prefix) {
    return str.rfind(prefix, 0) == 0;
}

namespace {

bool is_executable(const std::filesystem::path& p) {
#if defined(_WIN32)
    // Windows has no execute bit: an existing regular file is as close as it gets.
    std::error_code ec;
    return std::filesystem::is_regular_file(p, ec);
#else
    return access(p.c_str(), X_OK) == 0;
#endif
}

}  // namespace

std::optional<std::string> find_in_path(const std::string& binary_name) {
    if (binary_name.empty()) return std::nullopt;

    // If it contains a slash, check direct path
    if (binary_name.find('/') != std::string::npos) {
        if (is_executable(binary_name)) {
            return binary_name;
        }
        return std::nullopt;
    }

#if defined(_WIN32)
    constexpr char kPathSep = ';';
#else
    constexpr char kPathSep = ':';
#endif
    const char* path_env = std::getenv("PATH");
    if (!path_env) {
        path_env = "/usr/local/bin:/usr/bin:/bin";
    }

    for (const auto& dir : split(path_env, kPathSep)) {
        std::filesystem::path p = std::filesystem::path(dir) / binary_name;
        if (is_executable(p)) {
            return p.string();
        }
    }
    return std::nullopt;
}

std::string get_env(const char* name, const char* default_val) {
    const char* val = std::getenv(name);
    return val ? std::string(val) : std::string(default_val);
}

bool set_env(const std::string& name, const std::string& value) {
#if defined(_WIN32)
    return _putenv_s(name.c_str(), value.c_str()) == 0;
#else
    return ::setenv(name.c_str(), value.c_str(), 1) == 0;
#endif
}

void close_fd(int fd) {
    if (fd < 0) return;
#if defined(_WIN32)
    _close(fd);
#else
    ::close(fd);
#endif
}

std::string sanitize_unit_name(std::string_view input) {
    std::string out;
    out.reserve(input.size());
    for (char c : input) {
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-') {
            out.push_back(c);
        } else if (c == '.') {
            out.push_back('_');
        } else {
            out.push_back('-');
        }
    }
    if (out.empty()) {
        out = "app";
    }
    return out;
}

}  // namespace broseat::utils
