#pragma once

#include <ares/types.h>
#include <vector>
#include <string>
#include <cstdint>

namespace ares {
namespace display {

// EDID display capabilities
struct EDIDCapabilities {
    std::string manufacturer;
    std::string model_name;
    uint32_t serial_number = 0;
    uint16_t manufacture_year = 0;
    uint8_t manufacture_week = 0;

    // HDR capabilities (from CEA-861.3 extension)
    bool supports_hdr10 = false;
    bool supports_dolby_vision = false;
    bool supports_hlg = false;
    float max_luminance = 0.0f;      // cd/m²
    float max_frame_avg_luminance = 0.0f;
    float min_luminance = 0.0f;

    // Color space support
    bool supports_bt2020 = false;
    bool supports_dcip3 = false;

    // VRR capabilities
    bool supports_vrr = false;
    uint32_t vrr_min_refresh = 0;
    uint32_t vrr_max_refresh = 0;
};

class EDIDParser {
public:
    EDIDParser();
    ~EDIDParser();

    // Parse EDID from sysfs path (e.g., /sys/class/drm/card0-HDMI-A-1/edid)
    Result parseEDID(const std::string& edid_path, std::vector<DisplayMode>& modes);

    // Parse EDID from raw binary data
    Result parseEDIDData(const uint8_t* data, size_t size, std::vector<DisplayMode>& modes);

    // Get display capabilities from last parsed EDID
    const EDIDCapabilities& getCapabilities() const { return m_capabilities; }

private:
    // Parse base EDID block (128 bytes)
    Result parseBaseBlock(const uint8_t* data);

    // Parse standard timings
    void parseStandardTimings(const uint8_t* data, std::vector<DisplayMode>& modes);

    // Parse detailed timing descriptors
    void parseDetailedTiming(const uint8_t* data, std::vector<DisplayMode>& modes);

    // Parse CEA-861 extension block (for HDR capabilities)
    void parseCEAExtension(const uint8_t* data);

    // Parse HDR static metadata from CEA extension
    void parseHDRStaticMetadata(const uint8_t* data, size_t length);

    // Verify EDID checksum
    bool verifyChecksum(const uint8_t* data, size_t size);

    // Decode manufacturer ID (3 packed 5-bit characters)
    std::string decodeManufacturerID(uint16_t id);

    EDIDCapabilities m_capabilities;
};

} // namespace display
} // namespace ares
