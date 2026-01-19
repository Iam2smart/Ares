#pragma once

#include <ares/types.h>
#include <chrono>
#include <deque>

namespace ares {
namespace sync {

// Frame scheduling algorithm
enum class SchedulingAlgorithm {
    IMMEDIATE,      // Present immediately (lowest latency, may cause tearing)
    VSYNC,          // Wait for vsync (eliminates tearing)
    ADAPTIVE,       // Adaptive vsync (vsync when >= refresh, immediate when below)
    FRAME_PACING    // Frame pacing with sleep + busy wait (best smoothness)
};

// Frame scheduler for precise timing and judder-free playback
class FrameScheduler {
public:
    FrameScheduler();
    ~FrameScheduler();

    // Initialize scheduler with display refresh rate
    Result initialize(float display_refresh_rate, SchedulingAlgorithm algorithm = SchedulingAlgorithm::FRAME_PACING);

    // Schedule when to present a frame (blocks until presentation time)
    Result scheduleFrame(const VideoFrame& frame);

    // Set source frame rate (for frame rate conversion)
    void setSourceFrameRate(float fps);

    // Set display refresh rate (for dynamic refresh rate changes)
    void setDisplayRefreshRate(float hz);

    // Set scheduling algorithm
    void setAlgorithm(SchedulingAlgorithm algorithm);

    // Enable/disable VRR mode
    void setVRREnabled(bool enabled);

    // Get statistics
    struct Stats {
        uint64_t frames_scheduled = 0;
        uint64_t frames_dropped = 0;
        uint64_t frames_duplicated = 0;

        double avg_presentation_latency_ms = 0.0;
        double avg_frame_interval_ms = 0.0;
        double avg_sleep_error_ms = 0.0;

        double last_presentation_time_ms = 0.0;
        double source_fps = 0.0;
        double display_refresh_hz = 0.0;

        bool vrr_enabled = false;
        SchedulingAlgorithm algorithm = SchedulingAlgorithm::IMMEDIATE;
    };

    Stats getStats() const;

    // Reset statistics
    void resetStats();

private:
    using Clock = std::chrono::high_resolution_clock;
    using TimePoint = std::chrono::time_point<Clock>;
    using Duration = std::chrono::nanoseconds;

    // Calculate next presentation time
    TimePoint calculateNextPresentationTime();

    // Wait until presentation time (sleep + busy wait)
    void waitUntilPresentationTime(TimePoint target_time);

    // Determine if frame should be dropped (for frame rate conversion)
    bool shouldDropFrame();

    // Determine if frame should be duplicated
    bool shouldDuplicateFrame();

    // Update running statistics
    void updateStats(Duration presentation_latency, Duration frame_interval);

    // Configuration
    float m_display_refresh_rate = 60.0f;  // Hz
    float m_source_frame_rate = 60.0f;     // fps
    Duration m_display_frame_duration;      // nanoseconds between frames
    Duration m_source_frame_duration;       // nanoseconds between source frames

    SchedulingAlgorithm m_algorithm = SchedulingAlgorithm::FRAME_PACING;
    bool m_vrr_enabled = false;

    // Timing state
    TimePoint m_last_presentation_time;
    TimePoint m_next_scheduled_time;
    TimePoint m_first_frame_time;
    uint64_t m_frame_number = 0;

    // Frame rate conversion state
    double m_frame_accumulator = 0.0;  // For fractional frame rate conversion

    // Frame history (for adaptive algorithms)
    std::deque<TimePoint> m_presentation_history;
    static constexpr size_t MAX_HISTORY_SIZE = 60;

    // Statistics
    Stats m_stats;
    double m_latency_sum_ms = 0.0;
    double m_interval_sum_ms = 0.0;
    double m_sleep_error_sum_ms = 0.0;

    // State
    bool m_initialized = false;
    bool m_first_frame = true;
};

} // namespace sync
} // namespace ares
