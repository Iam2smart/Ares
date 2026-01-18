#pragma once

#include <ares/types.h>

namespace ares {
namespace osd {

class VulkanOSD {
public:
    VulkanOSD();
    ~VulkanOSD();

    Result initialize();
    Result render();

private:
    bool m_initialized = false;
};

} // namespace osd
} // namespace ares
