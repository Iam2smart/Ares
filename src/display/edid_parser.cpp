#include "edid_parser.h"
#include "core/logger.h"
#include <fstream>
#include <cstring>
#include <cmath>
#include <algorithm>

namespace ares {
namespace display {

// EDID structure constants
#define EDID_HEADER_SIZE 8
#define EDID_BASE_BLOCK_SIZE 128
#define EDID_EXTENSION_BLOCK_SIZE 128
#define EDID_STANDARD_TIMING_COUNT 8
#define EDID_DETAILED_TIMING_COUNT 4

// CEA-861 data block tags
#define CEA_TAG_AUDIO 1
#define CEA_TAG_VIDEO 2
#define CEA_TAG_VENDOR 3
#define CEA_TAG_SPEAKER 4
#define CEA_TAG_EXTENDED 7

// CEA-861 extended tags
#define CEA_EXT_TAG_VIDEO_CAPABILITY 0
#define CEA_EXT_TAG_VENDOR_VIDEO 1
#define CEA_EXT_TAG_COLORIMETRY 5
#define CEA_EXT_TAG_HDR_STATIC_METADATA 6
#define CEA_EXT_TAG_HDR_DYNAMIC_METADATA 7
#define CEA_EXT_TAG_YCBCR420_VIDEO 14
#define CEA_EXT_TAG_YCBCR420_CAPABILITY 15

EDIDParser::EDIDParser() {
}

EDIDParser::~EDIDParser() {
}

Result EDIDParser::parseEDID(const std::string& edid_path, std::vector<DisplayMode>& modes) {
    LOG_INFO("Display", "Parsing EDID from %s", edid_path.c_str());

    // Read EDID binary data from sysfs
    std::ifstream file(edid_path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOG_ERROR("Display", "Failed to open EDID file: %s", edid_path.c_str());
        return Result::ERROR_FILE_NOT_FOUND;
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    if (size < EDID_BASE_BLOCK_SIZE) {
        LOG_ERROR("Display", "EDID file too small: %ld bytes", size);
        return Result::ERROR_INVALID_DATA;
    }

    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        LOG_ERROR("Display", "Failed to read EDID file");
        return Result::ERROR_READ_FAILED;
    }

    return parseEDIDData(buffer.data(), size, modes);
}

Result EDIDParser::parseEDIDData(const uint8_t* data, size_t size, std::vector<DisplayMode>& modes) {
    if (size < EDID_BASE_BLOCK_SIZE) {
        LOG_ERROR("Display", "EDID data too small: %zu bytes", size);
        return Result::ERROR_INVALID_DATA;
    }

    // Verify EDID header (00 FF FF FF FF FF FF 00)
    const uint8_t edid_header[EDID_HEADER_SIZE] = {0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00};
    if (std::memcmp(data, edid_header, EDID_HEADER_SIZE) != 0) {
        LOG_ERROR("Display", "Invalid EDID header");
        return Result::ERROR_INVALID_DATA;
    }

    // Verify base block checksum
    if (!verifyChecksum(data, EDID_BASE_BLOCK_SIZE)) {
        LOG_ERROR("Display", "EDID base block checksum failed");
        return Result::ERROR_INVALID_DATA;
    }

    // Parse base block
    Result result = parseBaseBlock(data);
    if (result != Result::SUCCESS) {
        return result;
    }

    // Parse standard timings
    parseStandardTimings(data + 38, modes);

    // Parse detailed timing descriptors
    for (int i = 0; i < EDID_DETAILED_TIMING_COUNT; i++) {
        parseDetailedTiming(data + 54 + (i * 18), modes);
    }

    // Parse extension blocks
    uint8_t extension_count = data[126];
    LOG_INFO("Display", "EDID has %d extension block(s)", extension_count);

    for (uint8_t i = 0; i < extension_count && (i + 1) * EDID_BASE_BLOCK_SIZE < size; i++) {
        const uint8_t* ext_data = data + ((i + 1) * EDID_BASE_BLOCK_SIZE);
        uint8_t ext_tag = ext_data[0];

        // Verify extension block checksum
        if (!verifyChecksum(ext_data, EDID_EXTENSION_BLOCK_SIZE)) {
            LOG_WARN("Display", "Extension block %d checksum failed", i);
            continue;
        }

        // CEA-861 extension (tag = 0x02)
        if (ext_tag == 0x02) {
            LOG_INFO("Display", "Parsing CEA-861 extension block %d", i);
            parseCEAExtension(ext_data);
        }
    }

    // Log parsed capabilities
    LOG_INFO("Display", "Display: %s %s", m_capabilities.manufacturer.c_str(),
             m_capabilities.model_name.c_str());
    LOG_INFO("Display", "Manufactured: week %d, year %d",
             m_capabilities.manufacture_week, m_capabilities.manufacture_year);
    LOG_INFO("Display", "HDR10: %s, Dolby Vision: %s, HLG: %s",
             m_capabilities.supports_hdr10 ? "yes" : "no",
             m_capabilities.supports_dolby_vision ? "yes" : "no",
             m_capabilities.supports_hlg ? "yes" : "no");

    if (m_capabilities.supports_hdr10) {
        LOG_INFO("Display", "HDR luminance: max=%.0f cd/m², min=%.4f cd/m²",
                 m_capabilities.max_luminance, m_capabilities.min_luminance);
    }

    LOG_INFO("Display", "BT.2020: %s, DCI-P3: %s",
             m_capabilities.supports_bt2020 ? "yes" : "no",
             m_capabilities.supports_dcip3 ? "yes" : "no");

    if (m_capabilities.supports_vrr) {
        LOG_INFO("Display", "VRR: %d-%d Hz",
                 m_capabilities.vrr_min_refresh, m_capabilities.vrr_max_refresh);
    }

    LOG_INFO("Display", "Found %zu display modes", modes.size());

    return Result::SUCCESS;
}

Result EDIDParser::parseBaseBlock(const uint8_t* data) {
    // Manufacturer ID (bytes 8-9)
    uint16_t mfg_id = (data[8] << 8) | data[9];
    m_capabilities.manufacturer = decodeManufacturerID(mfg_id);

    // Product code (bytes 10-11)
    uint16_t product_code = data[10] | (data[11] << 8);
    (void)product_code;  // Used for identification but not stored separately

    // Serial number (bytes 12-15)
    m_capabilities.serial_number = data[12] | (data[13] << 8) |
                                   (data[14] << 16) | (data[15] << 24);

    // Manufacture week and year (bytes 16-17)
    m_capabilities.manufacture_week = data[16];
    m_capabilities.manufacture_year = 1990 + data[17];

    // EDID version (bytes 18-19)
    uint8_t edid_version = data[18];
    uint8_t edid_revision = data[19];
    LOG_DEBUG("Display", "EDID version %d.%d", edid_version, edid_revision);

    return Result::SUCCESS;
}

void EDIDParser::parseStandardTimings(const uint8_t* data, std::vector<DisplayMode>& modes) {
    for (int i = 0; i < EDID_STANDARD_TIMING_COUNT; i++) {
        uint8_t byte1 = data[i * 2];
        uint8_t byte2 = data[i * 2 + 1];

        // Skip unused entries (0x01 0x01)
        if (byte1 == 0x01 && byte2 == 0x01) {
            continue;
        }

        // Calculate horizontal resolution: (byte1 + 31) * 8
        uint32_t h_res = (byte1 + 31) * 8;

        // Calculate aspect ratio and vertical resolution
        uint8_t aspect_ratio = (byte2 >> 6) & 0x03;
        uint32_t v_res = 0;

        switch (aspect_ratio) {
            case 0: v_res = (h_res * 10) / 16; break;  // 16:10
            case 1: v_res = (h_res * 3) / 4; break;    // 4:3
            case 2: v_res = (h_res * 4) / 5; break;    // 5:4
            case 3: v_res = (h_res * 9) / 16; break;   // 16:9
        }

        // Calculate refresh rate: (byte2 & 0x3F) + 60
        float refresh_rate = (byte2 & 0x3F) + 60.0f;

        DisplayMode mode;
        mode.width = h_res;
        mode.height = v_res;
        mode.refresh_rate = refresh_rate;
        mode.interlaced = false;

        modes.push_back(mode);
        LOG_DEBUG("Display", "Standard timing: %dx%d@%.0fHz", h_res, v_res, refresh_rate);
    }
}

void EDIDParser::parseDetailedTiming(const uint8_t* data, std::vector<DisplayMode>& modes) {
    // Check if this is a detailed timing descriptor (pixel clock != 0)
    uint16_t pixel_clock = data[0] | (data[1] << 8);
    if (pixel_clock == 0) {
        // This is a display descriptor, not a timing
        uint8_t descriptor_type = data[3];

        // Monitor name descriptor (type 0xFC)
        if (descriptor_type == 0xFC) {
            char name[14];
            std::memcpy(name, data + 5, 13);
            name[13] = '\0';

            // Remove trailing spaces and newlines
            for (int i = 12; i >= 0; i--) {
                if (name[i] == ' ' || name[i] == '\n' || name[i] == '\r') {
                    name[i] = '\0';
                } else {
                    break;
                }
            }

            m_capabilities.model_name = name;
            LOG_DEBUG("Display", "Monitor name: %s", name);
        }

        return;
    }

    // Parse detailed timing
    uint32_t h_active = data[2] | ((data[4] & 0xF0) << 4);
    uint32_t v_active = data[5] | ((data[7] & 0xF0) << 4);

    uint32_t h_blank = data[3] | ((data[4] & 0x0F) << 8);
    uint32_t v_blank = data[6] | ((data[7] & 0x0F) << 8);

    // Calculate refresh rate from pixel clock and total pixels
    uint32_t h_total = h_active + h_blank;
    uint32_t v_total = v_active + v_blank;
    float refresh_rate = (pixel_clock * 10000.0f) / (h_total * v_total);

    // Check for interlaced
    bool interlaced = (data[17] & 0x80) != 0;

    DisplayMode mode;
    mode.width = h_active;
    mode.height = v_active;
    mode.refresh_rate = refresh_rate;
    mode.interlaced = interlaced;

    modes.push_back(mode);
    LOG_DEBUG("Display", "Detailed timing: %dx%d@%.2fHz%s",
              h_active, v_active, refresh_rate, interlaced ? "i" : "");
}

void EDIDParser::parseCEAExtension(const uint8_t* data) {
    uint8_t revision = data[1];
    uint8_t dtd_offset = data[2];  // Offset to detailed timing descriptors

    LOG_DEBUG("Display", "CEA-861 revision %d", revision);

    // Parse data blocks (from byte 4 to dtd_offset)
    size_t pos = 4;
    while (pos < dtd_offset && pos < EDID_EXTENSION_BLOCK_SIZE) {
        uint8_t block_header = data[pos];
        uint8_t tag = (block_header >> 5) & 0x07;
        uint8_t length = block_header & 0x1F;

        if (pos + length + 1 > dtd_offset) {
            LOG_WARN("Display", "CEA data block exceeds DTD offset");
            break;
        }

        const uint8_t* block_data = data + pos + 1;

        // Extended tag block
        if (tag == CEA_TAG_EXTENDED && length > 0) {
            uint8_t ext_tag = block_data[0];

            switch (ext_tag) {
                case CEA_EXT_TAG_COLORIMETRY:
                    // Colorimetry data block
                    if (length >= 2) {
                        uint8_t colorimetry = block_data[1];
                        m_capabilities.supports_bt2020 = (colorimetry & 0x08) != 0;  // BT.2020 cYCC
                        m_capabilities.supports_dcip3 = (colorimetry & 0x80) != 0;   // DCI-P3
                    }
                    break;

                case CEA_EXT_TAG_HDR_STATIC_METADATA:
                    // HDR static metadata block
                    parseHDRStaticMetadata(block_data, length);
                    break;

                case CEA_EXT_TAG_VIDEO_CAPABILITY:
                    // Video capability block (includes VRR info in some implementations)
                    if (length >= 1) {
                        // VRR support would be indicated here in some displays
                    }
                    break;
            }
        }
        // Vendor-specific data block
        else if (tag == CEA_TAG_VENDOR && length >= 3) {
            // IEEE OUI (3 bytes, little-endian)
            uint32_t oui = block_data[0] | (block_data[1] << 8) | (block_data[2] << 16);

            // HDMI Vendor-Specific Data Block (HDMI Forum OUI: 0xC45DD8)
            if (oui == 0xC45DD8 && length >= 7) {
                // Check for VRR support (HF-VSDB)
                if (length >= 8) {
                    uint8_t vrr_byte = block_data[7];
                    m_capabilities.supports_vrr = (vrr_byte & 0x40) != 0;

                    if (m_capabilities.supports_vrr && length >= 10) {
                        m_capabilities.vrr_min_refresh = block_data[8] & 0x3F;
                        m_capabilities.vrr_max_refresh = block_data[9] * 2;
                    }
                }
            }
        }

        pos += length + 1;
    }
}

void EDIDParser::parseHDRStaticMetadata(const uint8_t* data, size_t length) {
    if (length < 3) {
        return;
    }

    // Byte 1: Electro-Optical Transfer Functions (EOTFs)
    uint8_t eotf = data[1];
    m_capabilities.supports_hdr10 = (eotf & 0x04) != 0;  // SMPTE ST 2084 (PQ)
    m_capabilities.supports_hlg = (eotf & 0x08) != 0;    // HLG

    // Byte 2: Static Metadata Descriptors
    uint8_t metadata_type = data[2];
    (void)metadata_type;  // Type 1 is most common

    // Optional bytes for luminance info
    if (length >= 4) {
        // Desired content max luminance (byte 3)
        // Value encoding: 50 * 2^(byte / 32) cd/m²
        uint8_t max_lum_byte = data[3];
        if (max_lum_byte > 0) {
            m_capabilities.max_luminance = 50.0f * powf(2.0f, max_lum_byte / 32.0f);
        }
    }

    if (length >= 5) {
        // Desired content max frame-average luminance (byte 4)
        uint8_t max_fal_byte = data[4];
        if (max_fal_byte > 0) {
            m_capabilities.max_frame_avg_luminance = 50.0f * powf(2.0f, max_fal_byte / 32.0f);
        }
    }

    if (length >= 6) {
        // Desired content min luminance (byte 5)
        // Value encoding: max_luminance * (min_lum_byte / 255)² / 100 cd/m²
        uint8_t min_lum_byte = data[5];
        if (min_lum_byte > 0 && m_capabilities.max_luminance > 0) {
            float ratio = min_lum_byte / 255.0f;
            m_capabilities.min_luminance = m_capabilities.max_luminance * (ratio * ratio) / 100.0f;
        }
    }

    LOG_DEBUG("Display", "HDR capabilities: EOTF=0x%02X, Metadata=0x%02X", eotf, metadata_type);
}

bool EDIDParser::verifyChecksum(const uint8_t* data, size_t size) {
    uint8_t sum = 0;
    for (size_t i = 0; i < size; i++) {
        sum += data[i];
    }
    return (sum == 0);
}

std::string EDIDParser::decodeManufacturerID(uint16_t id) {
    // Manufacturer ID is 3 packed 5-bit characters (A-Z)
    // Bit 15 is reserved (0), bits 14-10 = 1st char, 9-5 = 2nd, 4-0 = 3rd
    char mfg[4];
    mfg[0] = '@' + ((id >> 10) & 0x1F);
    mfg[1] = '@' + ((id >> 5) & 0x1F);
    mfg[2] = '@' + (id & 0x1F);
    mfg[3] = '\0';
    return std::string(mfg);
}

} // namespace display
} // namespace ares
