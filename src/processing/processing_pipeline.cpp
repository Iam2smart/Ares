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

    // Initialize NLS shader
    if (m_config.nls.enabled) {
        m_nls_shader = std::make_unique<NLSShader>();
        Result result = m_nls_shader->initialize(m_vulkan_context.get());
        if (result != Result::SUCCESS) {
            LOG_ERROR("Processing", "Failed to initialize NLS shader");
            return result;
        }
        LOG_INFO("Processing", "NLS shader initialized (aspect ratio warping)");
    }

    // Initialize tone mapper
    m_tone_mapper = std::make_unique<PlaceboRenderer>();
    Result result = m_tone_mapper->initialize(m_vulkan_context.get());
    if (result != Result::SUCCESS) {
        LOG_ERROR("Processing", "Failed to initialize tone mapper");
        return result;
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

    // Stage 4: Apply tone mapping
    result = applyToneMapping(*current_frame, output);
    if (result != Result::SUCCESS) {
        LOG_ERROR("Processing", "Tone mapping failed");
        return result;
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
