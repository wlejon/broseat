#pragma once

#include "broseat/autostart.h"
#include "broseat/types.h"

#include <string>

namespace broseat {

bool evaluate_autostart_condition(
    const AutostartEntry& entry,
    const AutostartFilter& filter,
    std::string* reason = nullptr);

}  // namespace broseat
