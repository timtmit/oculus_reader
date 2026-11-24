#pragma once

#include "oculus_controller_mode.hpp"

#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace oculus_reader
{

using ButtonValue =
    std::variant<bool, std::vector<double>>;

using Buttons =
    std::unordered_map<std::string, ButtonValue>;

Buttons parse_buttons(
    const std::string& text,
    ControllerMode mode);

} // namespace oculus_reader