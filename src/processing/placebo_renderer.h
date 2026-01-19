#pragma once

#include <ares/types.h>
#include <ares/processing_config.h>
#include "vulkan_context.h"

#include <libplacebo/vulkan.h>
#include <libplacebo/renderer.h>
#include <libplacebo/shaders/colorspace.h>
#include <memory>

namespace ares {
namespace processing {

class PlaceboRenderer {
public:
    PlaceboRenderer();
    ~PlaceboRenderer();

    // Initialize with Vulkan context
    Result initialize(VulkanContext* vk_context);

    // Process a frame with HDR tone mapping
    Result processFrame(const VideoFrame& input, VideoFrame& output,
                       const ProcessingConfig& config);

    // Update configuration (can be called per-frame for real-time adjustments)
    void updateConfig(const ProcessingConfig& config);

    // Get processing statistics
    struct Stats {
        double last_frame_time_ms;
        double avg_frame_time_ms;
        uint64_t frames_processed;
        bool using_10bit;
        std::string tone_mapping_algorithm;
    };
    Stats getStats() const;

    // Get libplacebo GPU (for use by other processors)
    pl_gpu getGPU() const { return m_gpu; }

private:
    // Initialize libplacebo
    Result initializePlacebo(VulkanContext* vk_context);

    // Create rendering objects
    Result createRenderer();
    Result createTextures(uint32_t width, uint32_t height);

    // Upload frame to GPU
    Result uploadFrame(const VideoFrame& frame);

    // Render with tone mapping
    Result render(const ProcessingConfig& config);

    // Download result from GPU
    Result downloadFrame(VideoFrame& output);

    // Helper to convert tone mapping algorithm
    pl_tone_map_function getToneMappingFunction(ToneMappingAlgorithm algo);

    // Vulkan context (not owned)
    VulkanContext* m_vk_context = nullptr;

    // libplacebo objects
    pl_log m_log = nullptr;
    pl_vulkan m_vk = nullptr;
    pl_gpu m_gpu = nullptr;
    pl_renderer m_renderer = nullptr;

    // Textures
    pl_tex m_input_tex = nullptr;
    pl_tex m_output_tex = nullptr;

    // Frame dimensions
    uint32_t m_width = 0;
    uint32_t m_height = 0;

    // Statistics
    mutable Stats m_stats = {};
    Timestamp m_last_frame_time;

    // State
    bool m_initialized = false;
};

} // namespace processing
} // namespace ares
