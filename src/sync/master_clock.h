#pragma once

#include <ares/types.h>
#include <atomic>
#include <cstdint>

namespace ares {
namespace sync {

// High-resolution master clock using CLOCK_MONOTONIC_RAW
// This clock is not affected by NTP adjustments and provides
// deterministic timing for frame submission
class MasterClock {
public:
    MasterClock();

    // Get current time from master clock (high resolution)
    Timestamp now() const;

    // Get time in nanoseconds since clock start
    int64_t nowNanoseconds() const;

    // Get elapsed time since a timestamp
    Duration elapsed(Timestamp start) const;

    // Get elapsed time in nanoseconds
    int64_t elapsedNanoseconds(int64_t start_ns) const;

    // Convert timestamp to nanoseconds
    static int64_t toNanoseconds(Timestamp ts);

    // Convert nanoseconds to timestamp
    static Timestamp fromNanoseconds(int64_t ns);

    // Sleep until a specific timestamp
    void sleepUntil(Timestamp target) const;

    // Sleep for a duration
    void sleep(Duration duration) const;

    // Get clock resolution in nanoseconds
    int64_t getResolution() const;

    // Statistics
    struct Stats {
        int64_t resolution_ns;      // Clock resolution
        int64_t uptime_ns;          // Time since clock creation
        uint64_t now_calls;         // Number of now() calls
        double avg_call_time_ns;   // Average time for now() call
    };

    Stats getStats() const;

private:
    // Clock start time (CLOCK_MONOTONIC_RAW)
    int64_t m_start_time_ns;

    // Clock resolution
    int64_t m_resolution_ns;

    // Statistics (atomic for thread-safety)
    mutable std::atomic<uint64_t> m_now_calls{0};
    mutable std::atomic<uint64_t> m_total_call_time_ns{0};

    // Get raw clock value in nanoseconds
    int64_t getRawTime() const;
};

} // namespace sync
} // namespace ares
