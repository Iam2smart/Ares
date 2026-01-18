#pragma once

#include <ares/types.h>

namespace ares {
namespace sync {

class FrameScheduler {
public:
    FrameScheduler();
    Result scheduleFrame(const VideoFrame& frame);

private:
    bool m_initialized = false;
};

} // namespace sync
} // namespace ares
