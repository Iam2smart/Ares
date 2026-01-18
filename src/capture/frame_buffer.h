#pragma once

#include <ares/types.h>
#include <mutex>
#include <vector>
#include <queue>
#include <condition_variable>

namespace ares {
namespace capture {

// Frame timing information
struct FrameTiming {
    Timestamp arrival_time;    // When frame arrived in buffer
    Timestamp target_time;     // When frame should be displayed
    Duration latency;          // Arrival to target latency
    bool is_late;             // Frame arrived after target time
    bool is_dropped;          // Frame was dropped
    bool is_repeated;         // Frame was repeated from previous
};

class FrameBuffer {
public:
    explicit FrameBuffer(size_t capacity = 3);
    ~FrameBuffer();

    // Push frame into buffer (returns ERROR if full and drop_on_full is false)
    Result push(const VideoFrame& frame, bool drop_oldest_on_full = true);

    // Pop next frame for display (blocks until frame available or timeout)
    Result pop(VideoFrame& frame, int timeout_ms = 100);

    // Peek at next frame without removing
    Result peek(VideoFrame& frame) const;

    // Get frame by PTS (for jitter correction)
    Result getFrameByPTS(Timestamp target_pts, VideoFrame& frame, Duration tolerance);

    // Check if frames are available
    bool hasFrames() const;

    // Get number of queued frames
    size_t size() const;

    // Get buffer capacity
    size_t capacity() const { return m_capacity; }

    // Clear all frames
    void clear();

    // Frame timing statistics
    struct Stats {
        uint64_t frames_pushed = 0;
        uint64_t frames_popped = 0;
        uint64_t frames_dropped = 0;
        uint64_t frames_repeated = 0;
        uint64_t frames_late = 0;
        double avg_latency_ms = 0.0;
        double max_latency_ms = 0.0;
        size_t current_queue_size = 0;
    };

    Stats getStats() const;

private:
    struct BufferedFrame {
        VideoFrame frame;
        FrameTiming timing;
    };

    std::queue<BufferedFrame> m_queue;
    size_t m_capacity;

    mutable std::mutex m_mutex;
    std::condition_variable m_cv;

    // Statistics
    mutable std::mutex m_stats_mutex;
    Stats m_stats;

    // Last popped frame (for repeat logic)
    BufferedFrame m_last_frame;
    bool m_has_last_frame = false;

    // Helper to update statistics
    void updateStats(const BufferedFrame& frame);
};

} // namespace capture
} // namespace ares
