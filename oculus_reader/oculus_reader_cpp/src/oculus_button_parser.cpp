#include "oculus_button_parser.hpp"

#include <algorithm>
#include <sstream>

namespace oculus_reader
{

Buttons parse_buttons(
    const std::string& text,
    ControllerMode mode)
{
    std::vector<std::string> split_text;

    std::stringstream ss(text);
    std::string elem;

    while (std::getline(ss, elem, ','))
    {
        split_text.push_back(elem);
    }

    Buttons buttons;

    auto contains = [&split_text](const std::string& value)
    {
        return std::find(
            split_text.begin(),
            split_text.end(),
            value) != split_text.end();
    };

    auto remove = [&split_text](const std::string& value)
    {
        auto it = std::find(
            split_text.begin(),
            split_text.end(),
            value);

        if (it != split_text.end())
        {
            split_text.erase(it);
        }
    };


    // ------------------------------------------------------------
    // Right hand
    // ------------------------------------------------------------

    if ((mode == ControllerMode::RIGHT ||
         mode == ControllerMode::BOTH) &&
        contains("R"))
    {
        remove("R");

        buttons["A"]    = false;
        buttons["B"]    = false;
        buttons["RThU"] = false;
        buttons["RJ"]   = false;
        buttons["RG"]   = false;
        buttons["RTr"]  = false;
    }


    // ------------------------------------------------------------
    // Left hand
    // ------------------------------------------------------------

    if ((mode == ControllerMode::LEFT ||
         mode == ControllerMode::BOTH) &&
        contains("L"))
    {
        remove("L");

        buttons["X"]    = false;
        buttons["Y"]    = false;
        buttons["LThU"] = false;
        buttons["LJ"]   = false;
        buttons["LG"]   = false;
        buttons["LTr"]  = false;
    }


    // ------------------------------------------------------------
    // Boolean buttons
    // ------------------------------------------------------------

    for (auto& [key, value] : buttons)
    {
        if (contains(key))
        {
            value = true;
            remove(key);
        }
    }


    // ------------------------------------------------------------
    // Numeric values
    // ------------------------------------------------------------

    for (const auto& element : split_text)
    {
        std::stringstream element_stream(element);

        std::string key;

        if (!(element_stream >> key))
        {
            continue;
        }


        // --------------------------------------------------------
        // Ignore analog data from the controller we don't use
        // --------------------------------------------------------

        const bool is_left =
            key == "leftJS" ||
            key == "leftGrip" ||
            key == "leftTrig";

        const bool is_right =
            key == "rightJS" ||
            key == "rightGrip" ||
            key == "rightTrig";

        if (mode == ControllerMode::LEFT && is_right)
            continue;

        if (mode == ControllerMode::RIGHT && is_left)
            continue;


        // --------------------------------------------------------
        // Parse numerical values
        // --------------------------------------------------------

        std::vector<double> values;

        double value;

        while (element_stream >> value)
        {
            values.push_back(value);
        }

        if (values.empty())
        {
            continue;
        }

        buttons[key] = values;
    }

    return buttons;
}

} // namespace oculus_reader