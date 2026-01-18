#include "vulkan_presenter.h"
#include "core/logger.h"
#include <chrono>
#include <cstring>

namespace ares {
namespace display {

VulkanPresenter::VulkanPresenter() {
    LOG_INFO("Display", "VulkanPresenter created");
}

VulkanPresenter::~VulkanPresenter() {
    shutdown();
}

Result VulkanPresenter::initialize(processing::VulkanContext* vk_context,
                                   DRMDisplay* drm_display,
                                   const DisplayConfig& config) {
    if (m_initialized) {
        LOG_WARN("Display", "VulkanPresenter already initialized");
        return Result::SUCCESS;
    }

    if (!vk_context || !vk_context->isInitialized()) {
        LOG_ERROR("Display", "Invalid Vulkan context");
        return Result::ERROR_INVALID_PARAMETER;
    }

    if (!drm_display || !drm_display->isInitialized()) {
        LOG_ERROR("Display", "Invalid DRM display");
        return Result::ERROR_INVALID_PARAMETER;
    }

    m_vk_context = vk_context;
    m_device = vk_context->getDevice();
    m_graphics_queue = vk_context->getGraphicsQueue();
    m_drm_display = drm_display;
    m_config = config;

    LOG_INFO("Display", "Initializing Vulkan presenter");

    // Create command pool
    Result result = createCommandPool();
    if (result != Result::SUCCESS) {
        return result;
    }

    // Create Vulkan images for display
    result = createVulkanImages();
    if (result != Result::SUCCESS) {
        shutdown();
        return result;
    }

    // Create DRM framebuffers
    result = createDrmFramebuffers();
    if (result != Result::SUCCESS) {
        shutdown();
        return result;
    }

    // Create command buffers
    result = createCommandBuffers();
    if (result != Result::SUCCESS) {
        shutdown();
        return result;
    }

    m_initialized = true;
    LOG_INFO("Display", "Vulkan presenter initialized successfully");
    LOG_INFO("Display", "Buffers: %zu (triple buffering)", m_buffers.size());

    return Result::SUCCESS;
}

Result VulkanPresenter::createCommandPool() {
    m_command_pool = m_vk_context->createCommandPool(
        m_vk_context->getGraphicsQueueFamily(),
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    if (m_command_pool == VK_NULL_HANDLE) {
        LOG_ERROR("Display", "Failed to create command pool");
        return Result::ERROR_GENERIC;
    }

    return Result::SUCCESS;
}

Result VulkanPresenter::createVulkanImages() {
    // Get display mode
    auto mode = m_drm_display->getCurrentMode();
    uint32_t width = mode.width;
    uint32_t height = mode.height;

    LOG_INFO("Display", "Creating Vulkan images: %ux%u", width, height);

    // Create buffers (triple buffering)
    m_buffers.resize(m_config.buffer_count);

    for (auto& buffer : m_buffers) {
        buffer.width = width;
        buffer.height = height;

        // Create Vulkan image
        VkImageCreateInfo image_info = {};
        image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        image_info.imageType = VK_IMAGE_TYPE_2D;
        image_info.format = VK_FORMAT_B8G8R8A8_UNORM;  // Standard BGRA format
        image_info.extent.width = width;
        image_info.extent.height = height;
        image_info.extent.depth = 1;
        image_info.mipLevels = 1;
        image_info.arrayLayers = 1;
        image_info.samples = VK_SAMPLE_COUNT_1_BIT;
        image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
        image_info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
        image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        if (vkCreateImage(m_device, &image_info, nullptr, &buffer.image) != VK_SUCCESS) {
            LOG_ERROR("Display", "Failed to create Vulkan image");
            return Result::ERROR_GENERIC;
        }

        // Allocate memory
        VkMemoryRequirements mem_reqs;
        vkGetImageMemoryRequirements(m_device, buffer.image, &mem_reqs);

        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_reqs.size;
        alloc_info.memoryTypeIndex = m_vk_context->findMemoryType(
            mem_reqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(m_device, &alloc_info, nullptr, &buffer.memory) != VK_SUCCESS) {
            LOG_ERROR("Display", "Failed to allocate image memory");
            return Result::ERROR_GENERIC;
        }

        vkBindImageMemory(m_device, buffer.image, buffer.memory, 0);

        // Create image view
        VkImageViewCreateInfo view_info = {};
        view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view_info.image = buffer.image;
        view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view_info.format = VK_FORMAT_B8G8R8A8_UNORM;
        view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        view_info.subresourceRange.baseMipLevel = 0;
        view_info.subresourceRange.levelCount = 1;
        view_info.subresourceRange.baseArrayLayer = 0;
        view_info.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_device, &view_info, nullptr, &buffer.view) != VK_SUCCESS) {
            LOG_ERROR("Display", "Failed to create image view");
            return Result::ERROR_GENERIC;
        }
    }

    LOG_INFO("Display", "Created %zu Vulkan images", m_buffers.size());
    return Result::SUCCESS;
}

Result VulkanPresenter::createDrmFramebuffers() {
    LOG_INFO("Display", "Creating DRM framebuffers (placeholder)");

    // Note: Full DRM framebuffer creation would require:
    // 1. Export Vulkan images as DMA-BUF
    // 2. Import DMA-BUF into DRM
    // 3. Create DRM framebuffer from DRM bo (buffer object)
    // This is simplified for now

    for (auto& buffer : m_buffers) {
        // Placeholder FB ID (in production, would create actual DRM FBs)
        buffer.fb_id = 0;
    }

    return Result::SUCCESS;
}

Result VulkanPresenter::createCommandBuffers() {
    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = m_command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = m_buffers.size();

    m_command_buffers.resize(m_buffers.size());

    if (vkAllocateCommandBuffers(m_device, &alloc_info,
                                m_command_buffers.data()) != VK_SUCCESS) {
        LOG_ERROR("Display", "Failed to allocate command buffers");
        return Result::ERROR_GENERIC;
    }

    LOG_INFO("Display", "Created %zu command buffers", m_command_buffers.size());
    return Result::SUCCESS;
}

Result VulkanPresenter::presentFrame(const VideoFrame& frame) {
    if (!m_initialized) {
        LOG_ERROR("Display", "Presenter not initialized");
        return Result::ERROR_NOT_INITIALIZED;
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    // Upload frame to GPU
    Result result = uploadFrameToGpu(frame, m_current_buffer);
    if (result != Result::SUCCESS) {
        return result;
    }

    // Blit to framebuffer
    result = blitToFramebuffer(m_current_buffer, m_current_buffer);
    if (result != Result::SUCCESS) {
        return result;
    }

    // Present to display
    result = swapBuffers();
    if (result != Result::SUCCESS) {
        return result;
    }

    // Update statistics
    auto end_time = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration<double, std::milli>(end_time - start_time);

    m_stats.last_present_time_ms = elapsed.count();
    m_stats.frames_presented++;

    if (m_stats.frames_presented == 1) {
        m_stats.avg_present_time_ms = m_stats.last_present_time_ms;
    } else {
        m_stats.avg_present_time_ms =
            (m_stats.avg_present_time_ms * (m_stats.frames_presented - 1) +
             m_stats.last_present_time_ms) / m_stats.frames_presented;
    }

    return Result::SUCCESS;
}

Result VulkanPresenter::uploadFrameToGpu(const VideoFrame& frame, uint32_t buffer_index) {
    if (buffer_index >= m_buffers.size()) {
        LOG_ERROR("Display", "Invalid buffer index: %u", buffer_index);
        return Result::ERROR_INVALID_PARAMETER;
    }

    // TODO: Implement actual frame upload
    // Would use staging buffer to upload frame data to GPU image

    return Result::SUCCESS;
}

Result VulkanPresenter::blitToFramebuffer(uint32_t src_image_index, uint32_t fb_index) {
    if (src_image_index >= m_buffers.size() || fb_index >= m_buffers.size()) {
        LOG_ERROR("Display", "Invalid buffer indices");
        return Result::ERROR_INVALID_PARAMETER;
    }

    // TODO: Implement blit operation
    // Would use Vulkan command buffer to blit/copy image

    return Result::SUCCESS;
}

Result VulkanPresenter::swapBuffers() {
    // Wait for vsync if enabled
    if (m_config.vsync) {
        Result result = m_drm_display->waitForVBlank();
        if (result != Result::SUCCESS) {
            LOG_WARN("Display", "VBlank wait failed");
        }
    }

    // Present current buffer
    if (m_buffers[m_current_buffer].fb_id != 0) {
        Result result = m_drm_display->pageFlip(
            m_buffers[m_current_buffer].fb_id,
            nullptr);

        if (result != Result::SUCCESS) {
            LOG_ERROR("Display", "Page flip failed");
            m_stats.dropped_frames++;
            return result;
        }
    }

    // Advance to next buffer
    m_current_buffer = (m_current_buffer + 1) % m_buffers.size();
    m_stats.buffer_swaps++;

    return Result::SUCCESS;
}

uint32_t VulkanPresenter::getCurrentFramebuffer() const {
    if (m_current_buffer < m_buffers.size()) {
        return m_buffers[m_current_buffer].fb_id;
    }
    return 0;
}

void VulkanPresenter::shutdown() {
    if (!m_initialized) {
        return;
    }

    LOG_INFO("Display", "Shutting down Vulkan presenter");

    // Wait for device idle
    if (m_device) {
        vkDeviceWaitIdle(m_device);
    }

    // Destroy command buffers
    if (!m_command_buffers.empty() && m_command_pool) {
        vkFreeCommandBuffers(m_device, m_command_pool,
                            m_command_buffers.size(),
                            m_command_buffers.data());
        m_command_buffers.clear();
    }

    // Destroy staging resources
    if (m_staging_image) {
        vkDestroyImage(m_device, m_staging_image, nullptr);
        m_staging_image = VK_NULL_HANDLE;
    }
    if (m_staging_memory) {
        vkFreeMemory(m_device, m_staging_memory, nullptr);
        m_staging_memory = VK_NULL_HANDLE;
    }
    if (m_staging_buffer) {
        vkDestroyBuffer(m_device, m_staging_buffer, nullptr);
        m_staging_buffer = VK_NULL_HANDLE;
    }
    if (m_staging_buffer_memory) {
        vkFreeMemory(m_device, m_staging_buffer_memory, nullptr);
        m_staging_buffer_memory = VK_NULL_HANDLE;
    }

    // Destroy buffers
    for (auto& buffer : m_buffers) {
        if (buffer.view) {
            vkDestroyImageView(m_device, buffer.view, nullptr);
        }
        if (buffer.image) {
            vkDestroyImage(m_device, buffer.image, nullptr);
        }
        if (buffer.memory) {
            vkFreeMemory(m_device, buffer.memory, nullptr);
        }
    }
    m_buffers.clear();

    // Destroy command pool
    if (m_command_pool) {
        m_vk_context->destroyCommandPool(m_command_pool);
        m_command_pool = VK_NULL_HANDLE;
    }

    m_initialized = false;
    LOG_INFO("Display", "Vulkan presenter shut down");
}

} // namespace display
} // namespace ares
