#pragma once

#include <cstdint>
#include <chrono>

namespace ares {

// Common types used across modules
using Timestamp = std::chrono::time_point<std::chrono::steady_clock>;
using Duration = std::chrono::nanoseconds;

// Video frame format
enum class PixelFormat {
    UNKNOWN,
    YUV422_8BIT,
    YUV422_10BIT,
    RGB_8BIT,
    RGB_10BIT,
    RGB_16BIT_FLOAT
};

// HDR metadata types
enum class HDRType {
    NONE,
    HDR10,
    HLG,
    DOLBY_VISION
};

// HDR metadata structure (based on SMPTE ST 2086)
struct HDRMetadata {
    HDRType type = HDRType::NONE;

    // Mastering display metadata
    struct {
        uint16_t primary_r_x;
        uint16_t primary_r_y;
        uint16_t primary_g_x;
        uint16_t primary_g_y;
        uint16_t primary_b_x;
        uint16_t primary_b_y;
        uint16_t white_point_x;
        uint16_t white_point_y;
    } mastering_display;

    // Content light level
    uint16_t max_cll;  // Maximum Content Light Level (nits)
    uint16_t max_fall; // Maximum Frame-Average Light Level (nits)

    // Mastering display luminance
    uint32_t max_luminance; // cd/m² (nits)
    uint32_t min_luminance; // cd/m² * 10000
};

// Video frame structure
struct VideoFrame {
    uint8_t* data = nullptr;
    size_t size = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    PixelFormat format = PixelFormat::UNKNOWN;
    Timestamp pts;
    HDRMetadata hdr_metadata;
    bool interlaced = false;
};

// Display mode
struct DisplayMode {
    uint32_t width;
    uint32_t height;
    float refresh_rate;
    bool interlaced;
};

// Result type for error handling
enum class Result {
    SUCCESS = 0,
    ERROR_GENERIC = -1,
    ERROR_NOT_FOUND = -2,
    ERROR_INVALID_PARAMETER = -3,
    ERROR_NOT_INITIALIZED = -4,
    ERROR_DEVICE_LOST = -5,
    ERROR_OUT_OF_MEMORY = -6,
    ERROR_TIMEOUT = -7
};

} // namespace ares
