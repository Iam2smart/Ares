#pragma once

#include <ares/types.h>

namespace ares {
namespace sync {

class MasterClock {
public:
    MasterClock();
    Timestamp now() const;
    Duration elapsed(Timestamp start) const;

private:
    Timestamp m_start_time;
};

} // namespace sync
} // namespace ares
