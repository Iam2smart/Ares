#include "frame_scheduler.h"
#include "core/logger.h"
#include <thread>
#include <cmath>

namespace ares {
namespace sync {

FrameScheduler::FrameScheduler() {
    LOG_INFO("Sync", "FrameScheduler created");
}

FrameScheduler::~FrameScheduler() {
}

Result FrameScheduler::initialize(float display_refresh_rate, SchedulingAlgorithm algorithm) {
    if (display_refresh_rate <= 0.0f) {
        LOG_ERROR("Sync", "Invalid display refresh rate: %.2f", display_refresh_rate);
        return Result::ERROR_INVALID_PARAMETER;
    }

    m_display_refresh_rate = display_refresh_rate;
    m_source_frame_rate = display_refresh_rate;  // Assume matching initially
    m_algorithm = algorithm;

    // Calculate frame durations
    m_display_frame_duration = Duration(static_cast<int64_t>(1e9 / display_refresh_rate));
    m_source_frame_duration = Duration(static_cast<int64_t>(1e9 / m_source_frame_rate));

    // Initialize timing state
    m_last_presentation_time = Clock::now();
    m_next_scheduled_time = m_last_presentation_time;
    m_first_frame = true;
    m_frame_number = 0;
    m_frame_accumulator = 0.0;

    // Reset statistics
    resetStats();

    m_initialized = true;

    const char* algo_name = "Unknown";
    switch (algorithm) {
        case SchedulingAlgorithm::IMMEDIATE: algo_name = "Immediate"; break;
        case SchedulingAlgorithm::VSYNC: algo_name = "VSync"; break;
        case SchedulingAlgorithm::ADAPTIVE: algo_name = "Adaptive"; break;
        case SchedulingAlgorithm::FRAME_PACING: algo_name = "Frame Pacing"; break;
    }

    LOG_INFO("Sync", "FrameScheduler initialized: %.2f Hz, algorithm: %s",
             display_refresh_rate, algo_name);

    return Result::SUCCESS;
}

Result FrameScheduler::scheduleFrame(const VideoFrame& frame) {
    if (!m_initialized) {
        return Result::ERROR_NOT_INITIALIZED;
    }

    TimePoint now = Clock::now();

    // First frame - establish baseline
    if (m_first_frame) {
        m_first_frame_time = now;
        m_last_presentation_time = now;
        m_next_scheduled_time = now;
        m_first_frame = false;
        m_stats.frames_scheduled++;
        return Result::SUCCESS;
    }

    // Frame rate conversion - check if we should drop this frame
    if (m_source_frame_rate != m_display_refresh_rate) {
        if (shouldDropFrame()) {
            m_stats.frames_dropped++;
            LOG_DEBUG("Sync", "Frame %lu dropped (FPS conversion)", m_frame_number);
            m_frame_number++;
            return Result::SUCCESS;
        }
    }

    // Calculate next presentation time
    TimePoint target_time = calculateNextPresentationTime();

    // Different handling based on algorithm
    switch (m_algorithm) {
        case SchedulingAlgorithm::IMMEDIATE:
            // Present immediately, no waiting
            break;

        case SchedulingAlgorithm::VSYNC:
        case SchedulingAlgorithm::ADAPTIVE:
        case SchedulingAlgorithm::FRAME_PACING:
            // Wait until target presentation time
            waitUntilPresentationTime(target_time);
            break;
    }

    // Update timing state
    TimePoint presentation_time = Clock::now();
    Duration presentation_latency = presentation_time - now;
    Duration frame_interval = presentation_time - m_last_presentation_time;

    // Update statistics
    updateStats(presentation_latency, frame_interval);

    // Track presentation history
    m_presentation_history.push_back(presentation_time);
    if (m_presentation_history.size() > MAX_HISTORY_SIZE) {
        m_presentation_history.pop_front();
    }

    // Update state for next frame
    m_last_presentation_time = presentation_time;
    m_next_scheduled_time = target_time + m_display_frame_duration;
    m_frame_number++;
    m_stats.frames_scheduled++;

    return Result::SUCCESS;
}

void FrameScheduler::setSourceFrameRate(float fps) {
    if (fps <= 0.0f) {
        LOG_WARN("Sync", "Invalid source frame rate: %.2f", fps);
        return;
    }

    m_source_frame_rate = fps;
    m_source_frame_duration = Duration(static_cast<int64_t>(1e9 / fps));
    m_frame_accumulator = 0.0;  // Reset accumulator

    LOG_INFO("Sync", "Source frame rate set to %.3f fps", fps);
}

void FrameScheduler::setDisplayRefreshRate(float hz) {
    if (hz <= 0.0f) {
        LOG_WARN("Sync", "Invalid display refresh rate: %.2f", hz);
        return;
    }

    m_display_refresh_rate = hz;
    m_display_frame_duration = Duration(static_cast<int64_t>(1e9 / hz));

    LOG_INFO("Sync", "Display refresh rate set to %.2f Hz", hz);
}

void FrameScheduler::setAlgorithm(SchedulingAlgorithm algorithm) {
    m_algorithm = algorithm;
}

void FrameScheduler::setVRREnabled(bool enabled) {
    m_vrr_enabled = enabled;
    LOG_INFO("Sync", "VRR %s", enabled ? "enabled" : "disabled");
}

FrameScheduler::TimePoint FrameScheduler::calculateNextPresentationTime() {
    // For VRR, calculate based on source frame rate
    if (m_vrr_enabled) {
        return m_last_presentation_time + m_source_frame_duration;
    }

    // For fixed refresh rate, align to display vsync intervals
    TimePoint now = Clock::now();
    Duration elapsed_since_last = now - m_last_presentation_time;

    // If we're late, schedule for next vsync
    if (elapsed_since_last > m_display_frame_duration) {
        // Calculate how many frames we're late
        int64_t frames_late = elapsed_since_last.count() / m_display_frame_duration.count();
        return m_last_presentation_time + (m_display_frame_duration * (frames_late + 1));
    }

    // Schedule for next regular interval
    return m_last_presentation_time + m_display_frame_duration;
}

void FrameScheduler::waitUntilPresentationTime(TimePoint target_time) {
    TimePoint now = Clock::now();

    if (target_time <= now) {
        // Already past target time, don't wait
        return;
    }

    Duration sleep_duration = target_time - now;

    // Sleep for most of the duration (leave 2ms for busy wait)
    constexpr auto BUSY_WAIT_THRESHOLD = std::chrono::milliseconds(2);

    if (sleep_duration > BUSY_WAIT_THRESHOLD) {
        Duration sleep_time = sleep_duration - BUSY_WAIT_THRESHOLD;

        TimePoint sleep_start = Clock::now();
        std::this_thread::sleep_for(sleep_time);
        TimePoint sleep_end = Clock::now();

        // Track sleep error for statistics
        Duration actual_sleep = sleep_end - sleep_start;
        Duration sleep_error = actual_sleep > sleep_time ?
                              (actual_sleep - sleep_time) : (sleep_time - actual_sleep);

        m_sleep_error_sum_ms += std::chrono::duration<double, std::milli>(sleep_error).count();
    }

    // Busy wait for remaining time (for precise timing)
    while (Clock::now() < target_time) {
        // Tight loop for precision
        std::this_thread::yield();
    }
}

bool FrameScheduler::shouldDropFrame() {
    // Frame rate conversion using accumulator
    // Accumulator tracks how much "time" we've accumulated relative to display

    double source_to_display_ratio = m_source_frame_rate / m_display_refresh_rate;

    // Each source frame adds this much to the accumulator
    m_frame_accumulator += source_to_display_ratio;

    // If accumulator < 1.0, we should present this frame
    // If accumulator >= 1.0, we've already presented enough frames

    if (m_frame_accumulator >= 1.0) {
        // Drop this frame
        m_frame_accumulator -= 1.0;
        return true;
    }

    return false;
}

bool FrameScheduler::shouldDuplicateFrame() {
    // For frame rate conversion where source < display
    // This is handled by the accumulator in shouldDropFrame()
    // If accumulator doesn't reach 1.0, frame gets presented (effectively duplicated)
    return false;
}

void FrameScheduler::updateStats(Duration presentation_latency, Duration frame_interval) {
    // Convert to milliseconds
    double latency_ms = std::chrono::duration<double, std::milli>(presentation_latency).count();
    double interval_ms = std::chrono::duration<double, std::milli>(frame_interval).count();

    // Update running sums
    m_latency_sum_ms += latency_ms;
    m_interval_sum_ms += interval_ms;

    // Calculate running averages
    if (m_stats.frames_scheduled > 0) {
        m_stats.avg_presentation_latency_ms = m_latency_sum_ms / m_stats.frames_scheduled;
        m_stats.avg_frame_interval_ms = m_interval_sum_ms / m_stats.frames_scheduled;

        if (m_stats.frames_scheduled > 1) {
            m_stats.avg_sleep_error_ms = m_sleep_error_sum_ms / (m_stats.frames_scheduled - 1);
        }
    }

    m_stats.last_presentation_time_ms = latency_ms;
}

FrameScheduler::Stats FrameScheduler::getStats() const {
    Stats stats = m_stats;
    stats.source_fps = m_source_frame_rate;
    stats.display_refresh_hz = m_display_refresh_rate;
    stats.vrr_enabled = m_vrr_enabled;
    stats.algorithm = m_algorithm;
    return stats;
}

void FrameScheduler::resetStats() {
    m_stats = Stats();
    m_latency_sum_ms = 0.0;
    m_interval_sum_ms = 0.0;
    m_sleep_error_sum_ms = 0.0;
    m_presentation_history.clear();

    LOG_DEBUG("Sync", "Statistics reset");
}

} // namespace sync
} // namespace ares
