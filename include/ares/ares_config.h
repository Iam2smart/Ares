#pragma once

#include <ares/types.h>
#include <ares/processing_config.h>
#include <ares/display_config.h>
#include <ares/osd_config.h>
#include <string>

namespace ares {

// Capture configuration
struct CaptureConfig {
    int device_index = 0;
    std::string input_connection = "HDMI";
    int buffer_size = 3;
};

// Receiver configuration
struct ReceiverConfig {
    bool enabled = false;
    std::string ip_address = "192.168.1.100";
    uint16_t port = 60128;
    int max_volume = 80;
    bool monitoring_enabled = true;

    // Volume display
    bool show_on_change = true;
    int display_duration_ms = 3000;
    int fade_duration_ms = 500;
};

// Master configuration structure
struct AresConfig {
    CaptureConfig capture;
    ProcessingConfig processing;
    DisplayConfig display;
    OSDConfig osd;
    ReceiverConfig receiver;

    // Logging
    std::string log_level = "INFO";  // DEBUG, INFO, WARN, ERROR
    bool log_to_file = true;
    std::string log_file = "/var/log/ares/ares.log";

    // System
    bool daemon_mode = false;
    int thread_count = 4;  // Processing threads
};

} // namespace ares
