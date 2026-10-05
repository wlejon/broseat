#pragma once

#include "broseat/autostart.h"
#include "broseat/types.h"

#include <string>
#include <vector>

namespace broseat {

std::vector<std::string> expand_exec_line(const std::string& exec_line);

LaunchResult launch_entry(const AutostartEntry& entry, LaunchMode mode = LaunchMode::Auto);

}  // namespace broseat
