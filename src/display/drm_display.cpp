#include "drm_display.h"
#include "core/logger.h"
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <cmath>
#include <chrono>
#include <sys/poll.h>

namespace ares {
namespace display {

DRMDisplay::DRMDisplay() {
    LOG_INFO("Display", "DRMDisplay created");
}

DRMDisplay::~DRMDisplay() {
    shutdown();
}

Result DRMDisplay::initialize(const DisplayConfig& config) {
    if (m_initialized) {
        LOG_WARN("Display", "DRM display already initialized");
        return Result::SUCCESS;
    }

    m_config = config;

    LOG_INFO("Display", "Initializing DRM display");
    LOG_INFO("Display", "Card: %s", config.card.c_str());
    LOG_INFO("Display", "Connector: %s", config.connector.c_str());

    // Open DRM device
    Result result = openDrmDevice(config.card);
    if (result != Result::SUCCESS) {
        return result;
    }

    // Find connector
    result = findConnector(config.connector);
    if (result != Result::SUCCESS) {
        shutdown();
        return result;
    }

    // Find encoder
    result = findEncoder();
    if (result != Result::SUCCESS) {
        shutdown();
        return result;
    }

    // Find CRTC
    result = findCrtc();
    if (result != Result::SUCCESS) {
        shutdown();
        return result;
    }

    // Select and set mode
    result = selectMode(config.mode);
    if (result != Result::SUCCESS) {
        shutdown();
        return result;
    }

    m_initialized = true;

    // Log display info
    auto info = getDisplayInfo();
    LOG_INFO("Display", "Display initialized successfully");
    LOG_INFO("Display", "Connector: %s (ID: %u)", info.connector_name.c_str(), info.connector_id);
    LOG_INFO("Display", "Mode: %ux%u@%.2fHz", info.width, info.height, info.refresh_rate);
    LOG_INFO("Display", "HDR supported: %s", info.hdr_supported ? "yes" : "no");

    return Result::SUCCESS;
}

Result DRMDisplay::openDrmDevice(const std::string& card) {
    m_drm_fd = open(card.c_str(), O_RDWR | O_CLOEXEC);
    if (m_drm_fd < 0) {
        LOG_ERROR("Display", "Failed to open DRM device %s: %s", card.c_str(), strerror(errno));
        return Result::ERROR_OPEN_FAILED;
    }

    // Get DRM resources
    m_resources = drmModeGetResources(m_drm_fd);
    if (!m_resources) {
        LOG_ERROR("Display", "Failed to get DRM resources");
        close(m_drm_fd);
        m_drm_fd = -1;
        return Result::ERROR_GENERIC;
    }

    LOG_INFO("Display", "DRM device opened: %d connectors, %d encoders, %d CRTCs",
             m_resources->count_connectors, m_resources->count_encoders, m_resources->count_crtcs);

    return Result::SUCCESS;
}

Result DRMDisplay::findConnector(const std::string& connector_name) {
    // Try each connector
    for (int i = 0; i < m_resources->count_connectors; i++) {
        m_connector = drmModeGetConnector(m_drm_fd, m_resources->connectors[i]);
        if (!m_connector) {
            continue;
        }

        // Check if connected
        if (m_connector->connection != DRM_MODE_CONNECTED) {
            drmModeFreeConnector(m_connector);
            m_connector = nullptr;
            continue;
        }

        // Get connector type name
        const char* type_name = drmModeGetConnectorTypeName(m_connector->connector_type);
        char full_name[64];
        snprintf(full_name, sizeof(full_name), "%s-%d",
                type_name, m_connector->connector_type_id);

        LOG_DEBUG("Display", "Found connected connector: %s", full_name);

        // Check if this is the requested connector
        if (connector_name == "auto" || connector_name == full_name) {
            m_connector_id = m_connector->connector_id;
            LOG_INFO("Display", "Selected connector: %s (ID: %u)",
                    full_name, m_connector_id);
            return Result::SUCCESS;
        }

        drmModeFreeConnector(m_connector);
        m_connector = nullptr;
    }

    LOG_ERROR("Display", "No suitable connector found");
    return Result::ERROR_NOT_FOUND;
}

Result DRMDisplay::findEncoder() {
    if (!m_connector->encoder_id) {
        // Try first available encoder
        if (m_connector->count_encoders > 0) {
            m_encoder = drmModeGetEncoder(m_drm_fd, m_connector->encoders[0]);
        }
    } else {
        m_encoder = drmModeGetEncoder(m_drm_fd, m_connector->encoder_id);
    }

    if (!m_encoder) {
        LOG_ERROR("Display", "Failed to get encoder");
        return Result::ERROR_NOT_FOUND;
    }

    m_encoder_id = m_encoder->encoder_id;
    LOG_INFO("Display", "Found encoder (ID: %u)", m_encoder_id);

    return Result::SUCCESS;
}

Result DRMDisplay::findCrtc() {
    if (m_encoder->crtc_id) {
        m_crtc_id = m_encoder->crtc_id;
        m_crtc = drmModeGetCrtc(m_drm_fd, m_crtc_id);
    } else {
        // Find available CRTC
        for (int i = 0; i < m_resources->count_crtcs; i++) {
            if (m_encoder->possible_crtcs & (1 << i)) {
                m_crtc_id = m_resources->crtcs[i];
                m_crtc = drmModeGetCrtc(m_drm_fd, m_crtc_id);
                break;
            }
        }
    }

    if (!m_crtc) {
        LOG_ERROR("Display", "Failed to get CRTC");
        return Result::ERROR_NOT_FOUND;
    }

    // Save current CRTC for restoration
    m_saved_crtc = drmModeGetCrtc(m_drm_fd, m_crtc_id);

    LOG_INFO("Display", "Found CRTC (ID: %u)", m_crtc_id);

    return Result::SUCCESS;
}

Result DRMDisplay::selectMode(const DisplayMode& requested_mode) {
    if (m_config.auto_mode) {
        // Use preferred mode
        for (int i = 0; i < m_connector->count_modes; i++) {
            if (m_connector->modes[i].type & DRM_MODE_TYPE_PREFERRED) {
                m_drm_mode = m_connector->modes[i];
                m_current_mode = convertDrmMode(m_drm_mode);
                LOG_INFO("Display", "Using preferred mode: %ux%u@%.2fHz",
                        m_current_mode.width, m_current_mode.height,
                        m_current_mode.refresh_rate);
                return setModeInternal();
            }
        }

        // No preferred mode, use first available
        if (m_connector->count_modes > 0) {
            m_drm_mode = m_connector->modes[0];
            m_current_mode = convertDrmMode(m_drm_mode);
            LOG_INFO("Display", "Using first available mode: %ux%u@%.2fHz",
                    m_current_mode.width, m_current_mode.height,
                    m_current_mode.refresh_rate);
            return setModeInternal();
        }
    } else {
        // Find matching mode
        for (int i = 0; i < m_connector->count_modes; i++) {
            if (modesMatch(m_connector->modes[i], requested_mode)) {
                m_drm_mode = m_connector->modes[i];
                m_current_mode = convertDrmMode(m_drm_mode);
                LOG_INFO("Display", "Found matching mode: %ux%u@%.2fHz",
                        m_current_mode.width, m_current_mode.height,
                        m_current_mode.refresh_rate);
                return setModeInternal();
            }
        }

        LOG_ERROR("Display", "Requested mode not available: %ux%u@%.2fHz",
                 requested_mode.width, requested_mode.height, requested_mode.refresh_rate);
        return Result::ERROR_NOT_FOUND;
    }

    LOG_ERROR("Display", "No modes available");
    return Result::ERROR_NOT_FOUND;
}

Result DRMDisplay::setModeInternal() {
    // Note: Actual mode setting is deferred until first framebuffer is ready
    // This just validates that we have a mode selected
    return Result::SUCCESS;
}

bool DRMDisplay::modesMatch(const drmModeModeInfo& a, const DisplayMode& b) const {
    // Check resolution
    if (a.hdisplay != b.width || a.vdisplay != b.height) {
        return false;
    }

    // Check refresh rate (with tolerance)
    float refresh = (float)a.vrefresh;
    if (std::abs(refresh - b.refresh_rate) > 0.5f) {
        return false;
    }

    // Check interlaced flag
    bool interlaced = (a.flags & DRM_MODE_FLAG_INTERLACE) != 0;
    if (interlaced != b.interlaced) {
        return false;
    }

    return true;
}

DisplayMode DRMDisplay::convertDrmMode(const drmModeModeInfo& drm_mode) const {
    DisplayMode mode;
    mode.width = drm_mode.hdisplay;
    mode.height = drm_mode.vdisplay;
    mode.refresh_rate = (float)drm_mode.vrefresh;
    mode.interlaced = (drm_mode.flags & DRM_MODE_FLAG_INTERLACE) != 0;

    // Copy timing parameters
    mode.clock = drm_mode.clock;
    mode.htotal = drm_mode.htotal;
    mode.hsync_start = drm_mode.hsync_start;
    mode.hsync_end = drm_mode.hsync_end;
    mode.vtotal = drm_mode.vtotal;
    mode.vsync_start = drm_mode.vsync_start;
    mode.vsync_end = drm_mode.vsync_end;

    return mode;
}

Result DRMDisplay::setMode(const DisplayMode& mode) {
    // Find matching DRM mode
    for (int i = 0; i < m_connector->count_modes; i++) {
        if (modesMatch(m_connector->modes[i], mode)) {
            m_drm_mode = m_connector->modes[i];
            m_current_mode = convertDrmMode(m_drm_mode);
            LOG_INFO("Display", "Mode changed to: %ux%u@%.2fHz",
                    m_current_mode.width, m_current_mode.height,
                    m_current_mode.refresh_rate);
            return setModeInternal();
        }
    }

    LOG_ERROR("Display", "Requested mode not available");
    return Result::ERROR_NOT_FOUND;
}

std::vector<DisplayMode> DRMDisplay::getAvailableModes() const {
    std::vector<DisplayMode> modes;

    if (!m_connector) {
        return modes;
    }

    for (int i = 0; i < m_connector->count_modes; i++) {
        modes.push_back(convertDrmMode(m_connector->modes[i]));
    }

    return modes;
}

DRMDisplay::DisplayInfo DRMDisplay::getDisplayInfo() const {
    DisplayInfo info;

    if (!m_connector) {
        return info;
    }

    // Connector name
    const char* type_name = drmModeGetConnectorTypeName(m_connector->connector_type);
    char full_name[64];
    snprintf(full_name, sizeof(full_name), "%s-%d",
            type_name, m_connector->connector_type_id);
    info.connector_name = full_name;

    info.connector_id = m_connector_id;
    info.crtc_id = m_crtc_id;
    info.width = m_current_mode.width;
    info.height = m_current_mode.height;
    info.refresh_rate = m_current_mode.refresh_rate;

    // Check for HDR support (simplified - would need to check properties)
    info.hdr_supported = false;

    // Get available modes
    info.available_modes = getAvailableModes();

    return info;
}

Result DRMDisplay::pageFlip(uint32_t fb_id, void* user_data) {
    if (!m_initialized) {
        LOG_ERROR("Display", "Display not initialized");
        return Result::ERROR_NOT_INITIALIZED;
    }

    if (m_page_flip_pending) {
        LOG_WARN("Display", "Page flip already pending");
        return Result::ERROR_BUSY;
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    // Perform atomic page flip
    int ret = drmModePageFlip(m_drm_fd, m_crtc_id, fb_id,
                             DRM_MODE_PAGE_FLIP_EVENT, user_data);

    if (ret != 0) {
        LOG_ERROR("Display", "Page flip failed: %s", strerror(errno));
        return Result::ERROR_GENERIC;
    }

    m_page_flip_pending = true;

    // Wait for flip to complete
    drmEventContext ev_ctx = {};
    ev_ctx.version = DRM_EVENT_CONTEXT_VERSION;
    ev_ctx.page_flip_handler = pageFlipHandler;

    struct pollfd fds = {};
    fds.fd = m_drm_fd;
    fds.events = POLLIN;

    ret = poll(&fds, 1, 1000);  // 1 second timeout
    if (ret < 0) {
        LOG_ERROR("Display", "Poll failed: %s", strerror(errno));
        m_page_flip_pending = false;
        return Result::ERROR_GENERIC;
    }

    if (ret == 0) {
        LOG_ERROR("Display", "Page flip timeout");
        m_page_flip_pending = false;
        m_stats.missed_vblanks++;
        return Result::ERROR_TIMEOUT;
    }

    drmHandleEvent(m_drm_fd, &ev_ctx);
    m_page_flip_pending = false;

    // Update statistics
    auto end_time = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration<double, std::milli>(end_time - start_time);

    m_stats.last_frame_time_ms = elapsed.count();
    m_stats.frames_presented++;

    if (m_stats.frames_presented == 1) {
        m_stats.avg_frame_time_ms = m_stats.last_frame_time_ms;
    } else {
        m_stats.avg_frame_time_ms =
            (m_stats.avg_frame_time_ms * (m_stats.frames_presented - 1) +
             m_stats.last_frame_time_ms) / m_stats.frames_presented;
    }

    return Result::SUCCESS;
}

void DRMDisplay::pageFlipHandler(int fd, unsigned int sequence,
                                 unsigned int tv_sec, unsigned int tv_usec,
                                 void* user_data) {
    // Page flip completed
    LOG_DEBUG("Display", "Page flip complete (sequence: %u)", sequence);
}

Result DRMDisplay::waitForVBlank() {
    if (!m_initialized) {
        return Result::ERROR_NOT_INITIALIZED;
    }

    drmVBlank vbl = {};
    vbl.request.type = DRM_VBLANK_RELATIVE;
    vbl.request.sequence = 1;

    int ret = drmWaitVBlank(m_drm_fd, &vbl);
    if (ret != 0) {
        LOG_ERROR("Display", "VBlank wait failed: %s", strerror(errno));
        m_stats.missed_vblanks++;
        return Result::ERROR_GENERIC;
    }

    m_stats.vblank_waits++;
    return Result::SUCCESS;
}

void DRMDisplay::shutdown() {
    if (!m_initialized) {
        return;
    }

    LOG_INFO("Display", "Shutting down DRM display");

    // Restore saved CRTC
    if (m_saved_crtc && m_drm_fd >= 0) {
        drmModeSetCrtc(m_drm_fd, m_saved_crtc->crtc_id, m_saved_crtc->buffer_id,
                      m_saved_crtc->x, m_saved_crtc->y,
                      &m_connector_id, 1, &m_saved_crtc->mode);
        drmModeFreeCrtc(m_saved_crtc);
        m_saved_crtc = nullptr;
    }

    // Free resources
    if (m_crtc) {
        drmModeFreeCrtc(m_crtc);
        m_crtc = nullptr;
    }

    if (m_encoder) {
        drmModeFreeEncoder(m_encoder);
        m_encoder = nullptr;
    }

    if (m_connector) {
        drmModeFreeConnector(m_connector);
        m_connector = nullptr;
    }

    if (m_resources) {
        drmModeFreeResources(m_resources);
        m_resources = nullptr;
    }

    if (m_drm_fd >= 0) {
        close(m_drm_fd);
        m_drm_fd = -1;
    }

    m_initialized = false;
    LOG_INFO("Display", "DRM display shut down");
}

} // namespace display
} // namespace ares
