#include "master_clock.h"
#include <chrono>

namespace ares {
namespace sync {

MasterClock::MasterClock() : m_start_time(std::chrono::steady_clock::now()) {
}

Timestamp MasterClock::now() const {
    return std::chrono::steady_clock::now();
}

Duration MasterClock::elapsed(Timestamp start) const {
    return std::chrono::duration_cast<Duration>(now() - start);
}

} // namespace sync
} // namespace ares
