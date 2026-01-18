#pragma once

#include <ares/types.h>
#include <vector>
#include <string>

namespace ares {
namespace display {

class EDIDParser {
public:
    Result parseEDID(const std::string& edid_path, std::vector<DisplayMode>& modes);
};

} // namespace display
} // namespace ares
