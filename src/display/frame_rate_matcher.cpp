#include "frame_rate_matcher.h"
#include "core/logger.h"
#include <cmath>
#include <algorithm>

namespace ares {
namespace display {

Result FrameRateMatcher::initialize(DRMDisplay* display, capture::DeckLinkCapture* capture) {
    if (!display || !capture) {
        return Result::ERROR_INVALID_PARAMETER;
    }

    m_display = display;
    m_capture = capture;

    LOG_INFO("Display", "Frame rate matcher initialized");
    return Result::SUCCESS;
}

Result FrameRateMatcher::update() {
    if (!m_enabled || !m_display || !m_capture) {
        return Result::SUCCESS;
    }

    // Get detected frame rate from capture
    double detected_fps = m_capture->getDetectedFrameRate();
    bool is_stable = m_capture->isFrameRateStable();

    m_stats.current_source_fps = detected_fps;

    // Only switch if:
    // 1. Frame rate is stable
    // 2. Frame rate has changed significantly
    // 3. Current mode is not optimal
    if (!is_stable) {
        return Result::SUCCESS;
    }

    // Check if frame rate changed
    if (std::abs(detected_fps - m_last_detected_fps) < 0.5 && m_last_was_stable) {
        // No significant change
        return Result::SUCCESS;
    }

    // Check if current mode is already optimal
    if (isCurrentModeOptimal(detected_fps)) {
        m_stats.mode_matched = true;
        m_last_detected_fps = detected_fps;
        m_last_was_stable = is_stable;
        return Result::SUCCESS;
    }

    // Find best matching mode
    DisplayMode best_mode = findBestMatch(detected_fps);
    if (best_mode.refresh_rate == 0.0f) {
        LOG_WARN("Display", "No suitable display mode found for %.3f fps", detected_fps);
        m_stats.mode_matched = false;
        return Result::ERROR_NOT_FOUND;
    }

    // Switch to new mode
    LOG_INFO("Display", "Switching display mode: %.3f fps → %.2f Hz (source: %.3f fps)",
             m_display->getCurrentMode().refresh_rate, best_mode.refresh_rate, detected_fps);

    Result result = m_display->setMode(best_mode);
    if (result == Result::SUCCESS) {
        m_stats.mode_switches++;
        m_stats.current_display_refresh = best_mode.refresh_rate;
        m_stats.mode_matched = true;
        m_stats.last_switch_reason = "Source frame rate changed to " +
                                      std::to_string(detected_fps) + " fps";

        LOG_INFO("Display", "Display mode switched successfully to %.2f Hz", best_mode.refresh_rate);
    } else {
        LOG_ERROR("Display", "Failed to switch display mode");
        m_stats.mode_matched = false;
    }

    m_last_detected_fps = detected_fps;
    m_last_was_stable = is_stable;

    return result;
}

DisplayMode FrameRateMatcher::findBestMatch(double source_fps) const {
    if (!m_display) {
        return DisplayMode{};
    }

    auto available_modes = m_display->getAvailableModes();
    if (available_modes.empty()) {
        return DisplayMode{};
    }

    // Find closest matching refresh rate
    float target_refresh = findClosestRefreshRate(source_fps, available_modes);
    if (target_refresh == 0.0f) {
        return DisplayMode{};
    }

    // Find mode with matching refresh rate (prefer highest resolution)
    DisplayMode best_mode{};
    uint32_t best_pixels = 0;

    for (const auto& mode : available_modes) {
        // Check if refresh rate matches (within 0.5 Hz)
        if (std::abs(mode.refresh_rate - target_refresh) < 0.5f) {
            uint32_t pixels = mode.width * mode.height;
            if (pixels > best_pixels) {
                best_mode = mode;
                best_pixels = pixels;
            }
        }
    }

    return best_mode;
}

float FrameRateMatcher::findClosestRefreshRate(double source_fps,
                                                const std::vector<DisplayMode>& modes) const {
    // Common frame rate mappings
    // 23.976 → 24Hz or 48Hz or 96Hz or 120Hz (24*n)
    // 24.000 → 24Hz or 48Hz or 96Hz or 120Hz
    // 25.000 → 25Hz or 50Hz or 100Hz (25*n)
    // 29.970 → 30Hz or 60Hz or 120Hz (30*n)
    // 30.000 → 30Hz or 60Hz or 120Hz
    // 50.000 → 50Hz or 100Hz
    // 59.940 → 60Hz or 120Hz
    // 60.000 → 60Hz or 120Hz

    // Round source fps to nearest common rate
    double base_fps = source_fps;

    // Detect common film rates (23.976, 24)
    if (std::abs(source_fps - 23.976) < 0.1) {
        base_fps = 24.0;
    } else if (std::abs(source_fps - 29.970) < 0.1) {
        base_fps = 30.0;
    } else if (std::abs(source_fps - 59.940) < 0.1) {
        base_fps = 60.0;
    } else if (std::abs(source_fps - 119.880) < 0.1) {
        base_fps = 120.0;
    }

    // Build list of available refresh rates
    std::vector<float> available_rates;
    for (const auto& mode : modes) {
        // Add if not already in list
        if (std::find_if(available_rates.begin(), available_rates.end(),
                        [&](float r) { return std::abs(r - mode.refresh_rate) < 0.1f; })
            == available_rates.end()) {
            available_rates.push_back(mode.refresh_rate);
        }
    }

    // Find best match (prefer exact match, then integer multiples, then closest)
    float best_match = 0.0f;
    float best_score = 1000.0f;

    for (float rate : available_rates) {
        // Exact match (within 0.5 Hz)
        if (std::abs(rate - base_fps) < 0.5f) {
            return rate;
        }

        // Integer multiple match (e.g., 24fps → 48Hz, 120Hz)
        for (int mult = 2; mult <= 5; mult++) {
            if (std::abs(rate - base_fps * mult) < 0.5f) {
                // Prefer lower multiples (less overhead)
                float score = mult;
                if (score < best_score) {
                    best_match = rate;
                    best_score = score;
                }
            }
        }

        // Integer divisor match (e.g., 120fps → 60Hz)
        for (int div = 2; div <= 4; div++) {
            if (std::abs(rate - base_fps / div) < 0.5f) {
                float score = 10.0f + div;  // Penalize divisors more
                if (score < best_score) {
                    best_match = rate;
                    best_score = score;
                }
            }
        }
    }

    // If no good match, find closest
    if (best_match == 0.0f) {
        for (float rate : available_rates) {
            float diff = std::abs(rate - base_fps);
            if (diff < best_score) {
                best_match = rate;
                best_score = diff;
            }
        }
    }

    return best_match;
}

bool FrameRateMatcher::isCurrentModeOptimal(double source_fps) const {
    if (!m_display) {
        return false;
    }

    auto current = m_display->getCurrentMode();
    auto best = findBestMatch(source_fps);

    // Compare refresh rates (within 0.5 Hz)
    return std::abs(current.refresh_rate - best.refresh_rate) < 0.5f;
}

} // namespace display
} // namespace ares
