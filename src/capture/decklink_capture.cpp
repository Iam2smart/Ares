#include "decklink_capture.h"
#include "core/logger.h"

namespace ares {
namespace capture {

DeckLinkCapture::DeckLinkCapture() {
    LOG_INFO("Capture", "DeckLinkCapture created (stub)");
}

DeckLinkCapture::~DeckLinkCapture() {
    stop();
}

Result DeckLinkCapture::initialize(int device_index) {
    LOG_INFO("Capture", "Initializing DeckLink device %d (stub)", device_index);
    m_initialized = true;
    return Result::SUCCESS;
}

Result DeckLinkCapture::start() {
    if (!m_initialized) {
        return Result::ERROR_NOT_INITIALIZED;
    }
    LOG_INFO("Capture", "Starting capture (stub)");
    return Result::SUCCESS;
}

Result DeckLinkCapture::stop() {
    LOG_INFO("Capture", "Stopping capture (stub)");
    return Result::SUCCESS;
}

Result DeckLinkCapture::getFrame(VideoFrame& frame) {
    // Stub implementation
    return Result::ERROR_TIMEOUT;
}

} // namespace capture
} // namespace ares
