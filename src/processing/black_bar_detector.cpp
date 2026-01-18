#include "black_bar_detector.h"
#include "core/logger.h"
#include <algorithm>
#include <cstring>

namespace ares {
namespace processing {

BlackBarDetector::BlackBarDetector() {
    LOG_INFO("Processing", "BlackBarDetector created");
}

BlackBarDetector::~BlackBarDetector() = default;

void BlackBarDetector::analyzeFrame(const VideoFrame& frame, const BlackBarConfig& config) {
    if (!config.enabled) {
        return;
    }

    m_stats.frames_analyzed++;

    // Manual crop overrides detection
    if (config.manual_crop.enabled) {
        m_current_crop.top = config.manual_crop.top;
        m_current_crop.bottom = config.manual_crop.bottom;
        m_current_crop.left = config.manual_crop.left;
        m_current_crop.right = config.manual_crop.right;
        m_current_crop.confidence = 1.0f;
        m_current_crop.is_symmetric = true;
        return;
    }

    // Convert frame to grayscale for analysis (simplified - use Y channel)
    const uint8_t* luma = frame.data;

    // Detect bars
    int top = 0, bottom = 0, left = 0, right = 0;
    analyzeHorizontal(luma, frame.width, frame.height, config.threshold, top, bottom);
    analyzeVertical(luma, frame.width, frame.height, config.threshold, left, right);

    // Check minimum content size
    float content_height = (frame.height - top - bottom) / (float)frame.height;
    float content_width = (frame.width - left - right) / (float)frame.width;

    if (content_height < config.min_content_height ||
        content_width < config.min_content_width) {
        // Too much would be cropped, probably not black bars
        top = bottom = left = right = 0;
    }

    // Check symmetry if required
    bool symmetric = isSymmetric(top, bottom, left, right, frame.width, frame.height);
    if (config.symmetric_only && !symmetric) {
        top = bottom = left = right = 0;
    }

    // Create new crop region
    CropRegion new_crop;
    new_crop.top = top;
    new_crop.bottom = bottom;
    new_crop.left = left;
    new_crop.right = right;
    new_crop.is_symmetric = symmetric;

    // Add to history
    m_history.push_back(new_crop);
    if (m_history.size() > m_max_history) {
        m_history.pop_front();
    }

    // Calculate confidence
    new_crop.confidence = calculateConfidence();

    // Update current crop with smoothing
    if (config.crop_smoothing > 0.0f) {
        m_current_crop = smoothCrop(new_crop, config.crop_smoothing);
    } else {
        m_current_crop = new_crop;
    }

    // Update stable crop if confidence is high
    if (new_crop.confidence >= config.confidence_threshold) {
        m_stable_crop = new_crop;
        m_stats.bars_detected++;
    }

    m_stats.current_confidence = new_crop.confidence;
    m_stats.current_crop = m_current_crop;
}

void BlackBarDetector::analyzeHorizontal(const uint8_t* data, uint32_t width, uint32_t height,
                                        int threshold, int& top, int& bottom) {
    top = 0;
    bottom = 0;

    // Scan from top
    for (uint32_t y = 0; y < height / 2; y++) {
        bool is_black = true;

        // Sample multiple points across the line
        for (uint32_t x = 0; x < width; x += width / 16) {
            int pixel_value = data[y * width + x];
            if (pixel_value > threshold) {
                is_black = false;
                break;
            }
        }

        if (!is_black) {
            top = y;
            break;
        }
    }

    // Scan from bottom
    for (uint32_t y = height - 1; y > height / 2; y--) {
        bool is_black = true;

        // Sample multiple points across the line
        for (uint32_t x = 0; x < width; x += width / 16) {
            int pixel_value = data[y * width + x];
            if (pixel_value > threshold) {
                is_black = false;
                break;
            }
        }

        if (!is_black) {
            bottom = height - 1 - y;
            break;
        }
    }
}

void BlackBarDetector::analyzeVertical(const uint8_t* data, uint32_t width, uint32_t height,
                                      int threshold, int& left, int& right) {
    left = 0;
    right = 0;

    // Scan from left
    for (uint32_t x = 0; x < width / 2; x++) {
        bool is_black = true;

        // Sample multiple points down the column
        for (uint32_t y = 0; y < height; y += height / 16) {
            int pixel_value = data[y * width + x];
            if (pixel_value > threshold) {
                is_black = false;
                break;
            }
        }

        if (!is_black) {
            left = x;
            break;
        }
    }

    // Scan from right
    for (uint32_t x = width - 1; x > width / 2; x--) {
        bool is_black = true;

        // Sample multiple points down the column
        for (uint32_t y = 0; y < height; y += height / 16) {
            int pixel_value = data[y * width + x];
            if (pixel_value > threshold) {
                is_black = false;
                break;
            }
        }

        if (!is_black) {
            right = width - 1 - x;
            break;
        }
    }
}

bool BlackBarDetector::isSymmetric(int top, int bottom, int left, int right,
                                   uint32_t width, uint32_t height) const {
    // Check vertical symmetry (top/bottom bars)
    bool v_symmetric = true;
    if (top > 0 || bottom > 0) {
        int diff = std::abs(top - bottom);
        float tolerance = height * 0.05f; // 5% tolerance
        v_symmetric = (diff < tolerance);
    }

    // Check horizontal symmetry (left/right bars)
    bool h_symmetric = true;
    if (left > 0 || right > 0) {
        int diff = std::abs(left - right);
        float tolerance = width * 0.05f; // 5% tolerance
        h_symmetric = (diff < tolerance);
    }

    return v_symmetric && h_symmetric;
}

float BlackBarDetector::calculateConfidence() const {
    if (m_history.empty()) {
        return 0.0f;
    }

    // Count how many recent detections match the most recent one
    const CropRegion& latest = m_history.back();
    int matching = 0;
    int tolerance = 2; // 2 pixel tolerance

    for (const auto& crop : m_history) {
        if (std::abs(crop.top - latest.top) <= tolerance &&
            std::abs(crop.bottom - latest.bottom) <= tolerance &&
            std::abs(crop.left - latest.left) <= tolerance &&
            std::abs(crop.right - latest.right) <= tolerance) {
            matching++;
        }
    }

    return (float)matching / m_history.size();
}

CropRegion BlackBarDetector::smoothCrop(const CropRegion& target, float smoothing) {
    // Exponential moving average for smooth transitions
    float alpha = 1.0f - smoothing;

    CropRegion result;
    result.top = (int)(m_current_crop.top * smoothing + target.top * alpha);
    result.bottom = (int)(m_current_crop.bottom * smoothing + target.bottom * alpha);
    result.left = (int)(m_current_crop.left * smoothing + target.left * alpha);
    result.right = (int)(m_current_crop.right * smoothing + target.right * alpha);
    result.confidence = target.confidence;
    result.is_symmetric = target.is_symmetric;

    return result;
}

CropRegion BlackBarDetector::getCropRegion() const {
    return m_stable_crop;
}

bool BlackBarDetector::isStable() const {
    return m_stable_crop.confidence > 0.8f;
}

void BlackBarDetector::reset() {
    m_history.clear();
    m_current_crop = CropRegion();
    m_stable_crop = CropRegion();
    LOG_INFO("Processing", "BlackBarDetector reset");
}

BlackBarDetector::Stats BlackBarDetector::getStats() const {
    return m_stats;
}

} // namespace processing
} // namespace ares
