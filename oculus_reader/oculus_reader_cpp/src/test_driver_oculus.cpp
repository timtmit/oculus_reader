// Minimal test harness for OculusReader.
//
// Connects over USB (empty ip_address), starts the reader, and prints
// transforms + buttons in a loop s
//
// Run:
//   ./test_oculus_reader            # USB
//   ./test_oculus_reader 10.0.32.101 5555   # network (ip, port)

#include "oculus_reader.hpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>

namespace
{

void print_transform(char hand, const OculusReader::Transform& t)
{
    std::cout << "  [" << hand << "] pos=("
               << std::fixed << std::setprecision(3)
               << t(0, 3) << ", " << t(1, 3) << ", " << t(2, 3)
               << ")\n";
}

void print_button(const std::string& key, const oculus_reader::ButtonValue& value)
{
    std::cout << "  " << key << " = ";

    if (std::holds_alternative<bool>(value))
    {
        std::cout << (std::get<bool>(value) ? "true" : "false");
    }
    else
    {
        const auto& values = std::get<std::vector<double>>(value);

        std::cout << "[";

        for (std::size_t i = 0; i < values.size(); ++i)
        {
            std::cout << values[i];

            if (i + 1 < values.size())
                std::cout << ", ";
        }

        std::cout << "]";
    }

    std::cout << "\n";
}

} // namespace


int main(int argc, char** argv)
{
    std::string ip_address;
    int port = 5555;

    if (argc >= 2)
        ip_address = argv[1];

    if (argc >= 3)
        port = std::stoi(argv[2]);

    std::cout << "Connecting to Oculus oculus"
               << (ip_address.empty()
                       ? " over USB..."
                       : " at " + ip_address + ":" + std::to_string(port) + "...")
               << std::endl;

    try
    {
        OculusReader reader(
            ip_address,
            port,
            "com.rail.oculus.teleop",
            /*print_fps=*/false,
            /*run=*/true,
            ControllerMode::BOTH);

        std::cout << "Connected. Reading transforms/buttons "
                      "(Ctrl+C to stop earlier)...\n";
        int i = 0;
        while (true)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));

            auto [transforms, buttons] =
                reader.get_transformations_and_buttons();

            if (transforms.empty() && buttons.empty())
                continue;

            std::cout << "--- frame " << i << " ---\n";

            for (const auto& [hand, transform] : transforms)
                print_transform(hand, transform);

            for (const auto& [key, value] : buttons)
                print_button(key, value);
            i++;
        }

        reader.stop();
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}