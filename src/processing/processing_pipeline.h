#pragma once

#include <ares/types.h>
#include <ares/processing_config.h>
#include "vulkan_context.h"
#include "placebo_renderer.h"
#include "black_bar_detector.h"
#include "nls_shader.h"
#include "../osd/osd_renderer.h"
#include "../osd/menu_system.h"
#include "../input/ir_remote.h"
#include <memory>
#include <functional>

namespace ares {
namespace processing {

// Main processing pipeline that coordinates all processing stages
class ProcessingPipeline {
public:
    ProcessingPipeline();
    ~ProcessingPipeline();

    // Initialize the entire processing pipeline
    Result initialize(const ProcessingConfig& config);

    // Shutdown and cleanup
    void shutdown();

    // Process a single frame through the entire pipeline
    // Pipeline stages:
    // 1. Black bar detection (optional)
    // 2. NLS aspect ratio warping (optional)
    // 3. HDR tone mapping
    // 4. Color adjustments
    // 5. OSD compositing (optional)
    Result processFrame(const VideoFrame& input, VideoFrame& output);

    // OSD system access
    osd::MenuSystem* getMenuSystem() { return m_menu_system.get(); }
    osd::OSDRenderer* getOSDRenderer() { return m_osd_renderer.get(); }
    input::IRRemote* getIRRemote() { return m_ir_remote.get(); }

    // Update configuration
    void updateConfig(const ProcessingConfig& config);

    // Get current configuration
    const ProcessingConfig& getConfig() const { return m_config; }

    // Check if initialized
    bool isInitialized() const { return m_initialized; }

    // Statistics from all stages
    struct Stats {
        // Pipeline
        uint64_t frames_processed = 0;
        double total_frame_time_ms = 0.0;
        double avg_frame_time_ms = 0.0;

        // Individual stages
        BlackBarDetector::Stats black_bar_stats;
        NLSShader::Stats nls_stats;
        PlaceboRenderer::Stats tone_mapping_stats;

        // Frame dimensions through pipeline
        uint32_t input_width = 0;
        uint32_t input_height = 0;
        uint32_t after_crop_width = 0;
        uint32_t after_crop_height = 0;
        uint32_t after_nls_width = 0;
        uint32_t after_nls_height = 0;
        uint32_t output_width = 0;
        uint32_t output_height = 0;

        // Current crop region
        CropRegion current_crop;
        bool crop_stable = false;
    };

    Stats getStats() const;

    // Frame callback for monitoring
    using FrameCallback = std::function<void(const VideoFrame& frame, const char* stage)>;
    void setFrameCallback(FrameCallback callback) { m_frame_callback = callback; }

private:
    // Initialization helpers
    Result initializeVulkan();
    Result initializeProcessors();

    // Processing stages
    Result detectBlackBars(const VideoFrame& frame);
    Result applyCrop(const VideoFrame& input, VideoFrame& output);
    Result applyNLS(const VideoFrame& input, VideoFrame& output);
    Result applyToneMapping(const VideoFrame& input, VideoFrame& output);
    Result compositeOSD(const VideoFrame& input, VideoFrame& output);

    // Helper to allocate intermediate frames
    VideoFrame createIntermediateFrame(uint32_t width, uint32_t height,
                                      PixelFormat format = PixelFormat::RGB_8BIT);
    void freeIntermediateFrame(VideoFrame& frame);

    // Configuration
    ProcessingConfig m_config;

    // Vulkan context (shared by all processors)
    std::unique_ptr<VulkanContext> m_vulkan_context;

    // Processing modules
    std::unique_ptr<BlackBarDetector> m_black_bar_detector;
    std::unique_ptr<NLSShader> m_nls_shader;
    std::unique_ptr<PlaceboRenderer> m_tone_mapper;

    // OSD system
    std::unique_ptr<osd::OSDRenderer> m_osd_renderer;
    std::unique_ptr<osd::OSDCompositor> m_osd_compositor;
    std::unique_ptr<osd::MenuSystem> m_menu_system;
    std::unique_ptr<input::IRRemote> m_ir_remote;

    // Intermediate frames
    VideoFrame m_cropped_frame;
    VideoFrame m_warped_frame;
    VideoFrame m_tone_mapped_frame;

    // Statistics
    Stats m_stats;

    // Frame monitoring callback
    FrameCallback m_frame_callback;

    // State
    bool m_initialized = false;
};

} // namespace processing
} // namespace ares
