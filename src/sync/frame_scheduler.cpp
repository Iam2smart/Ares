#include "frame_scheduler.h"
#include "core/logger.h"

namespace ares {
namespace sync {

FrameScheduler::FrameScheduler() {
    LOG_INFO("Sync", "FrameScheduler created (stub)");
}

Result FrameScheduler::scheduleFrame(const VideoFrame& frame) {
    // Stub implementation
    return Result::SUCCESS;
}

} // namespace sync
} // namespace ares
