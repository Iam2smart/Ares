#pragma once

#include <cstdint>
#include <string>

namespace ares {

// Display mode configuration
struct DisplayMode {
    uint32_t width = 1920;
    uint32_t height = 1080;
    float refresh_rate = 60.0f;
    bool interlaced = false;

    // Timing parameters (for custom modes)
    uint32_t clock = 0;
    uint32_t htotal = 0;
    uint32_t hsync_start = 0;
    uint32_t hsync_end = 0;
    uint32_t vtotal = 0;
    uint32_t vsync_start = 0;
    uint32_t vsync_end = 0;
};

// Display configuration
struct DisplayConfig {
    // Display selection
    std::string connector = "auto";  // "auto", "HDMI-A-1", "DP-1", etc.
    std::string card = "/dev/dri/card0";  // DRM device

    // Display mode
    DisplayMode mode;
    bool auto_mode = true;  // Auto-detect best mode from EDID

    // Vsync settings
    bool vsync = true;
    bool adaptive_vsync = false;  // Allow tearing on missed frames

    // HDR settings
    bool hdr_output = false;  // Output HDR signal (if display supports)
    uint32_t hdr_eotf = 0;  // 0=SDR, 1=HDR10 (PQ), 2=HLG

    // Color settings
    enum class ColorSpace {
        BT709,
        BT2020,
        DCI_P3
    };
    ColorSpace output_color_space = ColorSpace::BT709;

    // Performance
    uint32_t buffer_count = 3;  // Triple buffering
    bool low_latency = false;  // Minimize latency (may drop frames)
};

} // namespace ares
