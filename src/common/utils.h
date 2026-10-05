#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace broseat::utils {

std::string_view trim(std::string_view str);

std::vector<std::string> split(std::string_view str, char delim, bool skip_empty = true);

bool iequals(std::string_view a, std::string_view b);

bool starts_with(std::string_view str, std::string_view prefix);

std::optional<std::string> find_in_path(const std::string& binary_name);

std::string get_env(const char* name, const char* default_val = "");

bool set_env(const std::string& name, const std::string& value);

std::string sanitize_unit_name(std::string_view input);

}  // namespace broseat::utils
