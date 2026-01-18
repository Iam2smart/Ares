#pragma once

#include <ares/types.h>
#include <ares/processing_config.h>
#include "vulkan_context.h"
#include <vulkan/vulkan.h>

namespace ares {
namespace processing {

// Non-Linear Stretch (NLS) shader for aspect ratio warping
// Transforms 16:9 content to cinemascope formats (2.35:1, 2.40:1, 2.55:1)
// with minimal distortion in center and more stretch at edges
class NLSShader {
public:
    NLSShader();
    ~NLSShader();

    // Initialize NLS shader with Vulkan context
    Result initialize(VulkanContext* vk_context);

    // Process frame with NLS warping
    Result processFrame(const VideoFrame& input, VideoFrame& output,
                       const NLSConfig& config);

    // Check if initialized
    bool isInitialized() const { return m_initialized; }

    // Update shader parameters
    void updateConfig(const NLSConfig& config);

    // Statistics
    struct Stats {
        uint64_t frames_processed = 0;
        double last_frame_time_ms = 0.0;
        double avg_frame_time_ms = 0.0;
        uint32_t input_width = 0;
        uint32_t input_height = 0;
        uint32_t output_width = 0;
        uint32_t output_height = 0;
        float current_aspect_ratio = 0.0f;
    };

    Stats getStats() const;

private:
    // Initialization helpers
    Result createPipeline();
    Result createDescriptorSetLayout();
    Result createDescriptorPool();
    Result createShaderModule(const std::vector<uint32_t>& code, VkShaderModule* module);

    // Texture management
    Result createTextures(uint32_t input_width, uint32_t input_height,
                         uint32_t output_width, uint32_t output_height);
    void destroyTextures();

    // Resource management
    Result createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                       VkMemoryPropertyFlags properties,
                       VkBuffer& buffer, VkDeviceMemory& memory);
    void destroyBuffer(VkBuffer buffer, VkDeviceMemory memory);

    // Frame processing
    Result uploadFrame(const VideoFrame& frame);
    Result runCompute(const NLSConfig& config);
    Result downloadFrame(VideoFrame& output);

    // Calculate output dimensions based on target aspect ratio
    void calculateOutputDimensions(uint32_t input_width, uint32_t input_height,
                                   const NLSConfig& config,
                                   uint32_t& output_width, uint32_t& output_height);

    // Get aspect ratio value from enum
    float getTargetAspectRatio(NLSConfig::TargetAspect aspect) const;

    // Vulkan context
    VulkanContext* m_vk_context = nullptr;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_compute_queue = VK_NULL_HANDLE;

    // Pipeline
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptor_set_layout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptor_pool = VK_NULL_HANDLE;
    VkDescriptorSet m_descriptor_set = VK_NULL_HANDLE;

    // Shader
    VkShaderModule m_shader_module = VK_NULL_HANDLE;

    // Textures
    VkImage m_input_image = VK_NULL_HANDLE;
    VkDeviceMemory m_input_memory = VK_NULL_HANDLE;
    VkImageView m_input_view = VK_NULL_HANDLE;
    VkSampler m_input_sampler = VK_NULL_HANDLE;

    VkImage m_output_image = VK_NULL_HANDLE;
    VkDeviceMemory m_output_memory = VK_NULL_HANDLE;
    VkImageView m_output_view = VK_NULL_HANDLE;

    // Staging buffers
    VkBuffer m_staging_input_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_staging_input_memory = VK_NULL_HANDLE;
    VkBuffer m_staging_output_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_staging_output_memory = VK_NULL_HANDLE;

    // Uniform buffer for parameters
    VkBuffer m_uniform_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_uniform_memory = VK_NULL_HANDLE;

    // Command pool and buffers
    VkCommandPool m_command_pool = VK_NULL_HANDLE;
    VkCommandBuffer m_command_buffer = VK_NULL_HANDLE;

    // Dimensions
    uint32_t m_input_width = 0;
    uint32_t m_input_height = 0;
    uint32_t m_output_width = 0;
    uint32_t m_output_height = 0;

    // Statistics
    Stats m_stats;

    // State
    bool m_initialized = false;

    // Push constants for shader parameters (NLS-Next style)
    struct PushConstants {
        float horizontal_stretch;
        float vertical_stretch;
        float crop_amount;
        float bars_amount;
        float center_protect;
        float input_width;
        float input_height;
        float output_width;
        float output_height;
        uint32_t interpolation_quality;
        uint32_t padding[2];  // Alignment to 16-byte boundary
    };
};

} // namespace processing
} // namespace ares
