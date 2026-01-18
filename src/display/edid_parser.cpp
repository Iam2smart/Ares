#include "edid_parser.h"
#include "core/logger.h"

namespace ares {
namespace display {

Result EDIDParser::parseEDID(const std::string& edid_path, std::vector<DisplayMode>& modes) {
    LOG_INFO("Display", "Parsing EDID from %s (stub)", edid_path.c_str());
    // Stub implementation - add some default modes
    modes.push_back({1920, 1080, 60.0f, false});
    modes.push_back({3840, 2160, 60.0f, false});
    return Result::SUCCESS;
}

} // namespace display
} // namespace ares
