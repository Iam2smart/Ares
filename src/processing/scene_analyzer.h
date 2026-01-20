#pragma once

#include <ares/types.h>
#include <ares/processing_config.h>
#include <vector>
#include <deque>

namespace ares {
namespace processing {

/**
 * Scene Analyzer for Dynamic Tone Mapping
 *
 * Analyzes brightness characteristics of video frames to enable
 * per-scene dynamic tone mapping (like madVR Envy).
 *
 * Features:
 * - Brightness histogram analysis (peak, average, median)
 * - Scene change detection
 * - Temporal smoothing over analysis window
 * - Dynamic parameter generation for tone mapping
 */
class SceneAnalyzer {
public:
    SceneAnalyzer();
    ~SceneAnalyzer();

    // Initialize with configuration
    void initialize(const ToneMappingConfig::DynamicToneMappingConfig& config);

    // Analyze a frame and update statistics
    // Returns true if scene changed
    bool analyzeFrame(const VideoFrame& frame, const HDRMetadata& hdr_metadata);

    // Get dynamic tone mapping parameters based on current scene
    struct DynamicParams {
        float source_nits;              // Dynamically adjusted source peak
        float knee_point;               // Dynamically adjusted knee point
        float avg_brightness;           // Average scene brightness (nits)
        float peak_brightness;          // Peak scene brightness (nits)
        float histogram_brightness;     // Histogram-based brightness
        bool scene_changed;             // Whether scene just changed
        uint64_t frame_count;           // Number of frames analyzed
    };
    DynamicParams getDynamicParams() const;

    // Reset analyzer (for seeking or input changes)
    void reset();

    // Get current statistics
    struct Stats {
        float current_avg_luma;         // Current average luminance [0-1]
        float current_peak_luma;        // Current peak luminance [0-1]
        float smoothed_avg_luma;        // Smoothed average over window
        float smoothed_peak_luma;       // Smoothed peak over window
        uint64_t frames_analyzed;       // Total frames analyzed
        uint64_t scene_changes;         // Number of scene changes detected
        float last_scene_delta;         // Last scene change delta
    };
    Stats getStats() const { return m_stats; }

private:
    // Brightness analysis
    struct BrightnessStats {
        float avg_luma;                 // Average luminance [0-1]
        float peak_luma;                // Peak luminance [0-1]
        float histogram_peak;           // Peak from histogram
        uint64_t frame_number;
    };

    // Calculate brightness statistics for a frame
    BrightnessStats calculateBrightness(const VideoFrame& frame);

    // Calculate histogram-based brightness
    float calculateHistogramBrightness(const VideoFrame& frame, float percentile);

    // Detect scene change
    bool detectSceneChange(const BrightnessStats& current);

    // Smooth parameters over time
    float smoothParameter(float target, float current, float speed);

    // Convert luminance [0-1] to nits using HDR metadata
    float lumaNitsToFloat(float luma, const HDRMetadata& hdr_metadata);

    // Configuration
    ToneMappingConfig::DynamicToneMappingConfig m_config;

    // Analysis window (circular buffer of brightness stats)
    std::deque<BrightnessStats> m_window;

    // Current smoothed values
    DynamicParams m_current_params;

    // Previous frame for scene detection
    BrightnessStats m_previous_stats;

    // Statistics
    Stats m_stats;

    // State
    bool m_initialized = false;
    uint64_t m_frame_count = 0;
};

} // namespace processing
} // namespace ares
