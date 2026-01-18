#include "frame_buffer.h"
#include "core/logger.h"
#include <algorithm>
#include <cstring>

namespace ares {
namespace capture {

FrameBuffer::FrameBuffer(size_t capacity) : m_capacity(capacity) {
    LOG_INFO("FrameBuffer", "Created with capacity %zu", capacity);
}

FrameBuffer::~FrameBuffer() {
    clear();
}

Result FrameBuffer::push(const VideoFrame& frame, bool drop_oldest_on_full) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Check if buffer is full
    if (m_queue.size() >= m_capacity) {
        if (!drop_oldest_on_full) {
            LOG_WARN("FrameBuffer", "Buffer full, cannot push frame");
            return Result::ERROR_OUT_OF_MEMORY;
        }

        // Drop oldest frame
        if (!m_queue.empty()) {
            m_queue.pop();

            std::lock_guard<std::mutex> stats_lock(m_stats_mutex);
            m_stats.frames_dropped++;

            LOG_DEBUG("FrameBuffer", "Dropped oldest frame, queue full");
        }
    }

    // Create buffered frame with timing info
    BufferedFrame buffered;

    // Deep copy frame data
    buffered.frame.width = frame.width;
    buffered.frame.height = frame.height;
    buffered.frame.format = frame.format;
    buffered.frame.pts = frame.pts;
    buffered.frame.hdr_metadata = frame.hdr_metadata;
    buffered.frame.interlaced = frame.interlaced;
    buffered.frame.size = frame.size;

    if (frame.data && frame.size > 0) {
        buffered.frame.data = new uint8_t[frame.size];
        std::memcpy(buffered.frame.data, frame.data, frame.size);
    } else {
        buffered.frame.data = nullptr;
    }

    // Set timing information
    buffered.timing.arrival_time = std::chrono::steady_clock::now();
    buffered.timing.target_time = frame.pts; // Target time is the PTS
    buffered.timing.latency = buffered.timing.target_time - buffered.timing.arrival_time;
    buffered.timing.is_late = buffered.timing.arrival_time > buffered.timing.target_time;
    buffered.timing.is_dropped = false;
    buffered.timing.is_repeated = false;

    // Add to queue
    m_queue.push(buffered);
    m_cv.notify_one();

    // Update statistics
    {
        std::lock_guard<std::mutex> stats_lock(m_stats_mutex);
        m_stats.frames_pushed++;

        if (buffered.timing.is_late) {
            m_stats.frames_late++;
        }

        // Update latency statistics
        double latency_ms = std::chrono::duration<double, std::milli>(
            buffered.timing.latency).count();

        m_stats.avg_latency_ms = (m_stats.avg_latency_ms * (m_stats.frames_pushed - 1) + latency_ms) /
                                  m_stats.frames_pushed;

        if (latency_ms > m_stats.max_latency_ms) {
            m_stats.max_latency_ms = latency_ms;
        }
    }

    return Result::SUCCESS;
}

Result FrameBuffer::pop(VideoFrame& frame, int timeout_ms) {
    std::unique_lock<std::mutex> lock(m_mutex);

    // Wait for frame with timeout
    if (m_queue.empty()) {
        auto timeout = std::chrono::milliseconds(timeout_ms);
        if (!m_cv.wait_for(lock, timeout, [this] { return !m_queue.empty(); })) {
            // Timeout - check if we can repeat last frame
            if (m_has_last_frame) {
                LOG_DEBUG("FrameBuffer", "Timeout, repeating last frame");

                // Deep copy last frame
                frame = m_last_frame.frame;
                if (m_last_frame.frame.data && m_last_frame.frame.size > 0) {
                    frame.data = new uint8_t[m_last_frame.frame.size];
                    std::memcpy(frame.data, m_last_frame.frame.data, m_last_frame.frame.size);
                }

                std::lock_guard<std::mutex> stats_lock(m_stats_mutex);
                m_stats.frames_repeated++;

                return Result::SUCCESS;
            }

            return Result::ERROR_TIMEOUT;
        }
    }

    if (m_queue.empty()) {
        return Result::ERROR_TIMEOUT;
    }

    // Get frame from queue
    BufferedFrame buffered = std::move(m_queue.front());
    m_queue.pop();

    // Transfer frame to output
    frame = buffered.frame;

    // Save as last frame for potential repeat
    if (m_has_last_frame && m_last_frame.frame.data) {
        delete[] m_last_frame.frame.data;
    }

    m_last_frame = buffered;
    // Deep copy for last frame
    if (buffered.frame.data && buffered.frame.size > 0) {
        m_last_frame.frame.data = new uint8_t[buffered.frame.size];
        std::memcpy(m_last_frame.frame.data, buffered.frame.data, buffered.frame.size);
    }
    m_has_last_frame = true;

    // Update statistics
    {
        std::lock_guard<std::mutex> stats_lock(m_stats_mutex);
        m_stats.frames_popped++;
    }

    return Result::SUCCESS;
}

Result FrameBuffer::peek(VideoFrame& frame) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_queue.empty()) {
        return Result::ERROR_NOT_FOUND;
    }

    const BufferedFrame& buffered = m_queue.front();

    // Shallow copy for peek
    frame = buffered.frame;

    return Result::SUCCESS;
}

Result FrameBuffer::getFrameByPTS(Timestamp target_pts, VideoFrame& frame, Duration tolerance) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Search queue for frame matching PTS within tolerance
    std::queue<BufferedFrame> temp_queue;
    BufferedFrame* found_frame = nullptr;

    while (!m_queue.empty()) {
        BufferedFrame& buffered = m_queue.front();

        Duration diff = buffered.frame.pts > target_pts ?
                       buffered.frame.pts - target_pts :
                       target_pts - buffered.frame.pts;

        if (diff <= tolerance) {
            // Found matching frame
            frame = buffered.frame;
            found_frame = &buffered;

            // Restore queue
            temp_queue.push(buffered);
            m_queue.pop();
            break;
        }

        temp_queue.push(buffered);
        m_queue.pop();
    }

    // Restore remaining queue
    while (!m_queue.empty()) {
        temp_queue.push(m_queue.front());
        m_queue.pop();
    }

    m_queue = temp_queue;

    if (found_frame) {
        return Result::SUCCESS;
    }

    return Result::ERROR_NOT_FOUND;
}

bool FrameBuffer::hasFrames() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_queue.empty();
}

size_t FrameBuffer::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_queue.size();
}

void FrameBuffer::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Free all frame data
    while (!m_queue.empty()) {
        BufferedFrame& buffered = m_queue.front();
        if (buffered.frame.data) {
            delete[] buffered.frame.data;
        }
        m_queue.pop();
    }

    // Free last frame
    if (m_has_last_frame && m_last_frame.frame.data) {
        delete[] m_last_frame.frame.data;
        m_has_last_frame = false;
    }

    LOG_INFO("FrameBuffer", "Cleared all frames");
}

FrameBuffer::Stats FrameBuffer::getStats() const {
    std::lock_guard<std::mutex> lock(m_stats_mutex);
    Stats stats = m_stats;

    std::lock_guard<std::mutex> queue_lock(m_mutex);
    stats.current_queue_size = m_queue.size();

    return stats;
}

void FrameBuffer::updateStats(const BufferedFrame& frame) {
    // Statistics updated inline in push/pop methods
}

} // namespace capture
} // namespace ares
