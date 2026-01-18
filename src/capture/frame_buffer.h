#pragma once

#include <ares/types.h>
#include <mutex>
#include <vector>

namespace ares {
namespace capture {

class FrameBuffer {
public:
    explicit FrameBuffer(size_t size);
    ~FrameBuffer();

    Result push(const VideoFrame& frame);
    Result pop(VideoFrame& frame);
    size_t size() const;

private:
    std::vector<VideoFrame> m_buffer;
    size_t m_capacity;
    size_t m_head = 0;
    size_t m_tail = 0;
    mutable std::mutex m_mutex;
};

} // namespace capture
} // namespace ares
