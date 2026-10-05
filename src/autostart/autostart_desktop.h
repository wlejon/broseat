#pragma once

#include "broseat/types.h"

#include <filesystem>
#include <string>

namespace broseat {

bool parse_desktop_entry(
    const std::filesystem::path& file_path,
    AutostartEntry* entry,
    std::string* error = nullptr);

}  // namespace broseat
