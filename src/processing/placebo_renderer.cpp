#include "placebo_renderer.h"
#include "scene_analyzer.h"
#include "core/logger.h"

#include <libplacebo/colorspace.h>
#include <libplacebo/filters.h>
#include <cstring>

namespace ares {
namespace processing {

// libplacebo log callback
static void pl_log_callback(void* priv, pl_log_level level, const char* msg) {
    switch (level) {
        case PL_LOG_FATAL:
        case PL_LOG_ERR:
            LOG_ERROR("libplacebo", "%s", msg);
            break;
        case PL_LOG_WARN:
            LOG_WARN("libplacebo", "%s", msg);
            break;
        case PL_LOG_INFO:
            LOG_INFO("libplacebo", "%s", msg);
            break;
        case PL_LOG_DEBUG:
        case PL_LOG_TRACE:
            LOG_DEBUG("libplacebo", "%s", msg);
            break;
        default:
            break;
    }
}

PlaceboRenderer::PlaceboRenderer() {
    LOG_INFO("Processing", "PlaceboRenderer created");
    m_last_frame_time = std::chrono::steady_clock::now();
    m_scene_analyzer = std::make_unique<SceneAnalyzer>();
}

PlaceboRenderer::~PlaceboRenderer() {
    // Destroy textures
    if (m_input_tex) {
        pl_tex_destroy(m_gpu, &m_input_tex);
    }
    if (m_output_tex) {
        pl_tex_destroy(m_gpu, &m_output_tex);
    }

    // Destroy renderer
    if (m_renderer) {
        pl_renderer_destroy(&m_renderer);
    }

    // Destroy Vulkan
    if (m_vk) {
        pl_vulkan_destroy(&m_vk);
    }

    // Destroy log
    if (m_log) {
        pl_log_destroy(&m_log);
    }

    LOG_INFO("Processing", "PlaceboRenderer destroyed");
}

Result PlaceboRenderer::initialize(VulkanContext* vk_context) {
    if (m_initialized) {
        LOG_WARN("Processing", "PlaceboRenderer already initialized");
        return Result::SUCCESS;
    }

    if (!vk_context || !vk_context->isInitialized()) {
        LOG_ERROR("Processing", "Invalid Vulkan context");
        return Result::ERROR_INVALID_PARAMETER;
    }

    m_vk_context = vk_context;

    LOG_INFO("Processing", "Initializing libplacebo renderer");

    // Initialize libplacebo
    Result result = initializePlacebo(vk_context);
    if (result != Result::SUCCESS) {
        return result;
    }

    // Create renderer
    result = createRenderer();
    if (result != Result::SUCCESS) {
        return result;
    }

    m_initialized = true;
    LOG_INFO("Processing", "libplacebo renderer initialized successfully");

    return Result::SUCCESS;
}

Result PlaceboRenderer::initializePlacebo(VulkanContext* vk_context) {
    // Create libplacebo log
    struct pl_log_params log_params = {};
    log_params.log_cb = pl_log_callback;
    log_params.log_level = PL_LOG_INFO;
    m_log = pl_log_create(PL_API_VER, &log_params);

    if (!m_log) {
        LOG_ERROR("Processing", "Failed to create libplacebo log");
        return Result::ERROR_GENERIC;
    }

    // Import Vulkan instance
    struct pl_vulkan_import_params vk_params = {};
    vk_params.instance = vk_context->getInstance();
    vk_params.device = vk_context->getDevice();
    vk_params.phys_device = vk_context->getPhysicalDevice();
    vk_params.get_proc_addr = vkGetInstanceProcAddr;
    vk_params.queue_graphics.index = vk_context->getGraphicsQueueFamily();
    vk_params.queue_graphics.count = 1;
    vk_params.queue_compute.index = vk_context->getComputeQueueFamily();
    vk_params.queue_compute.count = 1;
    vk_params.queue_transfer.index = vk_context->getTransferQueueFamily();
    vk_params.queue_transfer.count = 1;

    m_vk = pl_vulkan_import(m_log, &vk_params);
    if (!m_vk) {
        LOG_ERROR("Processing", "Failed to import Vulkan into libplacebo");
        return Result::ERROR_GENERIC;
    }

    m_gpu = m_vk->gpu;

    LOG_INFO("Processing", "libplacebo Vulkan integration successful");

    return Result::SUCCESS;
}

Result PlaceboRenderer::createRenderer() {
    // Create renderer with default parameters
    m_renderer = pl_renderer_create(m_log, m_gpu);
    if (!m_renderer) {
        LOG_ERROR("Processing", "Failed to create libplacebo renderer");
        return Result::ERROR_GENERIC;
    }

    return Result::SUCCESS;
}

Result PlaceboRenderer::createTextures(uint32_t width, uint32_t height) {
    // Destroy old textures if dimensions changed
    if (m_width != width || m_height != height) {
        if (m_input_tex) {
            pl_tex_destroy(m_gpu, &m_input_tex);
        }
        if (m_output_tex) {
            pl_tex_destroy(m_gpu, &m_output_tex);
        }

        m_width = width;
        m_height = height;
    }

    // Create input texture (10-bit YUV)
    if (!m_input_tex) {
        struct pl_tex_params input_params = {};
        input_params.w = static_cast<int>(width);
        input_params.h = static_cast<int>(height);
        input_params.format = pl_find_named_fmt(m_gpu, "yuv420p10le");
        input_params.sampleable = true;
        input_params.host_writable = true;
        m_input_tex = pl_tex_create(m_gpu, &input_params);

        if (!m_input_tex) {
            LOG_ERROR("Processing", "Failed to create input texture");
            return Result::ERROR_GENERIC;
        }
    }

    // Create output texture (8-bit RGB)
    if (!m_output_tex) {
        struct pl_tex_params output_params = {};
        output_params.w = static_cast<int>(width);
        output_params.h = static_cast<int>(height);
        output_params.format = pl_find_fmt(m_gpu, PL_FMT_UNORM, 3, 0, 8,
                                  static_cast<pl_fmt_caps>(PL_FMT_CAP_RENDERABLE | PL_FMT_CAP_HOST_READABLE));
        output_params.renderable = true;
        output_params.host_readable = true;
        m_output_tex = pl_tex_create(m_gpu, &output_params);

        if (!m_output_tex) {
            LOG_ERROR("Processing", "Failed to create output texture");
            return Result::ERROR_GENERIC;
        }
    }

    return Result::SUCCESS;
}

Result PlaceboRenderer::uploadFrame(const VideoFrame& frame) {
    // Create textures if needed
    Result result = createTextures(frame.width, frame.height);
    if (result != Result::SUCCESS) {
        return result;
    }

    // Upload frame data to GPU
    struct pl_tex_transfer_params upload_params = {};
    upload_params.tex = m_input_tex;
    upload_params.ptr = frame.data;
    upload_params.row_pitch = frame.width * 3; // Assuming RGB 8-bit format
    if (!pl_tex_upload(m_gpu, &upload_params)) {
        LOG_ERROR("Processing", "Failed to upload frame to GPU");
        return Result::ERROR_GENERIC;
    }

    return Result::SUCCESS;
}

const pl_tone_map_function* PlaceboRenderer::getToneMappingFunction(ToneMappingAlgorithm algo) {
    switch (algo) {
        case ToneMappingAlgorithm::BT2390:
            return &pl_tone_map_bt2390;
        case ToneMappingAlgorithm::REINHARD:
            return &pl_tone_map_reinhard;
        case ToneMappingAlgorithm::HABLE:
            return &pl_tone_map_hable;
        case ToneMappingAlgorithm::MOBIUS:
            return &pl_tone_map_mobius;
        case ToneMappingAlgorithm::CLIP:
            return &pl_tone_map_clip;
        default:
            return &pl_tone_map_bt2390;
    }
}

Result PlaceboRenderer::render(const ProcessingConfig& config) {
    auto start_time = std::chrono::high_resolution_clock::now();

    // Get dynamic tone mapping parameters if enabled
    float source_nits = config.tone_mapping.source_nits;
    float knee_point = config.tone_mapping.params.knee_point;

    if (config.tone_mapping.dynamic.enabled && m_scene_analyzer) {
        SceneAnalyzer::DynamicParams dynamic_params = m_scene_analyzer->getDynamicParams();
        source_nits = dynamic_params.source_nits;
        knee_point = dynamic_params.knee_point;

        LOG_DEBUG("Processing", "Dynamic tone mapping: source=%.1f nits (avg=%.1f, peak=%.1f), knee=%.3f",
                  source_nits, dynamic_params.avg_brightness,
                  dynamic_params.peak_brightness, knee_point);
    }

    // Setup source image
    struct pl_frame source = {0};
    struct pl_swapchain_frame source_swapchain = {};
    source_swapchain.fbo = m_input_tex;
    pl_frame_from_swapchain(&source, &source_swapchain);

    // Set source colorspace (BT.2020 for HDR)
    source.repr.sys = PL_COLOR_SYSTEM_BT_2020_NC;
    source.repr.levels = PL_COLOR_LEVELS_FULL;
    source.color.primaries = PL_COLOR_PRIM_BT_2020;
    source.color.transfer = PL_COLOR_TRC_PQ;  // HDR10 uses PQ
    source.color.hdr.max_luma = source_nits;  // Use dynamic source_nits
    source.color.hdr.min_luma = 0.0f;

    // Setup target image
    struct pl_frame target = {0};
    struct pl_swapchain_frame target_swapchain = {};
    target_swapchain.fbo = m_output_tex;
    pl_frame_from_swapchain(&target, &target_swapchain);

    // Set target colorspace (BT.709 for display)
    target.repr.sys = PL_COLOR_SYSTEM_BT_709;
    target.repr.levels = PL_COLOR_LEVELS_FULL;
    target.color.primaries = PL_COLOR_PRIM_BT_709;
    target.color.transfer = PL_COLOR_TRC_GAMMA22;
    target.color.hdr.max_luma = config.tone_mapping.target_nits;
    target.color.hdr.min_luma = 0.0f;

    // Setup render parameters
    struct pl_render_params render_params = pl_render_default_params;

    // Configure tone mapping
    static struct pl_color_map_params color_map_params = pl_color_map_default_params;
    color_map_params.tone_mapping_function = getToneMappingFunction(config.tone_mapping.algorithm);
    // Note: Some parameters removed in newer libplacebo API
    (void)knee_point; // Unused with newer API
    (void)config.color.soft_clip; // gamut_clipping removed
    (void)config.tone_mapping.contrast; // contrast removed
    (void)config.tone_mapping.saturation; // saturation removed
    render_params.color_map_params = &color_map_params;

    // Configure quality
    switch (config.quality) {
        case ProcessingConfig::Quality::FAST:
            render_params.skip_anti_aliasing = true;
            render_params.disable_linear_scaling = true;
            break;
        case ProcessingConfig::Quality::BALANCED:
            // Use defaults
            break;
        case ProcessingConfig::Quality::HIGH_QUALITY:
            render_params.upscaler = &pl_filter_lanczos;
            render_params.downscaler = &pl_filter_lanczos;
            break;
    }

    // Configure dithering
    static struct pl_dither_params dither_params = {};
    if (config.dithering.enabled) {
        switch (config.dithering.method) {
            case DitheringConfig::Method::ORDERED:
                dither_params.method = PL_DITHER_ORDERED_LUT;
                break;
            case DitheringConfig::Method::BLUE_NOISE:
                dither_params.method = PL_DITHER_BLUE_NOISE;
                break;
            case DitheringConfig::Method::WHITE_NOISE:
                dither_params.method = PL_DITHER_WHITE_NOISE;
                break;
            case DitheringConfig::Method::ERROR_DIFFUSION:
                dither_params.method = PL_DITHER_BLUE_NOISE; // Error diffusion removed in newer API
                break;
            default:
                dither_params.method = PL_DITHER_BLUE_NOISE;  // Best quality default
                break;
        }
        dither_params.lut_size = config.dithering.lut_size;
        dither_params.temporal = config.dithering.temporal;
        render_params.dither_params = &dither_params;
    }

    // Configure debanding
    static struct pl_deband_params deband_params = {};
    if (config.debanding.enabled) {
        deband_params.iterations = config.debanding.iterations;
        deband_params.threshold = config.debanding.threshold;
        deband_params.radius = (float)config.debanding.radius;
        deband_params.grain = config.debanding.grain;
        render_params.deband_params = &deband_params;
    }

    // Configure chroma upsampling
    if (config.chroma_upscaling.enabled) {
        // Map our ScalingAlgorithm to libplacebo filter
        const struct pl_filter_config* chroma_filter = nullptr;
        switch (config.chroma_upscaling.algorithm) {
            case ScalingAlgorithm::BILINEAR:
                chroma_filter = &pl_filter_bilinear;
                break;
            case ScalingAlgorithm::BICUBIC:
                chroma_filter = &pl_filter_bicubic;
                break;
            case ScalingAlgorithm::LANCZOS:
            case ScalingAlgorithm::EWA_LANCZOS:
                chroma_filter = &pl_filter_lanczos;
                break;
            case ScalingAlgorithm::SPLINE16:
                chroma_filter = &pl_filter_spline16;
                break;
            case ScalingAlgorithm::SPLINE36:
                chroma_filter = &pl_filter_spline36;
                break;
            case ScalingAlgorithm::SPLINE64:
                chroma_filter = &pl_filter_spline64;
                break;
            default:
                chroma_filter = &pl_filter_lanczos;  // High quality default
                break;
        }

        // Set chroma upscaling filter
        source.repr.sys = PL_COLOR_SYSTEM_BT_2020_C;  // Enable chroma processing
        render_params.plane_upscaler = chroma_filter;

        // Configure anti-ringing
        if (config.chroma_upscaling.antiring > 0.0f) {
            render_params.antiringing_strength = config.chroma_upscaling.antiring;
        }
    }

    // Configure image upscaling algorithms
    if (config.image_upscaling.luma_algorithm != ScalingAlgorithm::BILINEAR) {
        const struct pl_filter_config* luma_filter = nullptr;
        switch (config.image_upscaling.luma_algorithm) {
            case ScalingAlgorithm::LANCZOS:
                luma_filter = &pl_filter_lanczos;
                break;
            case ScalingAlgorithm::SPLINE36:
                luma_filter = &pl_filter_spline36;
                break;
            case ScalingAlgorithm::EWA_LANCZOS:
                luma_filter = &pl_filter_ewa_lanczos;
                break;
            case ScalingAlgorithm::EWA_LANCZOS_SHARP:
                luma_filter = &pl_filter_ewa_lanczossharp;
                break;
            default:
                luma_filter = &pl_filter_lanczos;
                break;
        }
        render_params.upscaler = luma_filter;
    }

    // Render
    if (!pl_render_image(m_renderer, &source, &target, &render_params)) {
        LOG_ERROR("Processing", "Failed to render frame");
        return Result::ERROR_GENERIC;
    }

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

Result PlaceboRenderer::downloadFrame(VideoFrame& output) {
    // Allocate output buffer
    size_t output_size = m_width * m_height * 3; // RGB 8-bit
    output.data = new uint8_t[output_size];
    output.size = output_size;
    output.width = m_width;
    output.height = m_height;
    output.format = PixelFormat::RGB_8BIT;

    // Download from GPU
    struct pl_tex_transfer_params download_params = {};
    download_params.tex = m_output_tex;
    download_params.ptr = output.data;
    download_params.row_pitch = m_width * 3;
    if (!pl_tex_download(m_gpu, &download_params)) {
        LOG_ERROR("Processing", "Failed to download frame from GPU");
        delete[] output.data;
        output.data = nullptr;
        return Result::ERROR_GENERIC;
    }

    return Result::SUCCESS;
}

Result PlaceboRenderer::processFrame(const VideoFrame& input, VideoFrame& output,
                                     const ProcessingConfig& config) {
    if (!m_initialized) {
        LOG_ERROR("Processing", "Renderer not initialized");
        return Result::ERROR_NOT_INITIALIZED;
    }

    // Initialize scene analyzer if dynamic tone mapping is enabled
    if (config.tone_mapping.dynamic.enabled && m_scene_analyzer) {
        static bool analyzer_initialized = false;
        if (!analyzer_initialized) {
            m_scene_analyzer->initialize(config.tone_mapping.dynamic);
            analyzer_initialized = true;
            LOG_INFO("Processing", "Dynamic tone mapping enabled");
        }

        // Analyze frame for brightness statistics
        m_scene_analyzer->analyzeFrame(input, input.hdr_metadata);
    }

    // Upload frame
    Result result = uploadFrame(input);
    if (result != Result::SUCCESS) {
        return result;
    }

    // Render with tone mapping (uses dynamic parameters if enabled)
    result = render(config);
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

    return Result::SUCCESS;
}

void PlaceboRenderer::updateConfig(const ProcessingConfig& config) {
    // Configuration is applied per-frame in render()
    // This is a no-op for now, but could cache config for optimization
}

PlaceboRenderer::Stats PlaceboRenderer::getStats() const {
    return m_stats;
}

} // namespace processing
} // namespace ares
