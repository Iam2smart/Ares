#include "black_bar_detector.h"
#include "core/logger.h"
#include <algorithm>
#include <cstring>
#include <sstream>
#include <cstdio>
#include <map>

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

    // FFmpeg bootstrap phase (skip detection during delay and bootstrap)
    if (config.use_ffmpeg_bootstrap && !m_bootstrap_complete) {
        // Calculate delay in frames (assume 60fps for simplicity)
        uint64_t delay_frames = static_cast<uint64_t>(config.bootstrap_delay * 60.0f);

        if (m_stats.frames_analyzed <= delay_frames) {
            // Still in delay period - don't detect anything yet
            return;
        }

        // Bootstrap period active - use FFmpeg-detected crop if available
        if (m_bootstrap_crop.confidence > 0.0f) {
            m_current_crop = m_bootstrap_crop;
            m_stable_crop = m_bootstrap_crop;
        }
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
    m_bootstrap_crop = CropRegion();
    m_bootstrap_complete = false;
    m_bootstrap_delay_frames = 0;
    LOG_INFO("Processing", "BlackBarDetector reset");
}

Result BlackBarDetector::bootstrapWithFFmpeg(const std::string& video_source,
                                             uint32_t frame_width, uint32_t frame_height,
                                             const BlackBarConfig& config) {
    if (m_bootstrap_complete) {
        LOG_INFO("Processing", "Bootstrap already complete");
        return Result::SUCCESS;
    }

    LOG_INFO("Processing", "Starting FFmpeg cropdetect bootstrap...");
    LOG_INFO("Processing", "  Delay: %.1fs, Duration: %.1fs, Threshold: %d",
            config.bootstrap_delay, config.bootstrap_duration, config.threshold);

    // Build FFmpeg command
    // We'll use ffmpeg to analyze the video file/stream and detect crop values
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
            "ffmpeg -ss %.1f -i \"%s\" -t %.1f "
            "-vf cropdetect=limit=%d/255:round=2:reset=0 "
            "-f null - 2>&1 | grep -o 'crop=[0-9:]*'",
            config.bootstrap_delay,
            video_source.c_str(),
            config.bootstrap_duration,
            config.threshold);

    LOG_DEBUG("Processing", "Running: %s", cmd);

    // Execute command and capture output
    FILE* pipe = popen(cmd, "r");
    if (!pipe) {
        LOG_ERROR("Processing", "Failed to execute FFmpeg cropdetect");
        return Result::ERROR_GENERIC;
    }

    // Read all crop lines
    std::string all_output;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        all_output += buffer;
    }

    int ret = pclose(pipe);
    if (ret != 0 && ret != 256) {  // 256 = normal exit from grep with matches
        LOG_WARN("Processing", "FFmpeg cropdetect returned non-zero: %d", ret);
    }

    if (all_output.empty()) {
        LOG_WARN("Processing", "No crop data detected by FFmpeg");
        // Mark as complete anyway to avoid infinite waiting
        m_bootstrap_complete = true;
        return Result::ERROR_GENERIC;
    }

    // Parse the output to find the most common crop
    CropRegion best_crop;
    if (parseFFmpegCropOutput(all_output, frame_width, frame_height, best_crop)) {
        LOG_INFO("Processing", "FFmpeg detected crop: top=%d bottom=%d left=%d right=%d",
                best_crop.top, best_crop.bottom, best_crop.left, best_crop.right);

        // Seed the detection history with bootstrap results
        seedHistoryWithBootstrap(best_crop);

        m_bootstrap_crop = best_crop;
        m_stable_crop = best_crop;
        m_current_crop = best_crop;
        m_bootstrap_complete = true;

        LOG_INFO("Processing", "FFmpeg bootstrap complete");
        return Result::SUCCESS;
    } else {
        LOG_WARN("Processing", "Failed to parse FFmpeg crop output");
        m_bootstrap_complete = true;
        return Result::ERROR_GENERIC;
    }
}

bool BlackBarDetector::parseFFmpegCropOutput(const std::string& output,
                                             uint32_t frame_width, uint32_t frame_height,
                                             CropRegion& result) {
    // FFmpeg cropdetect outputs lines like: "crop=1920:800:0:140"
    // Format: crop=width:height:x:y
    // We need to find the most common crop value

    std::map<std::string, int> crop_counts;
    std::string last_crop;

    std::istringstream stream(output);
    std::string line;

    while (std::getline(stream, line)) {
        size_t crop_pos = line.find("crop=");
        if (crop_pos != std::string::npos) {
            std::string crop_str = line.substr(crop_pos + 5);
            // Remove trailing whitespace/newline
            size_t end = crop_str.find_first_not_of("0123456789:");
            if (end != std::string::npos) {
                crop_str = crop_str.substr(0, end);
            }

            if (!crop_str.empty()) {
                crop_counts[crop_str]++;
                last_crop = crop_str;
            }
        }
    }

    if (crop_counts.empty()) {
        return false;
    }

    // Find most common crop value
    std::string best_crop_str;
    int max_count = 0;
    for (const auto& pair : crop_counts) {
        if (pair.second > max_count) {
            max_count = pair.second;
            best_crop_str = pair.first;
        }
    }

    // Parse crop string: "width:height:x:y"
    int crop_w = 0, crop_h = 0, crop_x = 0, crop_y = 0;
    if (sscanf(best_crop_str.c_str(), "%d:%d:%d:%d", &crop_w, &crop_h, &crop_x, &crop_y) != 4) {
        LOG_ERROR("Processing", "Failed to parse crop string: %s", best_crop_str.c_str());
        return false;
    }

    // Convert FFmpeg format (width, height, x, y) to our format (top, bottom, left, right)
    result.left = crop_x;
    result.top = crop_y;
    result.right = (frame_width - crop_w - crop_x);
    result.bottom = (frame_height - crop_h - crop_y);

    result.confidence = std::min(1.0f, (float)max_count / 10.0f);  // Higher count = higher confidence
    result.is_symmetric = isSymmetric(result.top, result.bottom, result.left, result.right,
                                     frame_width, frame_height);

    LOG_DEBUG("Processing", "Parsed crop: %dx%d at (%d,%d) -> TBLR={%d,%d,%d,%d} (count=%d)",
             crop_w, crop_h, crop_x, crop_y,
             result.top, result.bottom, result.left, result.right, max_count);

    return true;
}

void BlackBarDetector::seedHistoryWithBootstrap(const CropRegion& bootstrap_crop) {
    // Fill history with bootstrap results for immediate stability
    m_history.clear();
    for (size_t i = 0; i < m_max_history; i++) {
        m_history.push_back(bootstrap_crop);
    }

    LOG_DEBUG("Processing", "Seeded detection history with %zu bootstrap samples",
             m_history.size());
}

BlackBarDetector::Stats BlackBarDetector::getStats() const {
    Stats stats = m_stats;
    stats.bootstrap_complete = m_bootstrap_complete;
    return stats;
}

} // namespace processing
} // namespace ares
