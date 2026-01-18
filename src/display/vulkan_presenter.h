#pragma once

#include <ares/types.h>
#include <ares/display_config.h>
#include "drm_display.h"
#include "processing/vulkan_context.h"
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>

namespace ares {
namespace display {

// Vulkan-based display presenter with DRM integration
// Presents processed frames to display via Vulkan + DRM
class VulkanPresenter {
public:
    VulkanPresenter();
    ~VulkanPresenter();

    // Initialize presenter
    Result initialize(processing::VulkanContext* vk_context,
                     DRMDisplay* drm_display,
                     const DisplayConfig& config);

    // Shutdown
    void shutdown();

    // Check if initialized
    bool isInitialized() const { return m_initialized; }

    // Present frame (upload VideoFrame to display)
    Result presentFrame(const VideoFrame& frame);

    // Get current framebuffer for rendering
    uint32_t getCurrentFramebuffer() const;

    // Swap buffers
    Result swapBuffers();

    // Statistics
    struct Stats {
        uint64_t frames_presented = 0;
        uint64_t buffer_swaps = 0;
        double avg_present_time_ms = 0.0;
        double last_present_time_ms = 0.0;
        uint32_t dropped_frames = 0;
    };

    Stats getStats() const { return m_stats; }

private:
    // Initialization helpers
    Result createDrmFramebuffers();
    Result createVulkanImages();
    Result createCommandPool();
    Result createCommandBuffers();

    // Frame operations
    Result uploadFrameToGpu(const VideoFrame& frame, uint32_t buffer_index);
    Result blitToFramebuffer(uint32_t src_image_index, uint32_t fb_index);

    // DRM helpers
    uint32_t createDrmFb(uint32_t width, uint32_t height, uint32_t* handles,
                        uint32_t* pitches, uint32_t* offsets);

    // Vulkan context (not owned)
    processing::VulkanContext* m_vk_context = nullptr;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphics_queue = VK_NULL_HANDLE;

    // DRM display (not owned)
    DRMDisplay* m_drm_display = nullptr;

    // Configuration
    DisplayConfig m_config;

    // Framebuffer management
    struct DrmBuffer {
        uint32_t fb_id = 0;
        uint32_t width = 0;
        uint32_t height = 0;
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView view = VK_NULL_HANDLE;
    };

    std::vector<DrmBuffer> m_buffers;
    uint32_t m_current_buffer = 0;

    // Command pool and buffers
    VkCommandPool m_command_pool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_command_buffers;

    // Staging resources
    VkImage m_staging_image = VK_NULL_HANDLE;
    VkDeviceMemory m_staging_memory = VK_NULL_HANDLE;
    VkBuffer m_staging_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_staging_buffer_memory = VK_NULL_HANDLE;

    // Statistics
    Stats m_stats;

    // State
    bool m_initialized = false;
};

} // namespace display
} // namespace ares
