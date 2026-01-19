#include "processing_pipeline.h"
#include "core/logger.h"
#include <chrono>
#include <cstring>

namespace ares {
namespace processing {

ProcessingPipeline::ProcessingPipeline() {
    LOG_INFO("Processing", "ProcessingPipeline created");
}

ProcessingPipeline::~ProcessingPipeline() {
    shutdown();
    LOG_INFO("Processing", "ProcessingPipeline destroyed");
}

Result ProcessingPipeline::initialize(const ProcessingConfig& config) {
    if (m_initialized) {
        LOG_WARN("Processing", "ProcessingPipeline already initialized");
        return Result::SUCCESS;
    }

    LOG_INFO("Processing", "Initializing processing pipeline");

    m_config = config;

    // Initialize Vulkan context
    Result result = initializeVulkan();
    if (result != Result::SUCCESS) {
        LOG_ERROR("Processing", "Failed to initialize Vulkan context");
        return result;
    }

    // Initialize processors
    result = initializeProcessors();
    if (result != Result::SUCCESS) {
        LOG_ERROR("Processing", "Failed to initialize processors");
        return result;
    }

    m_initialized = true;
    LOG_INFO("Processing", "Processing pipeline initialized successfully");

    return Result::SUCCESS;
}

Result ProcessingPipeline::initializeVulkan() {
    m_vulkan_context = std::make_unique<VulkanContext>();

    // Enable validation in debug builds
    #ifdef NDEBUG
    bool enable_validation = false;
    #else
    bool enable_validation = true;
    #endif

    Result result = m_vulkan_context->initialize(enable_validation);
    if (result != Result::SUCCESS) {
        LOG_ERROR("Processing", "Failed to initialize Vulkan");
        return result;
    }

    // Log GPU info
    auto stats = m_vulkan_context->getStats();
    LOG_INFO("Processing", "Using GPU: %s", stats.device_name.c_str());
    LOG_INFO("Processing", "Driver: %s", stats.driver_version.c_str());
    LOG_INFO("Processing", "VRAM: %lu MB total, %lu MB available",
             stats.total_memory_mb, stats.available_memory_mb);

    return Result::SUCCESS;
}

Result ProcessingPipeline::initializeProcessors() {
    // Initialize black bar detector
    m_black_bar_detector = std::make_unique<BlackBarDetector>();
    LOG_INFO("Processing", "Black bar detector initialized");

    // Initialize tone mapper FIRST (provides libplacebo GPU for other processors)
    m_tone_mapper = std::make_unique<PlaceboRenderer>();
    Result result = m_tone_mapper->initialize(m_vulkan_context.get());
    if (result != Result::SUCCESS) {
        LOG_ERROR("Processing", "Failed to initialize tone mapper");
        return result;
    }

    // Initialize NLS shader (uses libplacebo GPU from tone mapper)
    if (m_config.nls.enabled) {
        m_nls_shader = std::make_unique<NLSShader>();
        result = m_nls_shader->initialize(m_vulkan_context.get(), m_tone_mapper->getGPU());
        if (result != Result::SUCCESS) {
            LOG_ERROR("Processing", "Failed to initialize NLS shader");
            return result;
        }
        LOG_INFO("Processing", "NLS shader initialized with libplacebo (aspect ratio warping)");
    }

    const char* algo_name = "BT.2390";
    switch (m_config.tone_mapping.algorithm) {
        case ToneMappingAlgorithm::REINHARD: algo_name = "Reinhard"; break;
        case ToneMappingAlgorithm::HABLE: algo_name = "Hable"; break;
        case ToneMappingAlgorithm::MOBIUS: algo_name = "Mobius"; break;
        case ToneMappingAlgorithm::CLIP: algo_name = "Clip"; break;
        default: break;
    }

    LOG_INFO("Processing", "Tone mapper initialized (algorithm: %s, target: %.0f nits)",
             algo_name, m_config.tone_mapping.target_nits);

    // Initialize OSD system
    // TODO: Get OSD config from main config (for now use defaults)
    OSDConfig osd_config;

    // Initialize IR remote
    m_ir_remote = std::make_unique<input::IRRemote>();
    result = m_ir_remote->initialize();
    if (result != Result::SUCCESS) {
        LOG_WARN("Processing", "Failed to initialize IR remote (OSD will not work)");
        // Non-fatal error, continue without OSD
    } else {
        LOG_INFO("Processing", "IR remote initialized");

        // Initialize OSD renderer
        m_osd_renderer = std::make_unique<osd::OSDRenderer>();
        result = m_osd_renderer->initialize(m_config.output_width,
                                           m_config.output_height,
                                           osd_config);
        if (result != Result::SUCCESS) {
            LOG_WARN("Processing", "Failed to initialize OSD renderer");
            m_ir_remote.reset();
        } else {
            LOG_INFO("Processing", "OSD renderer initialized");

            // Initialize OSD compositor
            m_osd_compositor = std::make_unique<osd::OSDCompositor>();
            result = m_osd_compositor->initialize(m_vulkan_context->getDevice(),
                                                 m_vulkan_context->getPhysicalDevice());
            if (result != Result::SUCCESS) {
                LOG_WARN("Processing", "Failed to initialize OSD compositor");
                m_osd_renderer.reset();
                m_ir_remote.reset();
            } else {
                // Initialize menu system
                m_menu_system = std::make_unique<osd::MenuSystem>();
                result = m_menu_system->initialize(m_osd_renderer.get(),
                                                  m_ir_remote.get(),
                                                  osd_config);
                if (result != Result::SUCCESS) {
                    LOG_WARN("Processing", "Failed to initialize menu system");
                    m_osd_compositor.reset();
                    m_osd_renderer.reset();
                    m_ir_remote.reset();
                } else {
                    LOG_INFO("Processing", "Menu system initialized");

                    // Load default menu structure
                    OSDMenuStructure menu = createDefaultOSDMenu();
                    m_menu_system->loadMenu(menu);
                }
            }
        }
    }

    return Result::SUCCESS;
}

void ProcessingPipeline::shutdown() {
    if (!m_initialized) {
        return;
    }

    LOG_INFO("Processing", "Shutting down processing pipeline");

    // Free intermediate frames
    freeIntermediateFrame(m_cropped_frame);
    freeIntermediateFrame(m_warped_frame);
    freeIntermediateFrame(m_tone_mapped_frame);

    // Destroy OSD system
    m_menu_system.reset();
    m_osd_compositor.reset();
    m_osd_renderer.reset();
    m_ir_remote.reset();

    // Destroy processors (in reverse order)
    m_tone_mapper.reset();
    m_nls_shader.reset();
    m_black_bar_detector.reset();

    // Destroy Vulkan context
    m_vulkan_context.reset();

    m_initialized = false;
    LOG_INFO("Processing", "Processing pipeline shut down");
}

Result ProcessingPipeline::processFrame(const VideoFrame& input, VideoFrame& output) {
    if (!m_initialized) {
        LOG_ERROR("Processing", "Pipeline not initialized");
        return Result::ERROR_NOT_INITIALIZED;
    }

    auto start_time = std::chrono::high_resolution_clock::now();

    // Stage 0: Auto-detect SDR vs HDR content and adjust color space
    bool is_hdr = (input.hdr_metadata.type != HDRType::NONE);

    // Automatically adjust color space based on content
    static bool last_was_hdr = false;
    if (is_hdr != last_was_hdr) {
        if (is_hdr) {
            // HDR content: use BT.2020 and apply tone mapping
            LOG_INFO("Processing", "Detected HDR content (type=%d), using BT.2020 + tone mapping",
                    static_cast<int>(input.hdr_metadata.type));
            m_config.color.input_gamut = ColorGamut::BT2020;
        } else {
            // SDR content: use BT.709, skip tone mapping
            LOG_INFO("Processing", "Detected SDR content, using BT.709 (no tone mapping)");
            m_config.color.input_gamut = ColorGamut::BT709;
        }
        last_was_hdr = is_hdr;
    }

    // Stage 1: Detect black bars
    Result result = detectBlackBars(input);
    if (result != Result::SUCCESS) {
        LOG_ERROR("Processing", "Black bar detection failed");
        return result;
    }

    // Stage 2: Apply crop (if needed)
    const VideoFrame* current_frame = &input;
    if (m_config.black_bars.enabled && m_config.black_bars.auto_crop) {
        CropRegion crop = m_black_bar_detector->getCropRegion();

        if (crop.top > 0 || crop.bottom > 0 || crop.left > 0 || crop.right > 0) {
            result = applyCrop(input, m_cropped_frame);
            if (result != Result::SUCCESS) {
                LOG_ERROR("Processing", "Crop failed");
                return result;
            }

            current_frame = &m_cropped_frame;
            m_stats.after_crop_width = m_cropped_frame.width;
            m_stats.after_crop_height = m_cropped_frame.height;

            if (m_frame_callback) {
                m_frame_callback(m_cropped_frame, "after_crop");
            }
        } else {
            m_stats.after_crop_width = input.width;
            m_stats.after_crop_height = input.height;
        }
    } else {
        m_stats.after_crop_width = input.width;
        m_stats.after_crop_height = input.height;
    }

    // Stage 3: Apply NLS warping (if enabled)
    if (m_config.nls.enabled && m_nls_shader) {
        result = applyNLS(*current_frame, m_warped_frame);
        if (result != Result::SUCCESS) {
            LOG_ERROR("Processing", "NLS warping failed");
            return result;
        }

        current_frame = &m_warped_frame;
        m_stats.after_nls_width = m_warped_frame.width;
        m_stats.after_nls_height = m_warped_frame.height;

        if (m_frame_callback) {
            m_frame_callback(m_warped_frame, "after_nls");
        }
    } else {
        m_stats.after_nls_width = current_frame->width;
        m_stats.after_nls_height = current_frame->height;
    }

    // Stage 4: Apply tone mapping (only for HDR content)
    VideoFrame tone_mapped_output;
    if (is_hdr) {
        // HDR content: apply tone mapping
        result = applyToneMapping(*current_frame, tone_mapped_output);
        if (result != Result::SUCCESS) {
            LOG_ERROR("Processing", "Tone mapping failed");
            return result;
        }
    } else {
        // SDR content: skip tone mapping, pass through
        tone_mapped_output = *current_frame;
    }

    // Stage 5: OSD compositing (if OSD is active)
    if (m_osd_renderer && m_osd_compositor && m_menu_system) {
        // Poll IR remote for input
        if (m_ir_remote) {
            m_ir_remote->pollEvents();
        }

        // Update menu system (handles animations, timeouts)
        if (m_menu_system->isVisible()) {
            m_menu_system->update(m_stats.total_frame_time_ms);
            m_menu_system->render();

            // Composite OSD over video
            result = compositeOSD(tone_mapped_output, output);
            if (result != Result::SUCCESS) {
                LOG_WARN("Processing", "OSD compositing failed, using frame without OSD");
                output = tone_mapped_output;
            }
        } else {
            // No OSD, use tone-mapped output directly
            output = tone_mapped_output;
        }
    } else {
        // No OSD system, use tone-mapped output directly
        output = tone_mapped_output;
    }

    m_stats.output_width = output.width;
    m_stats.output_height = output.height;

    if (m_frame_callback) {
        m_frame_callback(output, "final_output");
    }

    // Update statistics
    auto end_time = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration<double, std::milli>(end_time - start_time);

    m_stats.total_frame_time_ms = elapsed.count();
    m_stats.frames_processed++;

    if (m_stats.frames_processed == 1) {
        m_stats.avg_frame_time_ms = m_stats.total_frame_time_ms;
    } else {
        m_stats.avg_frame_time_ms =
            (m_stats.avg_frame_time_ms * (m_stats.frames_processed - 1) +
             m_stats.total_frame_time_ms) / m_stats.frames_processed;
    }

    m_stats.input_width = input.width;
    m_stats.input_height = input.height;

    return Result::SUCCESS;
}

Result ProcessingPipeline::detectBlackBars(const VideoFrame& frame) {
    if (!m_config.black_bars.enabled) {
        return Result::SUCCESS;
    }

    m_black_bar_detector->analyzeFrame(frame, m_config.black_bars);

    m_stats.current_crop = m_black_bar_detector->getCropRegion();
    m_stats.crop_stable = m_black_bar_detector->isStable();

    return Result::SUCCESS;
}

Result ProcessingPipeline::applyCrop(const VideoFrame& input, VideoFrame& output) {
    CropRegion crop = m_black_bar_detector->getCropRegion();

    // Calculate cropped dimensions
    uint32_t cropped_width = input.width - crop.left - crop.right;
    uint32_t cropped_height = input.height - crop.top - crop.bottom;

    // Allocate output if needed
    if (output.data == nullptr ||
        output.width != cropped_width ||
        output.height != cropped_height) {

        freeIntermediateFrame(output);
        output = createIntermediateFrame(cropped_width, cropped_height, input.format);
    }

    // Simple crop (copy relevant region)
    // Note: This is a simplified implementation for planar YUV
    // Production code would handle various pixel formats properly

    const uint8_t* src = input.data;
    uint8_t* dst = output.data;

    // For now, just copy line by line (simplified for Y plane)
    for (uint32_t y = crop.top; y < input.height - crop.bottom; y++) {
        const uint8_t* src_line = src + y * input.width + crop.left;
        uint8_t* dst_line = dst + (y - crop.top) * cropped_width;
        std::memcpy(dst_line, src_line, cropped_width);
    }

    // Copy metadata
    output.pts = input.pts;
    output.hdr_metadata = input.hdr_metadata;

    return Result::SUCCESS;
}

Result ProcessingPipeline::applyNLS(const VideoFrame& input, VideoFrame& output) {
    return m_nls_shader->processFrame(input, output, m_config.nls);
}

Result ProcessingPipeline::applyToneMapping(const VideoFrame& input, VideoFrame& output) {
    return m_tone_mapper->processFrame(input, output, m_config);
}

Result ProcessingPipeline::compositeOSD(const VideoFrame& input, VideoFrame& output) {
    if (!m_osd_renderer || !m_osd_compositor) {
        return Result::ERROR_NOT_INITIALIZED;
    }

    // Get OSD surface data
    const uint8_t* osd_data = m_osd_renderer->getSurfaceData();
    if (!osd_data) {
        return Result::ERROR_INVALID_PARAMETER;
    }

    uint32_t osd_width = m_osd_renderer->getWidth();
    uint32_t osd_height = m_osd_renderer->getHeight();

    // Get OSD config for positioning
    OSDConfig osd_config = m_osd_renderer->getConfig();

    // Composite OSD over video
    return m_osd_compositor->composite(input, osd_data, osd_width, osd_height,
                                      output, osd_config);
}

VideoFrame ProcessingPipeline::createIntermediateFrame(uint32_t width, uint32_t height,
                                                       PixelFormat format) {
    VideoFrame frame;
    frame.width = width;
    frame.height = height;
    frame.format = format;

    // Calculate buffer size based on format
    size_t bytes_per_pixel = 3;  // RGB 8-bit
    if (format == PixelFormat::YUV420P_10BIT) {
        bytes_per_pixel = 2;  // Simplified
    }

    frame.size = width * height * bytes_per_pixel;
    frame.data = new uint8_t[frame.size];

    return frame;
}

void ProcessingPipeline::freeIntermediateFrame(VideoFrame& frame) {
    if (frame.data) {
        delete[] frame.data;
        frame.data = nullptr;
        frame.size = 0;
    }
}

void ProcessingPipeline::updateConfig(const ProcessingConfig& config) {
    m_config = config;

    // Update individual processors
    if (m_nls_shader) {
        m_nls_shader->updateConfig(config.nls);
    }

    if (m_tone_mapper) {
        m_tone_mapper->updateConfig(config);
    }

    LOG_INFO("Processing", "Pipeline configuration updated");
}

ProcessingPipeline::Stats ProcessingPipeline::getStats() const {
    Stats stats = m_stats;

    // Collect stats from individual modules
    if (m_black_bar_detector) {
        stats.black_bar_stats = m_black_bar_detector->getStats();
    }

    if (m_nls_shader && m_nls_shader->isInitialized()) {
        stats.nls_stats = m_nls_shader->getStats();
    }

    if (m_tone_mapper) {
        stats.tone_mapping_stats = m_tone_mapper->getStats();
    }

    return stats;
}

} // namespace processing
} // namespace ares
