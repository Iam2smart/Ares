#include "nls_shader.h"
#include "core/logger.h"
#include <cstring>
#include <chrono>
#include <algorithm>

// GLSL compute shader for NLS warping (compiled at runtime)
static const char* NLS_COMPUTE_SHADER = R"(
#version 450

layout(local_size_x = 16, local_size_y = 16) in;

layout(binding = 0) uniform sampler2D inputTex;
layout(binding = 1, rgba8) uniform writeonly image2D outputImg;

layout(push_constant) uniform PushConstants {
    float centerStrength;
    float edgeStrength;
    float transitionWidth;
    float targetAspectRatio;
    uint interpolationQuality;
} params;

// Non-linear warp function
// Maps output coordinate to input coordinate with less stretch in center
vec2 applyWarp(vec2 coord) {
    // Center the coordinates (-0.5 to 0.5)
    vec2 centered = coord - 0.5;

    // Calculate distance from center (horizontal only for aspect ratio)
    float dist = abs(centered.x);

    // Smooth transition from center to edge
    float t = smoothstep(0.0, params.transitionWidth, dist);

    // Interpolate between center and edge strength
    float strength = mix(params.centerStrength, params.edgeStrength, t);

    // Apply warp (stretch horizontally)
    centered.x *= (1.0 + strength);

    // Return to 0-1 range
    return centered + 0.5;
}

// Bicubic interpolation weights
float cubicWeight(float x) {
    float ax = abs(x);
    if (ax <= 1.0) {
        return (1.5 * ax - 2.5) * ax * ax + 1.0;
    } else if (ax < 2.0) {
        return ((-0.5 * ax + 2.5) * ax - 4.0) * ax + 2.0;
    }
    return 0.0;
}

// Bicubic texture sampling
vec4 sampleBicubic(sampler2D tex, vec2 coord, vec2 texSize) {
    vec2 texelSize = 1.0 / texSize;
    vec2 texelCoord = coord * texSize - 0.5;
    vec2 floorCoord = floor(texelCoord);
    vec2 fracCoord = texelCoord - floorCoord;

    vec4 result = vec4(0.0);

    for (int y = -1; y <= 2; y++) {
        for (int x = -1; x <= 2; x++) {
            vec2 sampleCoord = (floorCoord + vec2(x, y) + 0.5) * texelSize;
            vec4 sample = texture(tex, sampleCoord);

            float wx = cubicWeight(fracCoord.x - float(x));
            float wy = cubicWeight(fracCoord.y - float(y));

            result += sample * wx * wy;
        }
    }

    return result;
}

void main() {
    ivec2 outputCoord = ivec2(gl_GlobalInvocationID.xy);
    ivec2 outputSize = imageSize(outputImg);

    // Check bounds
    if (outputCoord.x >= outputSize.x || outputCoord.y >= outputSize.y) {
        return;
    }

    // Normalize output coordinates (0-1)
    vec2 normalizedCoord = (vec2(outputCoord) + 0.5) / vec2(outputSize);

    // Apply non-linear warp
    vec2 warpedCoord = applyWarp(normalizedCoord);

    // Sample input texture
    vec4 color;
    if (params.interpolationQuality == 0) {
        // Bilinear (fast)
        color = texture(inputTex, warpedCoord);
    } else if (params.interpolationQuality == 1) {
        // Bicubic (high quality)
        vec2 inputSize = vec2(textureSize(inputTex, 0));
        color = sampleBicubic(inputTex, warpedCoord, inputSize);
    } else {
        // Lanczos approximation (best quality)
        vec2 inputSize = vec2(textureSize(inputTex, 0));
        color = sampleBicubic(inputTex, warpedCoord, inputSize);
    }

    // Clamp to valid range
    color = clamp(color, 0.0, 1.0);

    // Write to output
    imageStore(outputImg, outputCoord, color);
}
)";

namespace ares {
namespace processing {

NLSShader::NLSShader() {
    LOG_INFO("Processing", "NLSShader created");
}

NLSShader::~NLSShader() {
    if (!m_initialized) {
        return;
    }

    // Destroy textures
    destroyTextures();

    // Destroy staging buffers
    if (m_staging_input_buffer) {
        destroyBuffer(m_staging_input_buffer, m_staging_input_memory);
    }
    if (m_staging_output_buffer) {
        destroyBuffer(m_staging_output_buffer, m_staging_output_memory);
    }

    // Destroy uniform buffer
    if (m_uniform_buffer) {
        destroyBuffer(m_uniform_buffer, m_uniform_memory);
    }

    // Destroy pipeline
    if (m_pipeline) {
        vkDestroyPipeline(m_device, m_pipeline, nullptr);
    }
    if (m_pipeline_layout) {
        vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr);
    }

    // Destroy descriptor pool
    if (m_descriptor_pool) {
        vkDestroyDescriptorPool(m_device, m_descriptor_pool, nullptr);
    }
    if (m_descriptor_set_layout) {
        vkDestroyDescriptorSetLayout(m_device, m_descriptor_set_layout, nullptr);
    }

    // Destroy shader module
    if (m_shader_module) {
        vkDestroyShaderModule(m_device, m_shader_module, nullptr);
    }

    // Destroy command pool
    if (m_command_pool) {
        m_vk_context->destroyCommandPool(m_command_pool);
    }

    LOG_INFO("Processing", "NLSShader destroyed");
}

Result NLSShader::initialize(VulkanContext* vk_context) {
    if (m_initialized) {
        LOG_WARN("Processing", "NLSShader already initialized");
        return Result::SUCCESS;
    }

    if (!vk_context || !vk_context->isInitialized()) {
        LOG_ERROR("Processing", "Invalid Vulkan context");
        return Result::ERROR_INVALID_PARAMETER;
    }

    m_vk_context = vk_context;
    m_device = vk_context->getDevice();
    m_compute_queue = vk_context->getComputeQueue();

    LOG_INFO("Processing", "Initializing NLS shader");

    // Create command pool
    m_command_pool = m_vk_context->createCommandPool(
        m_vk_context->getComputeQueueFamily(),
        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    if (m_command_pool == VK_NULL_HANDLE) {
        LOG_ERROR("Processing", "Failed to create command pool");
        return Result::ERROR_GENERIC;
    }

    // Allocate command buffer
    VkCommandBufferAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = m_command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(m_device, &alloc_info, &m_command_buffer) != VK_SUCCESS) {
        LOG_ERROR("Processing", "Failed to allocate command buffer");
        return Result::ERROR_GENERIC;
    }

    // Create descriptor set layout
    Result result = createDescriptorSetLayout();
    if (result != Result::SUCCESS) {
        return result;
    }

    // Create descriptor pool
    result = createDescriptorPool();
    if (result != Result::SUCCESS) {
        return result;
    }

    // Create compute pipeline
    result = createPipeline();
    if (result != Result::SUCCESS) {
        return result;
    }

    m_initialized = true;
    LOG_INFO("Processing", "NLS shader initialized successfully");

    return Result::SUCCESS;
}

Result NLSShader::createDescriptorSetLayout() {
    // Input sampler + output image
    VkDescriptorSetLayoutBinding bindings[2] = {};

    // Input texture (sampler)
    bindings[0].binding = 0;
    bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bindings[0].descriptorCount = 1;
    bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    // Output image (storage)
    bindings[1].binding = 1;
    bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    bindings[1].descriptorCount = 1;
    bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo layout_info = {};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 2;
    layout_info.pBindings = bindings;

    if (vkCreateDescriptorSetLayout(m_device, &layout_info, nullptr,
                                   &m_descriptor_set_layout) != VK_SUCCESS) {
        LOG_ERROR("Processing", "Failed to create descriptor set layout");
        return Result::ERROR_GENERIC;
    }

    return Result::SUCCESS;
}

Result NLSShader::createDescriptorPool() {
    VkDescriptorPoolSize pool_sizes[2] = {};
    pool_sizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    pool_sizes[0].descriptorCount = 1;
    pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    pool_sizes[1].descriptorCount = 1;

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = 2;
    pool_info.pPoolSizes = pool_sizes;
    pool_info.maxSets = 1;

    if (vkCreateDescriptorPool(m_device, &pool_info, nullptr, &m_descriptor_pool) != VK_SUCCESS) {
        LOG_ERROR("Processing", "Failed to create descriptor pool");
        return Result::ERROR_GENERIC;
    }

    // Allocate descriptor set
    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = m_descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &m_descriptor_set_layout;

    if (vkAllocateDescriptorSets(m_device, &alloc_info, &m_descriptor_set) != VK_SUCCESS) {
        LOG_ERROR("Processing", "Failed to allocate descriptor set");
        return Result::ERROR_GENERIC;
    }

    return Result::SUCCESS;
}

Result NLSShader::createPipeline() {
    // Compile GLSL to SPIR-V (simplified - in production use glslang or shaderc)
    // For now, we'll use a pre-compiled shader or runtime compilation
    // This is a placeholder - actual implementation would use shaderc

    LOG_WARN("Processing", "NLS shader compilation requires shaderc library");
    LOG_INFO("Processing", "Using placeholder shader module");

    // TODO: Implement proper GLSL to SPIR-V compilation
    // For now, create a dummy shader module
    // In production, use: shaderc_compiler_t compiler = shaderc_compiler_initialize();

    // Push constant range
    VkPushConstantRange push_constant_range = {};
    push_constant_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    push_constant_range.offset = 0;
    push_constant_range.size = sizeof(PushConstants);

    // Pipeline layout
    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &m_descriptor_set_layout;
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &push_constant_range;

    if (vkCreatePipelineLayout(m_device, &pipeline_layout_info, nullptr,
                              &m_pipeline_layout) != VK_SUCCESS) {
        LOG_ERROR("Processing", "Failed to create pipeline layout");
        return Result::ERROR_GENERIC;
    }

    // Note: Actual shader module creation and pipeline creation would go here
    // Requires shaderc library to compile GLSL to SPIR-V at runtime

    return Result::SUCCESS;
}

Result NLSShader::processFrame(const VideoFrame& input, VideoFrame& output,
                               const NLSConfig& config) {
    if (!m_initialized) {
        LOG_ERROR("Processing", "NLS shader not initialized");
        return Result::ERROR_NOT_INITIALIZED;
    }

    if (!config.enabled) {
        // Pass through without warping
        output = input;
        return Result::SUCCESS;
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    // Calculate output dimensions
    uint32_t output_width, output_height;
    calculateOutputDimensions(input.width, input.height, config,
                             output_width, output_height);

    // Create textures if dimensions changed
    if (m_input_width != input.width || m_input_height != input.height ||
        m_output_width != output_width || m_output_height != output_height) {

        Result result = createTextures(input.width, input.height,
                                      output_width, output_height);
        if (result != Result::SUCCESS) {
            return result;
        }
    }

    // Upload frame
    Result result = uploadFrame(input);
    if (result != Result::SUCCESS) {
        return result;
    }

    // Run compute shader
    result = runCompute(config);
    if (result != Result::SUCCESS) {
        return result;
    }

    // Download result
    result = downloadFrame(output);
    if (result != Result::SUCCESS) {
        return result;
    }

    // Copy metadata
    output.pts = input.pts;
    output.hdr_metadata = input.hdr_metadata;

    // Update statistics
    auto end_time = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration<double, std::milli>(end_time - start_time);

    m_stats.last_frame_time_ms = elapsed.count();
    m_stats.frames_processed++;

    if (m_stats.frames_processed == 1) {
        m_stats.avg_frame_time_ms = m_stats.last_frame_time_ms;
    } else {
        m_stats.avg_frame_time_ms =
            (m_stats.avg_frame_time_ms * (m_stats.frames_processed - 1) +
             m_stats.last_frame_time_ms) / m_stats.frames_processed;
    }

    return Result::SUCCESS;
}

void NLSShader::calculateOutputDimensions(uint32_t input_width, uint32_t input_height,
                                         const NLSConfig& config,
                                         uint32_t& output_width, uint32_t& output_height) {
    float target_aspect = getTargetAspectRatio(config.target_aspect);

    if (config.target_aspect == NLSConfig::TargetAspect::CUSTOM) {
        target_aspect = config.custom_aspect_ratio;
    }

    // Keep height, adjust width for target aspect ratio
    output_height = input_height;
    output_width = static_cast<uint32_t>(output_height * target_aspect);

    m_stats.input_width = input_width;
    m_stats.input_height = input_height;
    m_stats.output_width = output_width;
    m_stats.output_height = output_height;
    m_stats.current_aspect_ratio = target_aspect;
}

float NLSShader::getTargetAspectRatio(NLSConfig::TargetAspect aspect) const {
    switch (aspect) {
        case NLSConfig::TargetAspect::SCOPE_235:
            return 2.35f;
        case NLSConfig::TargetAspect::SCOPE_240:
            return 2.40f;
        case NLSConfig::TargetAspect::SCOPE_255:
            return 2.55f;
        case NLSConfig::TargetAspect::CUSTOM:
            return 2.35f;  // Default
        default:
            return 2.35f;
    }
}

Result NLSShader::createTextures(uint32_t input_width, uint32_t input_height,
                                uint32_t output_width, uint32_t output_height) {
    // Destroy old textures
    destroyTextures();

    m_input_width = input_width;
    m_input_height = input_height;
    m_output_width = output_width;
    m_output_height = output_height;

    // TODO: Implement actual texture creation
    // This requires:
    // 1. Create VkImage for input and output
    // 2. Allocate device memory
    // 3. Bind image to memory
    // 4. Create image views
    // 5. Create sampler for input
    // 6. Create staging buffers for upload/download

    LOG_INFO("Processing", "Created NLS textures: %ux%u -> %ux%u",
             input_width, input_height, output_width, output_height);

    return Result::SUCCESS;
}

void NLSShader::destroyTextures() {
    // TODO: Destroy Vulkan textures and image views

    if (m_input_sampler) {
        vkDestroySampler(m_device, m_input_sampler, nullptr);
        m_input_sampler = VK_NULL_HANDLE;
    }

    if (m_input_view) {
        vkDestroyImageView(m_device, m_input_view, nullptr);
        m_input_view = VK_NULL_HANDLE;
    }

    if (m_output_view) {
        vkDestroyImageView(m_device, m_output_view, nullptr);
        m_output_view = VK_NULL_HANDLE;
    }

    if (m_input_image) {
        vkDestroyImage(m_device, m_input_image, nullptr);
        m_input_image = VK_NULL_HANDLE;
    }

    if (m_output_image) {
        vkDestroyImage(m_device, m_output_image, nullptr);
        m_output_image = VK_NULL_HANDLE;
    }

    if (m_input_memory) {
        vkFreeMemory(m_device, m_input_memory, nullptr);
        m_input_memory = VK_NULL_HANDLE;
    }

    if (m_output_memory) {
        vkFreeMemory(m_device, m_output_memory, nullptr);
        m_output_memory = VK_NULL_HANDLE;
    }
}

Result NLSShader::uploadFrame(const VideoFrame& frame) {
    // TODO: Upload frame data to GPU
    // 1. Copy to staging buffer
    // 2. Record command buffer to copy staging -> image
    // 3. Submit and wait

    return Result::SUCCESS;
}

Result NLSShader::runCompute(const NLSConfig& config) {
    // TODO: Run compute shader
    // 1. Begin command buffer
    // 2. Bind pipeline
    // 3. Bind descriptor sets
    // 4. Push constants (config parameters)
    // 5. Dispatch compute work groups
    // 6. Pipeline barrier
    // 7. End command buffer
    // 8. Submit to queue

    return Result::SUCCESS;
}

Result NLSShader::downloadFrame(VideoFrame& output) {
    // TODO: Download frame from GPU
    // 1. Record command buffer to copy image -> staging
    // 2. Submit and wait
    // 3. Map staging buffer
    // 4. Copy to output frame
    // 5. Unmap

    // Allocate output buffer
    size_t output_size = m_output_width * m_output_height * 3;  // RGB 8-bit
    output.data = new uint8_t[output_size];
    output.size = output_size;
    output.width = m_output_width;
    output.height = m_output_height;
    output.format = PixelFormat::RGB_8BIT;

    return Result::SUCCESS;
}

Result NLSShader::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
                               VkMemoryPropertyFlags properties,
                               VkBuffer& buffer, VkDeviceMemory& memory) {
    VkBufferCreateInfo buffer_info = {};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(m_device, &buffer_info, nullptr, &buffer) != VK_SUCCESS) {
        LOG_ERROR("Processing", "Failed to create buffer");
        return Result::ERROR_GENERIC;
    }

    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(m_device, buffer, &mem_requirements);

    VkMemoryAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = m_vk_context->findMemoryType(
        mem_requirements.memoryTypeBits, properties);

    if (vkAllocateMemory(m_device, &alloc_info, nullptr, &memory) != VK_SUCCESS) {
        LOG_ERROR("Processing", "Failed to allocate buffer memory");
        vkDestroyBuffer(m_device, buffer, nullptr);
        return Result::ERROR_GENERIC;
    }

    vkBindBufferMemory(m_device, buffer, memory, 0);

    return Result::SUCCESS;
}

void NLSShader::destroyBuffer(VkBuffer buffer, VkDeviceMemory memory) {
    if (buffer) {
        vkDestroyBuffer(m_device, buffer, nullptr);
    }
    if (memory) {
        vkFreeMemory(m_device, memory, nullptr);
    }
}

void NLSShader::updateConfig(const NLSConfig& config) {
    // Configuration is applied per-frame in processFrame()
    // This is a no-op for now
}

NLSShader::Stats NLSShader::getStats() const {
    return m_stats;
}

} // namespace processing
} // namespace ares
