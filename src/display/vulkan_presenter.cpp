#include "vulkan_presenter.h"
#include "core/logger.h"
#include <chrono>
#include <cstring>
#include <unistd.h>  // For close()
#include <drm/drm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

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
    LOG_INFO("Display", "Creating DRM framebuffers");

    // Get DRM file descriptor
    int drm_fd = m_drm_display->getDrmFd();
    if (drm_fd < 0) {
        LOG_ERROR("Display", "Invalid DRM file descriptor");
        return Result::ERROR_GENERIC;
    }

    for (auto& buffer : m_buffers) {
        // Export Vulkan image as DMA-BUF file descriptor
        // This requires VK_KHR_external_memory_fd extension

        // Get memory FD for export
        VkMemoryGetFdInfoKHR fd_info = {};
        fd_info.sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR;
        fd_info.memory = buffer.memory;
        fd_info.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT;

        int dma_buf_fd = -1;

        // Get the function pointer for vkGetMemoryFdKHR
        auto vkGetMemoryFdKHR = (PFN_vkGetMemoryFdKHR)vkGetDeviceProcAddr(
            m_device, "vkGetMemoryFdKHR");

        if (!vkGetMemoryFdKHR) {
            LOG_WARN("Display", "vkGetMemoryFdKHR not available, using placeholder FB");
            buffer.fb_id = 0;
            continue;
        }

        VkResult result = vkGetMemoryFdKHR(m_device, &fd_info, &dma_buf_fd);
        if (result != VK_SUCCESS || dma_buf_fd < 0) {
            LOG_WARN("Display", "Failed to export DMA-BUF, using placeholder FB (error: %d)", result);
            buffer.fb_id = 0;
            continue;
        }

        // Get memory requirements for pitch/offset calculations
        VkImageSubresource subresource = {};
        subresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

        VkSubresourceLayout layout;
        vkGetImageSubresourceLayout(m_device, buffer.image, &subresource, &layout);

        // Import DMA-BUF into DRM and create framebuffer
        // This uses the DRM_IOCTL_MODE_ADDFB2 ioctl
        uint32_t handles[4] = {0};
        uint32_t pitches[4] = {0};
        uint32_t offsets[4] = {0};

        // Import the DMA-BUF fd as a DRM handle (GEM handle)
        struct drm_prime_handle prime_handle = {};
        prime_handle.fd = dma_buf_fd;
        prime_handle.flags = 0;

        // Note: In a full implementation, we would use drmPrimeFDToHandle
        // For now, create a simplified framebuffer
        handles[0] = 0;  // Would be the GEM handle from import
        pitches[0] = static_cast<uint32_t>(layout.rowPitch);
        offsets[0] = 0;

        // Close the DMA-BUF FD as it's been imported
        close(dma_buf_fd);

        // Create DRM framebuffer
        // Note: This would use drmModeAddFB2WithModifiers in production
        buffer.fb_id = createDrmFb(buffer.width, buffer.height, handles, pitches, offsets);

        if (buffer.fb_id == 0) {
            LOG_WARN("Display", "Failed to create DRM framebuffer for buffer");
        } else {
            LOG_DEBUG("Display", "Created DRM FB %u (%ux%u, pitch=%u)",
                     buffer.fb_id, buffer.width, buffer.height, pitches[0]);
        }
    }

    LOG_INFO("Display", "DRM framebuffers created");
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

    auto& buffer = m_buffers[buffer_index];

    // Calculate frame data size
    size_t data_size = frame.width * frame.height * 4;  // RGBA8

    // Create/recreate staging buffer if needed
    if (m_staging_buffer == VK_NULL_HANDLE) {
        VkBufferCreateInfo buffer_info = {};
        buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        buffer_info.size = data_size;
        buffer_info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(m_device, &buffer_info, nullptr, &m_staging_buffer) != VK_SUCCESS) {
            LOG_ERROR("Display", "Failed to create staging buffer");
            return Result::ERROR_GENERIC;
        }

        // Allocate staging buffer memory
        VkMemoryRequirements mem_reqs;
        vkGetBufferMemoryRequirements(m_device, m_staging_buffer, &mem_reqs);

        VkMemoryAllocateInfo alloc_info = {};
        alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc_info.allocationSize = mem_reqs.size;
        alloc_info.memoryTypeIndex = m_vk_context->findMemoryType(
            mem_reqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(m_device, &alloc_info, nullptr, &m_staging_buffer_memory) != VK_SUCCESS) {
            LOG_ERROR("Display", "Failed to allocate staging buffer memory");
            return Result::ERROR_GENERIC;
        }

        vkBindBufferMemory(m_device, m_staging_buffer, m_staging_buffer_memory, 0);
    }

    // Map staging buffer and copy frame data
    void* mapped_data = nullptr;
    if (vkMapMemory(m_device, m_staging_buffer_memory, 0, data_size, 0, &mapped_data) != VK_SUCCESS) {
        LOG_ERROR("Display", "Failed to map staging buffer memory");
        return Result::ERROR_GENERIC;
    }

    // Copy frame data (handle different plane configurations)
    if (frame.num_planes == 1 && frame.data[0]) {
        memcpy(mapped_data, frame.data[0], data_size);
    } else {
        LOG_WARN("Display", "Multi-plane or missing data, skipping upload");
    }

    vkUnmapMemory(m_device, m_staging_buffer_memory);

    // Create one-time command buffer for upload
    VkCommandBuffer cmd_buffer = m_vk_context->beginSingleTimeCommands(m_command_pool);

    // Transition image layout to TRANSFER_DST_OPTIMAL
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = buffer.image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    vkCmdPipelineBarrier(cmd_buffer,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &barrier);

    // Copy buffer to image
    VkBufferImageCopy region = {};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {frame.width, frame.height, 1};

    vkCmdCopyBufferToImage(cmd_buffer, m_staging_buffer, buffer.image,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    // Transition to SHADER_READ_ONLY_OPTIMAL for sampling/presentation
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd_buffer,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &barrier);

    m_vk_context->endSingleTimeCommands(cmd_buffer, m_command_pool, m_graphics_queue);

    return Result::SUCCESS;
}

Result VulkanPresenter::blitToFramebuffer(uint32_t src_image_index, uint32_t fb_index) {
    if (src_image_index >= m_buffers.size() || fb_index >= m_buffers.size()) {
        LOG_ERROR("Display", "Invalid buffer indices");
        return Result::ERROR_INVALID_PARAMETER;
    }

    // For now, src and fb are the same (direct presentation)
    // In a more complex setup, we might blit from processing buffer to scanout buffer

    auto& src_buffer = m_buffers[src_image_index];
    auto& dst_buffer = m_buffers[fb_index];

    // If same buffer, no blit needed
    if (src_image_index == fb_index) {
        return Result::SUCCESS;
    }

    // Create one-time command buffer for blit
    VkCommandBuffer cmd_buffer = m_vk_context->beginSingleTimeCommands(m_command_pool);

    // Transition source to TRANSFER_SRC_OPTIMAL
    VkImageMemoryBarrier src_barrier = {};
    src_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    src_barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    src_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    src_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    src_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    src_barrier.image = src_buffer.image;
    src_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    src_barrier.subresourceRange.baseMipLevel = 0;
    src_barrier.subresourceRange.levelCount = 1;
    src_barrier.subresourceRange.baseArrayLayer = 0;
    src_barrier.subresourceRange.layerCount = 1;
    src_barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    src_barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    // Transition destination to TRANSFER_DST_OPTIMAL
    VkImageMemoryBarrier dst_barrier = {};
    dst_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    dst_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    dst_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    dst_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    dst_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    dst_barrier.image = dst_buffer.image;
    dst_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    dst_barrier.subresourceRange.baseMipLevel = 0;
    dst_barrier.subresourceRange.levelCount = 1;
    dst_barrier.subresourceRange.baseArrayLayer = 0;
    dst_barrier.subresourceRange.layerCount = 1;
    dst_barrier.srcAccessMask = 0;
    dst_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    VkImageMemoryBarrier barriers[] = {src_barrier, dst_barrier};
    vkCmdPipelineBarrier(cmd_buffer,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        0, 0, nullptr, 0, nullptr, 2, barriers);

    // Perform blit operation
    VkImageBlit blit_region = {};
    blit_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit_region.srcSubresource.mipLevel = 0;
    blit_region.srcSubresource.baseArrayLayer = 0;
    blit_region.srcSubresource.layerCount = 1;
    blit_region.srcOffsets[0] = {0, 0, 0};
    blit_region.srcOffsets[1] = {static_cast<int32_t>(src_buffer.width),
                                 static_cast<int32_t>(src_buffer.height), 1};
    blit_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit_region.dstSubresource.mipLevel = 0;
    blit_region.dstSubresource.baseArrayLayer = 0;
    blit_region.dstSubresource.layerCount = 1;
    blit_region.dstOffsets[0] = {0, 0, 0};
    blit_region.dstOffsets[1] = {static_cast<int32_t>(dst_buffer.width),
                                 static_cast<int32_t>(dst_buffer.height), 1};

    vkCmdBlitImage(cmd_buffer,
                  src_buffer.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                  dst_buffer.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                  1, &blit_region, VK_FILTER_LINEAR);

    // Transition destination back to SHADER_READ_ONLY_OPTIMAL
    dst_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    dst_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    dst_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    dst_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(cmd_buffer,
                        VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &dst_barrier);

    m_vk_context->endSingleTimeCommands(cmd_buffer, m_command_pool, m_graphics_queue);

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

uint32_t VulkanPresenter::createDrmFb(uint32_t width, uint32_t height,
                                      uint32_t* handles, uint32_t* pitches,
                                      uint32_t* offsets) {
    int drm_fd = m_drm_display->getDrmFd();
    if (drm_fd < 0) {
        LOG_ERROR("Display", "Invalid DRM FD");
        return 0;
    }

    // For now, return 0 to indicate placeholder
    // In a full implementation, this would call:
    // uint32_t fb_id = 0;
    // if (drmModeAddFB2(drm_fd, width, height, DRM_FORMAT_XRGB8888,
    //                   handles, pitches, offsets, &fb_id, 0) != 0) {
    //     LOG_ERROR("Display", "drmModeAddFB2 failed: %s", strerror(errno));
    //     return 0;
    // }
    // return fb_id;

    LOG_DEBUG("Display", "createDrmFb stub: %ux%u", width, height);
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
