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
    m_log = pl_log_create(PL_API_VER, &(struct pl_log_params) {
        .log_cb = pl_log_callback,
        .log_level = PL_LOG_INFO,
    });

    if (!m_log) {
        LOG_ERROR("Processing", "Failed to create libplacebo log");
        return Result::ERROR_GENERIC;
    }

    // Import Vulkan instance
    struct pl_vulkan_params vk_params = {
        .instance = vk_context->getInstance(),
        .device = vk_context->getDevice(),
        .phys_device = vk_context->getPhysicalDevice(),
        .get_proc_addr = vkGetInstanceProcAddr,
        .queue_graphics = {
            .index = vk_context->getGraphicsQueueFamily(),
            .count = 1,
        },
        .queue_compute = {
            .index = vk_context->getComputeQueueFamily(),
            .count = 1,
        },
        .queue_transfer = {
            .index = vk_context->getTransferQueueFamily(),
            .count = 1,
        },
    };

    m_vk = pl_vulkan_import(m_log, &vk_params);
    if (!m_vk) {
        LOG_ERROR("Processing", "Failed to import Vulkan into libplacebo");
        return Result::ERROR_GENERIC;
    }

    m_gpu = m_vk->gpu;

    LOG_INFO("Processing", "libplacebo Vulkan integration successful");
    LOG_INFO("Processing", "GPU: %s", m_gpu->glsl.description);

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
        m_input_tex = pl_tex_create(m_gpu, &(struct pl_tex_params) {
            .w = width,
            .h = height,
            .format = pl_find_named_fmt(m_gpu, "yuv420p10le"),
            .sampleable = true,
            .host_writable = true,
        });

        if (!m_input_tex) {
            LOG_ERROR("Processing", "Failed to create input texture");
            return Result::ERROR_GENERIC;
        }
    }

    // Create output texture (8-bit RGB)
    if (!m_output_tex) {
        m_output_tex = pl_tex_create(m_gpu, &(struct pl_tex_params) {
            .w = width,
            .h = height,
            .format = pl_find_fmt(m_gpu, PL_FMT_UNORM, 3, 0, 8,
                                  PL_FMT_CAP_RENDERABLE | PL_FMT_CAP_HOST_READABLE),
            .renderable = true,
            .host_readable = true,
        });

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
    if (!pl_tex_upload(m_gpu, &(struct pl_tex_transfer_params) {
        .tex = m_input_tex,
        .ptr = frame.data,
        .stride_w = frame.width,
        .stride_h = frame.height,
    })) {
        LOG_ERROR("Processing", "Failed to upload frame to GPU");
        return Result::ERROR_GENERIC;
    }

    return Result::SUCCESS;
}

pl_tone_map_function PlaceboRenderer::getToneMappingFunction(ToneMappingAlgorithm algo) {
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
    pl_frame_from_swapchain(&source, &(struct pl_swapchain_frame) {
        .fbo = (struct pl_tex*) m_input_tex,
    });

    // Set source colorspace (BT.2020 for HDR)
    source.repr.sys = PL_COLOR_SYSTEM_BT_2020_NC;
    source.repr.levels = PL_COLOR_LEVELS_FULL;
    source.color = (struct pl_color_space) {
        .primaries = PL_COLOR_PRIM_BT_2020,
        .transfer = PL_COLOR_TRC_PQ,  // HDR10 uses PQ
        .hdr.max_luma = source_nits,  // Use dynamic source_nits
        .hdr.min_luma = 0.0f,
    };

    // Setup target image
    struct pl_frame target = {0};
    pl_frame_from_swapchain(&target, &(struct pl_swapchain_frame) {
        .fbo = (struct pl_tex*) m_output_tex,
    });

    // Set target colorspace (BT.709 for display)
    target.repr.sys = PL_COLOR_SYSTEM_BT_709;
    target.repr.levels = PL_COLOR_LEVELS_FULL;
    target.color = (struct pl_color_space) {
        .primaries = PL_COLOR_PRIM_BT_709,
        .transfer = PL_COLOR_TRC_GAMMA22,
        .hdr.max_luma = config.tone_mapping.target_nits,
        .hdr.min_luma = 0.0f,
    };

    // Setup render parameters
    struct pl_render_params render_params = pl_render_default_params;

    // Configure tone mapping
    render_params.color_map_params = &(struct pl_color_map_params) {
        .tone_mapping_function = getToneMappingFunction(config.tone_mapping.algorithm),
        .tone_mapping_param = knee_point,  // Use dynamic knee_point
        .tone_mapping_mode = PL_TONE_MAP_AUTO,
        .gamut_mapping = PL_GAMUT_CLIP,
        .gamut_clipping = config.color.soft_clip,
        .contrast = config.tone_mapping.contrast,
        .saturation = config.tone_mapping.saturation,
    };

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
    if (config.dithering.enabled) {
        render_params.dither_params = &(struct pl_dither_params) {
            .method = [&]() {
                switch (config.dithering.method) {
                    case DitheringConfig::Method::ORDERED:
                        return PL_DITHER_ORDERED_LUT;
                    case DitheringConfig::Method::BLUE_NOISE:
                        return PL_DITHER_BLUE_NOISE;
                    case DitheringConfig::Method::WHITE_NOISE:
                        return PL_DITHER_WHITE_NOISE;
                    case DitheringConfig::Method::ERROR_DIFFUSION:
                        return PL_DITHER_ERROR_DIFFUSION;
                    default:
                        return PL_DITHER_BLUE_NOISE;  // Best quality default
                }
            }(),
            .lut_size = config.dithering.lut_size,
            .temporal = config.dithering.temporal,
        };
    }

    // Configure debanding
    if (config.debanding.enabled) {
        render_params.deband_params = &(struct pl_deband_params) {
            .iterations = config.debanding.iterations,
            .threshold = config.debanding.threshold,
            .radius = (float)config.debanding.radius,
            .grain = config.debanding.grain,
        };
    }

    // Configure chroma upsampling
    if (config.chroma_upscaling.enabled) {
        // Map our ScalingAlgorithm to libplacebo filter
        const struct pl_filter_preset* chroma_filter = nullptr;
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
        const struct pl_filter_preset* luma_filter = nullptr;
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
    if (!pl_tex_download(m_gpu, &(struct pl_tex_transfer_params) {
        .tex = m_output_tex,
        .ptr = output.data,
        .stride_w = m_width * 3,
        .stride_h = m_height,
    })) {
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
