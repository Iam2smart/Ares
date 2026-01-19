#pragma once

#include <ares/types.h>
#include "drm_display.h"
#include "../capture/decklink_capture.h"

namespace ares {
namespace display {

// Automatic display mode switching to match source frame rate
// Eliminates judder by matching display refresh to content
class FrameRateMatcher {
public:
    FrameRateMatcher() = default;
    ~FrameRateMatcher() = default;

    // Initialize with display and capture
    Result initialize(DRMDisplay* display, capture::DeckLinkCapture* capture);

    // Update and potentially switch modes
    // Call this regularly (e.g., every second) or when frame rate changes
    Result update();

    // Check if mode switching is enabled
    bool isEnabled() const { return m_enabled; }
    void setEnabled(bool enabled) { m_enabled = enabled; }

    // Get matching mode for a given frame rate
    // Returns best matching display mode, or nullptr if none suitable
    DisplayMode findBestMatch(double source_fps) const;

    // Statistics
    struct Stats {
        uint64_t mode_switches = 0;
        double current_source_fps = 0.0;
        float current_display_refresh = 0.0f;
        bool mode_matched = false;
        std::string last_switch_reason;
    };

    Stats getStats() const { return m_stats; }

private:
    // Find closest matching refresh rate
    // Handles common conversions: 23.976→24Hz, 59.94→60Hz
    float findClosestRefreshRate(double source_fps, const std::vector<DisplayMode>& modes) const;

    // Check if current mode matches source
    bool isCurrentModeOptimal(double source_fps) const;

    DRMDisplay* m_display = nullptr;
    capture::DeckLinkCapture* m_capture = nullptr;

    bool m_enabled = true;
    double m_last_detected_fps = 0.0;
    bool m_last_was_stable = false;

    Stats m_stats;
};

} // namespace display
} // namespace ares
