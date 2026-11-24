#include "oculus_reader.hpp"
#include "oculus_button_parser.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <sys/wait.h>


namespace
{

std::string trim(const std::string& input)
{
    const auto begin =
        input.find_first_not_of(" \t\r\n");

    if (begin == std::string::npos)
        return "";

    const auto end =
        input.find_last_not_of(" \t\r\n");

    return input.substr(
        begin,
        end - begin + 1);
}


std::vector<std::string> split(
    const std::string& input,
    char delimiter)
{
    std::vector<std::string> result;

    std::stringstream stream(input);
    std::string item;

    while (std::getline(stream, item, delimiter))
        result.push_back(item);

    return result;
}


bool string_to_double(
    const std::string& value,
    double& result)
{
    try
    {
        std::size_t pos = 0;

        result = std::stod(value, &pos);

        return pos == value.size();
    }
    catch (...)
    {
        return false;
    }
}

} // namespace


OculusReader::OculusReader(
    const std::string& ip_address,
    int port,
    const std::string& apk_name,
    bool print_fps,
    bool run,
    ControllerMode controller_mode)
    : ip_address_(ip_address),
      port_(port),
      apk_name_(apk_name),
      print_fps_(print_fps),
      controller_mode_(controller_mode)
{
    device_ = get_device();

    if (!device_.valid)
    {
        throw std::runtime_error(
            "Could not find Oculus oculus device.");
    }

    if (!install("", false, false))
    {
        throw std::runtime_error(
            "Could not install/access Oculus Reader APK.");
    }

    if (run)
        this->run();
}


OculusReader::~OculusReader()
{
    stop();
}


void OculusReader::run()
{
    if (running_)
        return;

    running_ = true;

    if (!start_activity())
    {
        running_ = false;

        throw std::runtime_error(
            "Could not start Oculus Reader activity.");
    }

    // Best-effort: keep the headset from sleeping (and stopping the
    // stream) when it's set down instead of worn. Not fatal if it
    // fails -- the reader still works, it'll just sleep if unworn.
    if (!disable_sleep())
    {
        print_error(
            "Could not disable headset auto-sleep "
            "(proximity sensor override). The stream may stop "
            "if the headset isn't worn.");
    }

    thread_ = std::thread(
        &OculusReader::read_logcat,
        this);
}


void OculusReader::stop()
{
    running_ = false;

    if (thread_.joinable())
        thread_.join();

    // Best-effort: restore normal proximity-triggered sleep so the
    // headset doesn't stay awake indefinitely / drain battery after
    // we're done reading from it.
    restore_sleep();
}


bool OculusReader::is_running() const
{
    return running_;
}


OculusReader::AdbDevice
OculusReader::get_device()
{
    if (!ip_address_.empty())
        return get_network_device();

    return get_usb_device();
}


OculusReader::AdbDevice
OculusReader::get_usb_device()
{
    std::string output;

    if (!execute_adb_command(
            {"devices"},
            &output))
    {
        print_error(
            "Could not execute `adb devices`.");

        return {};
    }

    std::stringstream stream(output);
    std::string line;

    while (std::getline(stream, line))
    {
        line = trim(line);

        if (line.empty() ||
            line.find("List of devices") == 0)
        {
            continue;
        }

        std::stringstream line_stream(line);

        std::string serial;
        std::string state;

        line_stream >> serial >> state;

        if (state == "device")
        {
            const auto dot_count =
                std::count(
                    serial.begin(),
                    serial.end(),
                    '.');

            if (dot_count < 3)
            {
                return {
                    serial,
                    true
                };
            }
        }
    }

    print_error(
        "Device not found. "
        "Make sure the oculus is connected over USB.");

    return {};
}


OculusReader::AdbDevice
OculusReader::get_network_device(int retry)
{
    const std::string target =
        ip_address_ + ":" + std::to_string(port_);

    std::string output;

    if (!execute_adb_command(
            {
                "connect",
                target
            },
            &output))
    {
        if (retry == 0)
        {
            execute_adb_command({"devices"});

            return get_network_device(1);
        }

        if (retry == 1)
        {
            execute_adb_command(
                {
                    "tcpip",
                    std::to_string(port_)
                });

            return get_network_device(2);
        }

        print_error(
            "Could not connect to Oculus oculus at " +
            target);

        return {};
    }

    if (!execute_adb_command(
            {"devices"},
            &output))
    {
        return {};
    }

    std::stringstream stream(output);
    std::string line;

    while (std::getline(stream, line))
    {
        line = trim(line);

        if (line.find(target) != std::string::npos &&
            line.find("device") != std::string::npos)
        {
            return {
                target,
                true
            };
        }
    }

    if (retry < 2)
        return get_network_device(retry + 1);

    print_error(
        "Make sure that the oculus is running and "
        "available at IP address " +
        ip_address_);

    return {};
}


bool OculusReader::execute_adb_command(
    const std::vector<std::string>& args,
    std::string* output) const
{
    std::string command = "adb";

    for (const auto& arg : args)
    {
        command += " \"";

        for (const char c : arg)
        {
            if (c == '"')
                command += '\\';

            command += c;
        }

        command += "\"";
    }

    if (output)
    {
        FILE* pipe = popen(
            (command + " 2>&1").c_str(),
            "r");

        if (!pipe)
            return false;

        char buffer[4096];

        while (fgets(
            buffer,
            sizeof(buffer),
            pipe))
        {
            (*output) += buffer;
        }

        const int status = pclose(pipe);

        return WIFEXITED(status) &&
               WEXITSTATUS(status) == 0;
    }

    const int status =
        std::system(command.c_str());

    return status == 0;
}


bool OculusReader::broadcast_intent(const std::string& action) const
{
    std::string output;

    return execute_adb_command(
        {
            "-s",
            device_.serial,
            "shell",
            "am",
            "broadcast",
            "-a",
            action
        },
        &output);
}


bool OculusReader::disable_sleep()
{
    // automation_disable turns off the underlying auto-sleep logic;
    // prox_close then forces the proximity sensor to report "worn" so
    // the screen/stream stay active even with the headset set down.
    // automation_disable's effect is undocumented past this point (no
    // known "automation_enable" counterpart), so don't rely on it for
    // restoring normal behavior -- see restore_sleep().
    const bool automation_ok =
        broadcast_intent(
            "com.oculus.vrpowermanager.automation_disable");

    const bool prox_ok =
        broadcast_intent(
            "com.oculus.vrpowermanager.prox_close");

    return automation_ok && prox_ok;
}


bool OculusReader::restore_sleep()
{
    // prox_far simulates the headset being taken off, restoring the
    // normal proximity-triggered sleep behavior.
    return broadcast_intent(
        "com.oculus.vrpowermanager.prox_far");
}


bool OculusReader::start_activity()
{
    const std::string activity =
        apk_name_ +
        "/.MainActivity";

    std::string output;

    return execute_adb_command(
        {
            "-s",
            device_.serial,
            "shell",
            "am",
            "start",
            "-n",
            activity,
            "-a",
            "android.intent.action.MAIN",
            "-c",
            "android.intent.category.LAUNCHER"
        },
        &output);
}


bool OculusReader::install(
    const std::string& apk_path,
    bool verbose,
    bool reinstall)
{
    std::string output;

    if (execute_adb_command(
            {
                "-s",
                device_.serial,
                "shell",
                "pm",
                "list",
                "packages",
                apk_name_
            },
            &output))
    {
        const bool installed =
            output.find(
                "package:" + apk_name_) !=
            std::string::npos;

        if (installed && !reinstall)
        {
            if (verbose)
            {
                std::cout
                    << "APK is already installed."
                    << std::endl;
            }

            return true;
        }
    }

    if (apk_path.empty())
    {
        print_error(
            "APK is not installed and no APK path "
            "was provided.");

        return false;
    }

    std::vector<std::string> args{
        "-s",
        device_.serial,
        "install"
    };

    if (reinstall)
        args.push_back("-r");

    args.push_back(apk_path);

    if (!execute_adb_command(
            args,
            &output))
    {
        print_error(
            "APK installation failed:\n" +
            output);

        return false;
    }

    if (verbose)
    {
        std::cout
            << "APK installed successfully."
            << std::endl;
    }

    return true;
}


bool OculusReader::uninstall(bool verbose)
{
    std::string output;

    if (!execute_adb_command(
            {
                "-s",
                device_.serial,
                "shell",
                "pm",
                "list",
                "packages",
                apk_name_
            },
            &output))
    {
        return false;
    }

    if (output.find(
            "package:" + apk_name_) ==
        std::string::npos)
    {
        if (verbose)
        {
            std::cout
                << "APK is not installed."
                << std::endl;
        }

        return true;
    }

    if (!execute_adb_command(
            {
                "-s",
                device_.serial,
                "uninstall",
                apk_name_
            },
            &output))
    {
        print_error(
            "APK uninstall failed:\n" +
            output);

        return false;
    }

    if (verbose)
    {
        std::cout
            << "APK uninstall finished."
            << std::endl;
    }

    return true;
}


std::string OculusReader::extract_data(
    const std::string& line) const
{
    const std::string search =
        tag_ + ": ";

    const auto position =
        line.find(search);

    if (position == std::string::npos)
        return "";

    return line.substr(
        position + search.size());
}


std::pair<
    OculusReader::Transforms,
    OculusReader::Buttons>
OculusReader::process_data(
    const std::string& data)
{
    Transforms transforms;
    Buttons buttons;

    const auto ampersand =
        data.find('&');

    if (ampersand == std::string::npos)
        return {
            transforms,
            buttons
        };

    const std::string transforms_string =
        data.substr(
            0,
            ampersand);

    const std::string buttons_string =
        data.substr(
            ampersand + 1);


    // ------------------------------------------------------------
    // Parse transformations
    // ------------------------------------------------------------

    const auto transform_strings =
        split(
            transforms_string,
            '|');

    for (const auto& pair_string :
         transform_strings)
    {
        const auto separator =
            pair_string.find(':');

        if (separator == std::string::npos)
            continue;

        if (separator == 0)
            continue;

        const char left_right_char =
            pair_string[0];


        // --------------------------------------------------------
        // Controller filtering
        // --------------------------------------------------------

        if (controller_mode_ == ControllerMode::LEFT &&
            left_right_char != 'l')
        {
            continue;
        }

        if (controller_mode_ == ControllerMode::RIGHT &&
            left_right_char != 'r')
        {
            continue;
        }


        const std::string transform_string =
            pair_string.substr(
                separator + 1);

        Transform transform;

        if (parse_transform(
                transform_string,
                transform))
        {
            transforms[left_right_char] =
                transform;
        }
    }


    // ------------------------------------------------------------
    // Parse buttons
    //
    // parse_buttons() is responsible for filtering the
    // reoculused controller.
    // ------------------------------------------------------------

    buttons =
        parse_buttons(
            buttons_string,
            controller_mode_);


    return {
        transforms,
        buttons
    };
}


bool OculusReader::parse_transform(
    const std::string& value,
    Transform& transform)
{
    transform.setZero();

    std::stringstream stream(value);

    std::string token;

    int count = 0;

    while (stream >> token)
    {
        double number;

        if (!string_to_double(
                token,
                number))
        {
            return false;
        }

        if (count >= 16)
            return false;

        const int row = count / 4;
        const int col = count % 4;

        transform(row, col) = number;

        ++count;
    }

    return count == 16;
}


void OculusReader::read_logcat()
{
    const std::string command =
        "adb -s \"" +
        device_.serial +
        "\" shell logcat -T 0";

    FILE* pipe =
        popen(
            command.c_str(),
            "r");

    if (!pipe)
    {
        print_error(
            "Could not start adb logcat.");

        running_ = false;

        return;
    }

    char buffer[8192];

    while (running_)
    {
        if (!fgets(
                buffer,
                sizeof(buffer),
                pipe))
        {
            break;
        }

        std::string line =
            trim(buffer);

        const std::string data =
            extract_data(line);

        if (data.empty())
            continue;

        auto [transforms, buttons] =
            process_data(data);

        update_state(
            transforms,
            buttons);
    }

    pclose(pipe);
}


void OculusReader::update_state(
    const Transforms& transforms,
    const Buttons& buttons)
{
    std::lock_guard<std::mutex> lock(
        mutex_);

    last_transforms_ = transforms;
    last_buttons_ = buttons;
}


std::pair<
    OculusReader::Transforms,
    OculusReader::Buttons>
OculusReader::get_transformations_and_buttons() const
{
    std::lock_guard<std::mutex> lock(
        mutex_);

    return {
        last_transforms_,
        last_buttons_
    };
}


void OculusReader::print_error(
    const std::string& message)
{
    std::cerr
        << "\033[1;31m"
        << message
        << "\033[0m"
        << std::endl;
}