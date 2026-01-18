#pragma once

#include <ares/types.h>
#include <ares/display_config.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <vector>
#include <string>

namespace ares {
namespace display {

// DRM/KMS display manager
// Handles direct display output via Linux DRM subsystem
class DRMDisplay {
public:
    DRMDisplay();
    ~DRMDisplay();

    // Initialize DRM display
    Result initialize(const DisplayConfig& config);

    // Cleanup
    void shutdown();

    // Check if initialized
    bool isInitialized() const { return m_initialized; }

    // Get display information
    struct DisplayInfo {
        std::string connector_name;
        std::string display_name;       // From EDID
        std::string manufacturer;        // From EDID
        uint32_t connector_id;
        uint32_t crtc_id;
        uint32_t width;
        uint32_t height;
        float refresh_rate;

        // HDR capabilities from EDID
        bool hdr_supported;
        bool hdr10_supported;
        bool hlg_supported;
        bool dolby_vision_supported;
        uint16_t max_luminance;          // nits
        uint16_t min_luminance;          // 0.0001 nits units
        uint16_t max_cll;                // Max Content Light Level
        uint16_t max_fall;               // Max Frame Average Light Level

        std::vector<DisplayMode> available_modes;
    };

    DisplayInfo getDisplayInfo() const;

    // EDID parsing
    struct EDIDInfo {
        std::string manufacturer;
        std::string product_name;
        uint16_t product_code;
        uint32_t serial_number;

        // HDR capabilities
        bool hdr_supported;
        bool hdr10_supported;
        bool hlg_supported;
        bool dolby_vision_supported;
        uint16_t max_luminance;
        uint16_t min_luminance;
        uint16_t max_cll;
        uint16_t max_fall;

        // Color gamut
        float red_x, red_y;
        float green_x, green_y;
        float blue_x, blue_y;
        float white_x, white_y;
    };

    Result parseEDID(EDIDInfo& edid_info) const;

    // Mode management
    Result setMode(const DisplayMode& mode);
    DisplayMode getCurrentMode() const { return m_current_mode; }
    std::vector<DisplayMode> getAvailableModes() const;

    // Page flip (present frame)
    Result pageFlip(uint32_t fb_id, void* user_data = nullptr);

    // Wait for vblank
    Result waitForVBlank();

    // Get DRM file descriptor (for Vulkan integration)
    int getDrmFd() const { return m_drm_fd; }
    uint32_t getCrtcId() const { return m_crtc_id; }
    uint32_t getConnectorId() const { return m_connector_id; }

    // Statistics
    struct Stats {
        uint64_t frames_presented = 0;
        uint64_t vblank_waits = 0;
        uint64_t missed_vblanks = 0;
        double avg_frame_time_ms = 0.0;
        double last_frame_time_ms = 0.0;
    };

    Stats getStats() const { return m_stats; }

private:
    // Initialization helpers
    Result openDrmDevice(const std::string& card);
    Result findConnector(const std::string& connector_name);
    Result findEncoder();
    Result findCrtc();
    Result selectMode(const DisplayMode& requested_mode);
    Result setModeInternal();

    // Mode helpers
    bool modesMatch(const drmModeModeInfo& a, const DisplayMode& b) const;
    DisplayMode convertDrmMode(const drmModeModeInfo& drm_mode) const;

    // Page flip callback
    static void pageFlipHandler(int fd, unsigned int sequence,
                               unsigned int tv_sec, unsigned int tv_usec,
                               void* user_data);

    // DRM resources
    int m_drm_fd = -1;
    drmModeRes* m_resources = nullptr;
    drmModeConnector* m_connector = nullptr;
    drmModeEncoder* m_encoder = nullptr;
    drmModeCrtc* m_crtc = nullptr;
    drmModeCrtc* m_saved_crtc = nullptr;  // For restoration

    // IDs
    uint32_t m_connector_id = 0;
    uint32_t m_encoder_id = 0;
    uint32_t m_crtc_id = 0;

    // Current mode
    drmModeModeInfo m_drm_mode = {};
    DisplayMode m_current_mode;

    // Configuration
    DisplayConfig m_config;

    // Statistics
    Stats m_stats;

    // State
    bool m_initialized = false;
    bool m_page_flip_pending = false;
};

} // namespace display
} // namespace ares
