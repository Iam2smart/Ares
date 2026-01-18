#pragma once

#include <ares/types.h>

namespace ares {
namespace processing {

class VulkanContext {
public:
    VulkanContext();
    ~VulkanContext();

    Result initialize();
    void cleanup();

private:
    bool m_initialized = false;
};

} // namespace processing
} // namespace ares
