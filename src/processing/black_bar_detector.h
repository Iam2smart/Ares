#pragma once

#include <ares/types.h>
#include <ares/processing_config.h>
#include <vector>
#include <deque>

namespace ares {
namespace processing {

// Detected crop region
struct CropRegion {
    int top = 0;
    int bottom = 0;
    int left = 0;
    int right = 0;

    float confidence = 0.0f;  // 0.0-1.0
    bool is_symmetric = false;
};

class BlackBarDetector {
public:
    BlackBarDetector();
    ~BlackBarDetector();

    // Analyze frame for black bars
    void analyzeFrame(const VideoFrame& frame, const BlackBarConfig& config);

    // Bootstrap detection using FFmpeg cropdetect
    Result bootstrapWithFFmpeg(const std::string& video_source,
                              uint32_t frame_width, uint32_t frame_height,
                              const BlackBarConfig& config);

    // Check if bootstrap is complete
    bool isBootstrapComplete() const { return m_bootstrap_complete; }

    // Get current crop region with confidence
    CropRegion getCropRegion() const;

    // Check if detection is stable
    bool isStable() const;

    // Reset detection state
    void reset();

    // Statistics
    struct Stats {
        uint64_t frames_analyzed = 0;
        uint64_t bars_detected = 0;
        float current_confidence = 0.0f;
        CropRegion current_crop;
        bool bootstrap_complete = false;
    };
    Stats getStats() const;

private:
    // Analyze horizontal bars (letterbox)
    void analyzeHorizontal(const uint8_t* data, uint32_t width, uint32_t height,
                          int threshold, int& top, int& bottom);

    // Analyze vertical bars (pillarbox)
    void analyzeVertical(const uint8_t* data, uint32_t width, uint32_t height,
                        int threshold, int& left, int& right);

    // Check if crop is symmetric
    bool isSymmetric(int top, int bottom, int left, int right,
                    uint32_t width, uint32_t height) const;

    // Calculate confidence based on history
    float calculateConfidence() const;

    // Smooth crop transitions
    CropRegion smoothCrop(const CropRegion& target, float smoothing);

    // Parse FFmpeg cropdetect output
    bool parseFFmpegCropOutput(const std::string& output, uint32_t frame_width,
                              uint32_t frame_height, CropRegion& result);

    // Update stable crop from FFmpeg results
    void seedHistoryWithBootstrap(const CropRegion& bootstrap_crop);

    // Detection history for temporal stability
    std::deque<CropRegion> m_history;
    size_t m_max_history = 30;

    // Current crop
    CropRegion m_current_crop;
    CropRegion m_stable_crop;
    CropRegion m_bootstrap_crop;  // Results from FFmpeg bootstrap

    // Bootstrap state
    bool m_bootstrap_complete = false;
    uint64_t m_bootstrap_delay_frames = 0;

    // Statistics
    mutable Stats m_stats;
};

} // namespace processing
} // namespace ares
