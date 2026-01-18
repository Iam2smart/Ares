#pragma once

#include <ares/types.h>
#include <string>

namespace ares {
namespace display {

class DRMDisplay {
public:
    DRMDisplay();
    ~DRMDisplay();

    Result initialize(const std::string& connector);
    Result setMode(const DisplayMode& mode);
    Result presentFrame(const VideoFrame& frame);

private:
    bool m_initialized = false;
};

} // namespace display
} // namespace ares
