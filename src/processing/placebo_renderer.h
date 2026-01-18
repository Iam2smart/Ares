#pragma once

#include <ares/types.h>

namespace ares {
namespace processing {

class PlaceboRenderer {
public:
    PlaceboRenderer();
    ~PlaceboRenderer();

    Result initialize();
    Result processFrame(const VideoFrame& input, VideoFrame& output);

private:
    bool m_initialized = false;
};

} // namespace processing
} // namespace ares
