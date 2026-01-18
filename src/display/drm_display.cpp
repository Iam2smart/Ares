#include "drm_display.h"
#include "core/logger.h"

namespace ares {
namespace display {

DRMDisplay::DRMDisplay() {
    LOG_INFO("Display", "DRMDisplay created (stub)");
}

DRMDisplay::~DRMDisplay() = default;

Result DRMDisplay::initialize(const std::string& connector) {
    LOG_INFO("Display", "Initializing DRM display on %s (stub)", connector.c_str());
    m_initialized = true;
    return Result::SUCCESS;
}

Result DRMDisplay::setMode(const DisplayMode& mode) {
    LOG_INFO("Display", "Setting mode %dx%d@%.2f (stub)", mode.width, mode.height, mode.refresh_rate);
    return Result::SUCCESS;
}

Result DRMDisplay::presentFrame(const VideoFrame& frame) {
    // Stub implementation
    return Result::SUCCESS;
}

} // namespace display
} // namespace ares
