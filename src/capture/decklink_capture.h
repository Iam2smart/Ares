#pragma once

#include <ares/types.h>
#include <atomic>
#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>

// Forward declarations for DeckLink SDK
class IDeckLink;
class IDeckLinkInput;
class IDeckLinkDisplayMode;
class IDeckLinkVideoInputFrame;
class IDeckLinkInputCallback;

namespace ares {
namespace capture {

// Configuration for capture
struct CaptureConfig {
    int device_index = 0;
    uint32_t width = 3840;
    uint32_t height = 2160;
    float frame_rate = 60.0f;
    bool enable_10bit = true;
    std::string input_connection = "HDMI"; // HDMI, SDI, etc.
};

// Internal DeckLink callback handler
class DeckLinkCaptureCallback;

class DeckLinkCapture {
public:
    DeckLinkCapture();
    ~DeckLinkCapture();

    // Initialize with device index
    Result initialize(int device_index);

    // Initialize with full configuration
    Result initialize(const CaptureConfig& config);

    // Start/stop capture
    Result start();
    Result stop();

    // Get next available frame (blocking with timeout)
    Result getFrame(VideoFrame& frame, int timeout_ms = 100);

    // Check if frames are available
    bool hasFrame() const;

    // Get current capture statistics
    struct Stats {
        uint64_t frames_captured = 0;
        uint64_t frames_dropped = 0;
        double current_fps = 0.0;
        double detected_fps = 0.0;      // Detected input frame rate
        bool frame_rate_stable = false; // Frame rate detection stable
        uint32_t queue_size = 0;
    };
    Stats getStats() const;

    // Get detected input frame rate
    double getDetectedFrameRate() const { return m_detected_frame_rate; }
    bool isFrameRateStable() const { return m_frame_rate_stable; }

private:
    friend class DeckLinkCaptureCallback;

    // Internal frame handling
    void onFrameReceived(IDeckLinkVideoInputFrame* video_frame, int64_t pts);

    // Parse HDR metadata from frame
    void parseHDRMetadata(IDeckLinkVideoInputFrame* video_frame, HDRMetadata& metadata);

    // DeckLink objects
    IDeckLink* m_decklink = nullptr;
    IDeckLinkInput* m_decklink_input = nullptr;
    DeckLinkCaptureCallback* m_callback = nullptr;

    // Configuration
    CaptureConfig m_config;

    // Frame queue
    struct QueuedFrame {
        std::unique_ptr<uint8_t[]> data;
        size_t size;
        uint32_t width;
        uint32_t height;
        PixelFormat format;
        Timestamp pts;
        HDRMetadata hdr_metadata;
    };

    std::queue<QueuedFrame> m_frame_queue;
    mutable std::mutex m_queue_mutex;
    std::condition_variable m_queue_cv;
    static constexpr size_t MAX_QUEUE_SIZE = 5;

    // Statistics
    mutable std::mutex m_stats_mutex;
    Stats m_stats;
    Timestamp m_last_frame_time;

    // Frame rate detection
    void detectFrameRate(int64_t pts);
    std::vector<double> m_frame_intervals;  // Recent frame intervals for detection
    double m_detected_frame_rate = 0.0;
    bool m_frame_rate_stable = false;
    int64_t m_last_pts = 0;
    static constexpr size_t FRAME_RATE_SAMPLES = 30;

    // State
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_running{false};
};

} // namespace capture
} // namespace ares
