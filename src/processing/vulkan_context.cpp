#include "vulkan_context.h"
#include "core/logger.h"

namespace ares {
namespace processing {

VulkanContext::VulkanContext() {
    LOG_INFO("Processing", "VulkanContext created (stub)");
}

VulkanContext::~VulkanContext() {
    cleanup();
}

Result VulkanContext::initialize() {
    LOG_INFO("Processing", "Initializing Vulkan (stub)");
    m_initialized = true;
    return Result::SUCCESS;
}

void VulkanContext::cleanup() {
    if (m_initialized) {
        LOG_INFO("Processing", "Cleaning up Vulkan (stub)");
        m_initialized = false;
    }
}

} // namespace processing
} // namespace ares
