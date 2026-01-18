#pragma once

#include <ares/types.h>

namespace ares {
namespace capture {

class DeckLinkCapture {
public:
    DeckLinkCapture();
    ~DeckLinkCapture();

    Result initialize(int device_index);
    Result start();
    Result stop();
    Result getFrame(VideoFrame& frame);

private:
    bool m_initialized = false;
};

} // namespace capture
} // namespace ares
