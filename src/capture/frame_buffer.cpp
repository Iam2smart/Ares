#include "frame_buffer.h"

namespace ares {
namespace capture {

FrameBuffer::FrameBuffer(size_t size) : m_capacity(size) {
    m_buffer.resize(size);
}

FrameBuffer::~FrameBuffer() = default;

Result FrameBuffer::push(const VideoFrame& frame) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Stub implementation
    return Result::SUCCESS;
}

Result FrameBuffer::pop(VideoFrame& frame) {
    std::lock_guard<std::mutex> lock(m_mutex);
    // Stub implementation
    return Result::ERROR_TIMEOUT;
}

size_t FrameBuffer::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return 0;
}

} // namespace capture
} // namespace ares
