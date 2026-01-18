#include "vulkan_osd.h"
#include "core/logger.h"

namespace ares {
namespace osd {

VulkanOSD::VulkanOSD() {
    LOG_INFO("OSD", "VulkanOSD created (stub)");
}

VulkanOSD::~VulkanOSD() = default;

Result VulkanOSD::initialize() {
    LOG_INFO("OSD", "Initializing OSD (stub)");
    m_initialized = true;
    return Result::SUCCESS;
}

Result VulkanOSD::render() {
    // Stub implementation
    return Result::SUCCESS;
}

} // namespace osd
} // namespace ares
