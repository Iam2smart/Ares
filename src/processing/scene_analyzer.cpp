#include "scene_analyzer.h"
#include "core/logger.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace ares {
namespace processing {

SceneAnalyzer::SceneAnalyzer() {
    LOG_INFO("Processing", "SceneAnalyzer created");
}

SceneAnalyzer::~SceneAnalyzer() {
    LOG_INFO("Processing", "SceneAnalyzer destroyed");
}

void SceneAnalyzer::initialize(const ToneMappingConfig::DynamicToneMappingConfig& config) {
    m_config = config;
    m_initialized = true;

    // Initialize current params with safe defaults
    m_current_params.source_nits = 1000.0f;
    m_current_params.knee_point = 0.75f;
    m_current_params.avg_brightness = 100.0f;
    m_current_params.peak_brightness = 1000.0f;
    m_current_params.histogram_brightness = 500.0f;
    m_current_params.scene_changed = false;
    m_current_params.frame_count = 0;

    // Reset stats
    memset(&m_stats, 0, sizeof(m_stats));

    LOG_INFO("Processing", "SceneAnalyzer initialized (analysis_frames=%d, adaptation_speed=%.2f)",
             m_config.analysis_frames, m_config.adaptation_speed);
}

void SceneAnalyzer::reset() {
    m_window.clear();
    m_frame_count = 0;
    memset(&m_previous_stats, 0, sizeof(m_previous_stats));
    memset(&m_stats, 0, sizeof(m_stats));

    LOG_DEBUG("Processing", "SceneAnalyzer reset");
}

float SceneAnalyzer::lumaNitsToFloat(float luma, const HDRMetadata& hdr_metadata) {
    // Convert normalized luma [0-1] to nits
    // Use HDR metadata if available, otherwise use PQ curve

    if (hdr_metadata.max_cll > 0.0f) {
        // Scale by MaxCLL from metadata
        return luma * hdr_metadata.max_cll;
    } else if (hdr_metadata.max_luminance > 0) {
        // Scale by mastering display luminance
        return luma * hdr_metadata.max_luminance;
    } else {
        // Fallback: assume PQ curve with 10000 nits max
        // PQ inverse EOTF approximation
        const float m1 = 0.1593017578125f;
        const float m2 = 78.84375f;
        const float c1 = 0.8359375f;
        const float c2 = 18.8515625f;
        const float c3 = 18.6875f;

        float pq = powf(luma, 1.0f / m2);
        float num = std::max(pq - c1, 0.0f);
        float den = c2 - c3 * pq;
        float linear = powf(num / den, 1.0f / m1);

        // Scale to nits (PQ uses 10000 nits reference)
        return linear * 10000.0f;
    }
}

SceneAnalyzer::BrightnessStats SceneAnalyzer::calculateBrightness(const VideoFrame& frame) {
    BrightnessStats stats = {};
    stats.frame_number = m_frame_count;

    // Simplified brightness calculation
    // Assumes frame data is in linear light or gamma-corrected space

    if (!frame.data || frame.size == 0) {
        LOG_WARN("Processing", "Cannot analyze empty frame");
        return stats;
    }

    // Sample pixels for brightness analysis
    // For performance, we sample every Nth pixel
    const int sample_stride = 8; // Sample every 8th pixel
    const int bytes_per_pixel = (frame.format == PixelFormat::RGB_8BIT) ? 3 :
                                (frame.format == PixelFormat::RGBA_8BIT) ? 4 :
                                (frame.format == PixelFormat::YUV420P_10BIT) ? 2 : 3;

    uint64_t sum_luma = 0;
    uint64_t sample_count = 0;
    float max_luma = 0.0f;

    // Calculate average and peak brightness
    for (uint32_t y = 0; y < frame.height; y += sample_stride) {
        for (uint32_t x = 0; x < frame.width; x += sample_stride) {
            // Calculate Y (luma) from RGB or extract directly from YUV
            float luma = 0.0f;

            if (frame.format == PixelFormat::RGB_8BIT || frame.format == PixelFormat::RGBA_8BIT) {
                size_t idx = (y * frame.width + x) * bytes_per_pixel;
                if (idx + 2 < frame.size) {
                    uint8_t r = frame.data[idx + 0];
                    uint8_t g = frame.data[idx + 1];
                    uint8_t b = frame.data[idx + 2];

                    // BT.709 luma coefficients
                    luma = (0.2126f * r + 0.7152f * g + 0.0722f * b) / 255.0f;
                }
            } else if (frame.format == PixelFormat::YUV420P_10BIT) {
                // Y plane is first
                size_t idx = y * frame.width + x;
                if (idx * 2 < frame.size) {
                    uint16_t y_value = *(uint16_t*)(frame.data + idx * 2);
                    luma = y_value / 1023.0f; // 10-bit
                }
            } else {
                // Fallback: treat as 8-bit grayscale
                size_t idx = y * frame.width + x;
                if (idx < frame.size) {
                    luma = frame.data[idx] / 255.0f;
                }
            }

            sum_luma += (uint64_t)(luma * 1000.0f);
            max_luma = std::max(max_luma, luma);
            sample_count++;
        }
    }

    if (sample_count > 0) {
        stats.avg_luma = (sum_luma / (float)sample_count) / 1000.0f;
        stats.peak_luma = max_luma;
    }

    return stats;
}

float SceneAnalyzer::calculateHistogramBrightness(const VideoFrame& frame, float percentile) {
    // Build brightness histogram
    const int histogram_bins = 256;
    std::vector<uint32_t> histogram(histogram_bins, 0);

    const int sample_stride = 8;
    const int bytes_per_pixel = (frame.format == PixelFormat::RGB_8BIT) ? 3 : 4;

    // Build histogram
    for (uint32_t y = 0; y < frame.height; y += sample_stride) {
        for (uint32_t x = 0; x < frame.width; x += sample_stride) {
            float luma = 0.0f;

            if (frame.format == PixelFormat::RGB_8BIT || frame.format == PixelFormat::RGBA_8BIT) {
                size_t idx = (y * frame.width + x) * bytes_per_pixel;
                if (idx + 2 < frame.size) {
                    uint8_t r = frame.data[idx + 0];
                    uint8_t g = frame.data[idx + 1];
                    uint8_t b = frame.data[idx + 2];
                    luma = (0.2126f * r + 0.7152f * g + 0.0722f * b) / 255.0f;
                }
            } else {
                continue;
            }

            int bin = std::min((int)(luma * (histogram_bins - 1)), histogram_bins - 1);
            histogram[bin]++;
        }
    }

    // Find percentile
    uint64_t total_samples = 0;
    for (uint32_t count : histogram) {
        total_samples += count;
    }

    uint64_t target_count = (uint64_t)(total_samples * (percentile / 100.0f));
    uint64_t cumulative = 0;
    int percentile_bin = histogram_bins - 1;

    for (int i = 0; i < histogram_bins; i++) {
        cumulative += histogram[i];
        if (cumulative >= target_count) {
            percentile_bin = i;
            break;
        }
    }

    return percentile_bin / (float)(histogram_bins - 1);
}

bool SceneAnalyzer::detectSceneChange(const BrightnessStats& current) {
    if (m_previous_stats.frame_number == 0) {
        // First frame
        return true;
    }

    // Calculate delta in average brightness
    float avg_delta = std::abs(current.avg_luma - m_previous_stats.avg_luma);
    float peak_delta = std::abs(current.peak_luma - m_previous_stats.peak_luma);

    // Combined delta
    float delta = (avg_delta * 0.7f + peak_delta * 0.3f);

    m_stats.last_scene_delta = delta;

    // Scene change if delta exceeds threshold
    if (delta > m_config.scene_threshold) {
        LOG_DEBUG("Processing", "Scene change detected (delta=%.3f, threshold=%.3f)",
                  delta, m_config.scene_threshold);
        return true;
    }

    return false;
}

float SceneAnalyzer::smoothParameter(float target, float current, float speed) {
    // Exponential smoothing
    return current + (target - current) * speed;
}

bool SceneAnalyzer::analyzeFrame(const VideoFrame& frame, const HDRMetadata& hdr_metadata) {
    if (!m_initialized) {
        LOG_WARN("Processing", "SceneAnalyzer not initialized");
        return false;
    }

    m_frame_count++;
    m_current_params.frame_count = m_frame_count;

    // Calculate brightness statistics
    BrightnessStats stats = calculateBrightness(frame);

    // Detect scene change
    bool scene_changed = detectSceneChange(stats);
    m_current_params.scene_changed = scene_changed;

    if (scene_changed) {
        m_stats.scene_changes++;

        // On scene change, reset window for fast adaptation
        if (m_config.smooth_transitions) {
            // Keep some history for smoothing
            while (m_window.size() > (size_t)(m_config.analysis_frames / 4)) {
                m_window.pop_front();
            }
        } else {
            // Immediate adaptation
            m_window.clear();
        }
    }

    // Add to analysis window
    m_window.push_back(stats);
    while ((int)m_window.size() > m_config.analysis_frames) {
        m_window.pop_front();
    }

    // Calculate smoothed statistics over window
    if (!m_window.empty()) {
        float sum_avg = 0.0f;
        float max_peak = 0.0f;

        for (const auto& s : m_window) {
            sum_avg += s.avg_luma;
            max_peak = std::max(max_peak, s.peak_luma);
        }

        float window_avg_luma = sum_avg / m_window.size();
        float window_peak_luma = max_peak;

        // Update stats
        m_stats.current_avg_luma = stats.avg_luma;
        m_stats.current_peak_luma = stats.peak_luma;
        m_stats.smoothed_avg_luma = window_avg_luma;
        m_stats.smoothed_peak_luma = window_peak_luma;
        m_stats.frames_analyzed = m_frame_count;

        // Calculate histogram-based brightness
        float histogram_luma = calculateHistogramBrightness(frame, m_config.peak_percentile);

        // Convert to nits
        float avg_nits = lumaNitsToFloat(window_avg_luma, hdr_metadata);
        float peak_nits = lumaNitsToFloat(window_peak_luma, hdr_metadata);
        float histogram_nits = lumaNitsToFloat(histogram_luma, hdr_metadata);

        // Calculate target source_nits based on scene brightness
        float target_source_nits = 1000.0f;

        if (m_config.use_peak_brightness && m_config.use_average_brightness) {
            // Combine peak and average (weighted)
            target_source_nits = peak_nits * 0.7f + avg_nits * 0.3f;
        } else if (m_config.use_peak_brightness) {
            target_source_nits = peak_nits;
        } else if (m_config.use_average_brightness) {
            target_source_nits = avg_nits * 2.0f; // Scale up average
        }

        // Clamp to bounds
        target_source_nits = std::max(m_config.bounds.min_source_nits,
                                     std::min(m_config.bounds.max_source_nits, target_source_nits));

        // Calculate target knee_point based on scene contrast
        float contrast_ratio = window_peak_luma / std::max(window_avg_luma, 0.01f);
        float target_knee_point = 0.75f;

        if (contrast_ratio > 10.0f) {
            // High contrast: lower knee point (more aggressive tone mapping)
            target_knee_point = 0.6f;
        } else if (contrast_ratio < 3.0f) {
            // Low contrast: higher knee point (preserve more)
            target_knee_point = 0.85f;
        }

        // Clamp knee point
        target_knee_point = std::max(m_config.bounds.min_knee_point,
                                     std::min(m_config.bounds.max_knee_point, target_knee_point));

        // Smooth parameters over time (prevent flickering)
        float adaptation_speed = m_config.adaptation_speed;
        if (scene_changed) {
            // Faster adaptation on scene changes
            adaptation_speed = std::min(1.0f, adaptation_speed * 2.0f);
        }

        // Apply minimum change threshold
        float source_delta = std::abs(target_source_nits - m_current_params.source_nits);
        if (source_delta > m_config.min_change_threshold) {
            m_current_params.source_nits = smoothParameter(target_source_nits,
                                                          m_current_params.source_nits,
                                                          adaptation_speed);
        }

        m_current_params.knee_point = smoothParameter(target_knee_point,
                                                       m_current_params.knee_point,
                                                       adaptation_speed);

        // Update brightness values
        m_current_params.avg_brightness = avg_nits;
        m_current_params.peak_brightness = peak_nits;
        m_current_params.histogram_brightness = histogram_nits;

        LOG_DEBUG("Processing", "Scene analysis: avg=%.1f nits, peak=%.1f nits, source=%.1f nits, knee=%.3f",
                  avg_nits, peak_nits, m_current_params.source_nits, m_current_params.knee_point);
    }

    // Save for next frame
    m_previous_stats = stats;

    return scene_changed;
}

SceneAnalyzer::DynamicParams SceneAnalyzer::getDynamicParams() const {
    return m_current_params;
}

} // namespace processing
} // namespace ares
