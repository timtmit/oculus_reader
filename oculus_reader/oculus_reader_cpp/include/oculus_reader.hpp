#pragma once

#include <Eigen/Dense>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>
#include "oculus_button_parser.hpp"
#include "oculus_controller_mode.hpp"

using ControllerMode = oculus_reader::ControllerMode;

class OculusReader
{
public:
    using Transform = Eigen::Matrix4d;
    using Transforms = std::unordered_map<char, Transform>;
    using Buttons = oculus_reader::Buttons;

    OculusReader(
        const std::string& ip_address,
        int port,
        const std::string& apk_name,
        bool print_fps,
        bool run,
        ControllerMode controller_mode);

    ~OculusReader();

    OculusReader(const OculusReader&) = delete;
    OculusReader& operator=(const OculusReader&) = delete;

    void run();
    void stop();

    bool install(
        const std::string& apk_path = "",
        bool verbose = true,
        bool reinstall = false);

    bool uninstall(bool verbose = true);

    // ----------------------------------------------------------------
    // Prevents the headset from going to sleep (and thus stopping the
    // data stream) when it isn't being worn, by faking a "worn" state
    // to the proximity sensor. Call disable_sleep() once after run(),
    // and restore_sleep() when you're done testing / before leaving
    // the headset unattended for a while.
    // ----------------------------------------------------------------
    bool disable_sleep();
    bool restore_sleep();

    std::pair<Transforms, Buttons>
    get_transformations_and_buttons() const;

    std::pair<Transforms, Buttons>
    process_data(const std::string& data);

    std::string extract_data(const std::string& line) const;

    bool is_running() const;

private:
    struct AdbDevice
    {
        std::string serial;
        bool valid{false};
    };

    AdbDevice get_device();

    AdbDevice get_network_device(int retry = 0);
    AdbDevice get_usb_device();

    bool execute_adb_command(
        const std::vector<std::string>& args,
        std::string* output = nullptr) const;

    bool start_activity();

    bool broadcast_intent(const std::string& action) const;

    void read_logcat();

    void update_state(
        const Transforms& transforms,
        const Buttons& buttons);

    static bool parse_transform(
        const std::string& value,
        Transform& transform);

    static void print_error(const std::string& message);

private:
    std::atomic<bool> running_{false};

    Transforms last_transforms_;
    Buttons last_buttons_;

    mutable std::mutex mutex_;

    std::thread thread_;

    ControllerMode controller_mode_;
    std::string ip_address_;
    int port_;

    std::string apk_name_;

    bool print_fps_;

    std::string tag_{"wE9ryARX"};

    AdbDevice device_;
};