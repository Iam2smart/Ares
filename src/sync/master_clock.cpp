#include "master_clock.h"
#include "core/logger.h"

#include <chrono>
#include <time.h>
#include <errno.h>
#include <thread>

namespace ares {
namespace sync {

MasterClock::MasterClock() {
    // Get current time from CLOCK_MONOTONIC_RAW
    m_start_time_ns = getRawTime();

    // Get clock resolution
    struct timespec res;
    if (clock_getres(CLOCK_MONOTONIC_RAW, &res) == 0) {
        m_resolution_ns = res.tv_sec * 1000000000LL + res.tv_nsec;
    } else {
        m_resolution_ns = 1; // Assume 1ns resolution if query fails
    }

    LOG_INFO("Sync", "MasterClock initialized with CLOCK_MONOTONIC_RAW");
    LOG_INFO("Sync", "Clock resolution: %ld ns", m_resolution_ns);
}

int64_t MasterClock::getRawTime() const {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) != 0) {
        LOG_ERROR("Sync", "Failed to get CLOCK_MONOTONIC_RAW: %d", errno);
        // Fallback to steady_clock
        auto now = std::chrono::steady_clock::now();
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
            now.time_since_epoch()).count();
    }

    return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}

Timestamp MasterClock::now() const {
    auto start = std::chrono::high_resolution_clock::now();

    int64_t raw_ns = getRawTime();

    // Measure overhead
    auto end = std::chrono::high_resolution_clock::now();
    auto overhead = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

    m_now_calls.fetch_add(1, std::memory_order_relaxed);
    m_total_call_time_ns.fetch_add(overhead, std::memory_order_relaxed);

    // Convert nanoseconds to Timestamp
    return fromNanoseconds(raw_ns);
}

int64_t MasterClock::nowNanoseconds() const {
    m_now_calls.fetch_add(1, std::memory_order_relaxed);
    return getRawTime();
}

Duration MasterClock::elapsed(Timestamp start) const {
    return std::chrono::duration_cast<Duration>(now() - start);
}

int64_t MasterClock::elapsedNanoseconds(int64_t start_ns) const {
    return getRawTime() - start_ns;
}

int64_t MasterClock::toNanoseconds(Timestamp ts) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        ts.time_since_epoch()).count();
}

Timestamp MasterClock::fromNanoseconds(int64_t ns) {
    return Timestamp(std::chrono::nanoseconds(ns));
}

void MasterClock::sleepUntil(Timestamp target) const {
    struct timespec target_ts;
    int64_t target_ns = toNanoseconds(target);

    target_ts.tv_sec = target_ns / 1000000000LL;
    target_ts.tv_nsec = target_ns % 1000000000LL;

    // Use clock_nanosleep with CLOCK_MONOTONIC_RAW and TIMER_ABSTIME
    int result = clock_nanosleep(CLOCK_MONOTONIC_RAW, TIMER_ABSTIME, &target_ts, nullptr);

    if (result != 0 && result != EINTR) {
        LOG_WARN("Sync", "clock_nanosleep failed: %d", result);
        // Fallback to busy-wait for remaining time
        while (nowNanoseconds() < target_ns) {
            std::this_thread::yield();
        }
    }
}

void MasterClock::sleep(Duration duration) const {
    Timestamp target = now() + duration;
    sleepUntil(target);
}

int64_t MasterClock::getResolution() const {
    return m_resolution_ns;
}

MasterClock::Stats MasterClock::getStats() const {
    Stats stats;

    stats.resolution_ns = m_resolution_ns;
    stats.uptime_ns = getRawTime() - m_start_time_ns;

    uint64_t calls = m_now_calls.load(std::memory_order_relaxed);
    uint64_t total_time = m_total_call_time_ns.load(std::memory_order_relaxed);

    stats.now_calls = calls;
    stats.avg_call_time_ns = calls > 0 ? (double)total_time / calls : 0.0;

    return stats;
}

} // namespace sync
} // namespace ares
