#pragma once

#include <ares/types.h>
#include <string>

namespace ares {

// Tone mapping algorithm selection
enum class ToneMappingAlgorithm {
    BT2390,      // ITU-R BT.2390 EETF (most accurate)
    REINHARD,    // Simple Reinhard
    HABLE,       // Hable (Uncharted 2) filmic
    MOBIUS,      // Mobius (preserves highlights)
    CLIP,        // Simple clipping (no tone mapping)
    CUSTOM       // Custom curve from LUT
};

// Color gamut selection
enum class ColorGamut {
    BT709,       // Rec.709 (sRGB)
    BT2020,      // Rec.2020 (wide gamut)
    DCI_P3,      // DCI-P3 (cinema)
    ADOBE_RGB    // Adobe RGB
};

// Transfer function
enum class TransferFunction {
    GAMMA22,     // Gamma 2.2
    GAMMA28,     // Gamma 2.8
    SRGB,        // sRGB
    LINEAR,      // Linear light
    PQ,          // SMPTE ST 2084 (PQ)
    HLG          // Hybrid Log-Gamma
};

// NLS (Non-Linear Stretch) configuration
// Based on NLS-Next by NotMithical (mpv shader)
// Warps 16:9 content to fit cinemascope (2.35:1 / 2.40:1) screens
// Uses power curves for sophisticated non-linear stretching
struct NLSConfig {
    bool enabled = false;

    // Target aspect ratio
    enum class TargetAspect {
        SCOPE_235,      // 2.35:1 (cinemascope)
        SCOPE_240,      // 2.40:1 (ultra wide)
        SCOPE_255,      // 2.55:1 (ultra panavision)
        CUSTOM          // Custom aspect ratio
    } target_aspect = TargetAspect::SCOPE_235;

    float custom_aspect_ratio = 2.35f;  // Custom aspect ratio (if target_aspect == CUSTOM)

    // Stretch distribution (NLS-Next style)
    // Values are normalized to total 1.0 automatically
    float horizontal_stretch = 0.5f;    // Horizontal stretch amount (0.0-1.0)
    float vertical_stretch = 0.5f;      // Vertical stretch amount (0.0-1.0)

    // Content adjustment (NLS-Next style)
    float crop_amount = 0.0f;           // Crop edges before stretch (0.0-1.0)
                                        // Higher = less content but less distortion
    float bars_amount = 0.0f;           // Add black bars/padding (0.0-1.0)
                                        // Higher = scale down + add bars = less distortion

    // Center protection (NLS-Next power curve)
    float center_protect = 1.0f;        // Power curve for stretching (0.1-6.0)
                                        // Higher = more stretch at edges, less in center
                                        // 1.0 = linear, >1.0 = protect center more

    // Quality
    enum class InterpolationQuality {
        BILINEAR,       // Fast, lower quality
        BICUBIC,        // Good quality
        LANCZOS         // Best quality, slower
    } interpolation = InterpolationQuality::BICUBIC;

    // Advanced options
    float vertical_offset = 0.0f;       // Vertical offset (-0.5 to +0.5)
    bool show_grid = false;             // Preview mode (show grid overlay)
};

// Black bar detection configuration
struct BlackBarConfig {
    bool enabled = true;

    // FFmpeg bootstrap (high-accuracy initial detection)
    bool use_ffmpeg_bootstrap = false;  // Use FFmpeg cropdetect for initial detection (disabled by default for live sources)
    float bootstrap_delay = 4.0f;       // Seconds to wait before bootstrap (skip dark openings)
    float bootstrap_duration = 2.0f;    // Seconds to run FFmpeg detection

    // Detection parameters
    int threshold = 16;                 // Pixel brightness threshold (0-255)
    float min_content_height = 0.5f;    // Minimum content height ratio (0.5 = 50%)
    float min_content_width = 0.5f;     // Minimum content width ratio

    // Detection behavior
    int detection_frames = 10;          // Frames to analyze before decision
    float confidence_threshold = 0.8f;  // Confidence needed to apply crop
    bool symmetric_only = true;         // Only detect symmetric bars

    // Cropping behavior
    bool auto_crop = true;              // Automatically crop detected bars
    bool zoom_to_fit = false;           // Zoom to fill screen (may crop content)
    float crop_smoothing = 0.3f;        // Smooth crop transitions (0.0-1.0)

    // Manual override
    struct ManualCrop {
        bool enabled = false;
        int top = 0;
        int bottom = 0;
        int left = 0;
        int right = 0;
    } manual_crop;
};

// HDR tone mapping configuration
struct ToneMappingConfig {
    ToneMappingAlgorithm algorithm = ToneMappingAlgorithm::BT2390;

    // Target display parameters
    float target_nits = 100.0f;         // Target display peak brightness
    float target_contrast = 1000.0f;    // Target contrast ratio

    // Source parameters
    float source_nits = 1000.0f;        // Source content peak brightness
    bool use_metadata = true;           // Use HDR metadata if available

    // Algorithm-specific parameters
    struct {
        // BT.2390 parameters
        float knee_point = 0.75f;       // Knee point for soft clipping
        float max_boost = 1.2f;         // Maximum brightness boost

        // Reinhard parameters
        float reinhard_peak = 1.0f;

        // Hable parameters
        float shoulder_strength = 0.22f;
        float linear_strength = 0.30f;
        float linear_angle = 0.10f;
        float toe_strength = 0.20f;
        float toe_numerator = 0.01f;
        float toe_denominator = 0.30f;

        // Mobius parameters
        float mobius_transition = 0.3f;
        float mobius_peak = 1.0f;
    } params;

    // Post-processing
    float contrast = 1.0f;              // Contrast adjustment (0.5-2.0)
    float saturation = 1.0f;            // Saturation adjustment (0.5-2.0)
    float brightness = 0.0f;            // Brightness offset (-0.5 to +0.5)
    float gamma = 1.0f;                 // Gamma adjustment (0.5-2.0)

    // Shadow/highlight enhancement
    float shadow_lift = 0.0f;           // Lift shadows (0.0-0.3)
    float highlight_compression = 0.0f; // Compress highlights (0.0-0.3)

    // Custom LUT (if algorithm == CUSTOM)
    std::string lut_path;
};

// Color space conversion configuration
struct ColorConfig {
    ColorGamut input_gamut = ColorGamut::BT2020;
    ColorGamut output_gamut = ColorGamut::BT709;
    TransferFunction output_transfer = TransferFunction::GAMMA22;

    // Gamut mapping
    bool soft_clip = true;              // Soft clip out-of-gamut colors
    float desaturation = 0.0f;          // Desaturate out-of-gamut (0.0-1.0)

    // Color adjustments
    float hue = 0.0f;                   // Hue shift (-180 to +180)
    float temperature = 0.0f;           // Color temperature (-1.0 to +1.0)
    float tint = 0.0f;                  // Tint adjustment (-1.0 to +1.0)
};

// Sharpening configuration
struct SharpeningConfig {
    bool enabled = false;

    float strength = 0.5f;              // Sharpening strength (0.0-1.0)
    float radius = 1.0f;                // Sharpening radius (pixels)
    float threshold = 0.0f;             // Threshold to avoid noise (0.0-0.3)

    bool adaptive = true;               // Adaptive based on content
};

// Deinterlacing configuration
struct DeinterlaceConfig {
    bool enabled = false;
    bool auto_detect = true;            // Auto-detect interlaced content

    enum class Method {
        WEAVE,                          // Simple weave (no deinterlace)
        BOB,                            // Bob (double framerate)
        YADIF,                          // Yet Another DeInterlacing Filter
        NNEDI3                          // Neural network deinterlacer
    } method = Method::YADIF;
};

// Upscaling/Downscaling algorithms (like madVR)
enum class ScalingAlgorithm {
    // Fast algorithms
    BILINEAR,               // Fast, low quality
    NEAREST,                // Nearest neighbor (pixel art)

    // Good quality
    BICUBIC,                // Standard bicubic
    MITCHELL,               // Mitchell-Netravali
    CATMULL_ROM,            // Catmull-Rom (sharper)
    HERMITE,                // Hermite

    // High quality (madVR style)
    LANCZOS,                // Lanczos windowed sinc
    SPLINE16,               // 2-tap Spline
    SPLINE36,               // 3-tap Spline
    SPLINE64,               // 4-tap Spline

    // Best quality (slow)
    EWA_LANCZOS,            // Elliptical Weighted Average Lanczos
    EWA_LANCZOS_SHARP,      // EWA Lanczos Sharpened
    EWA_LANCZOS_4TAP,       // EWA Lanczos 4-tap
    JINC,                   // Jinc (madVR quality)

    // Neural network upscaling
    NNEDI3_16,              // NNEDI3 neurons=16
    NNEDI3_32,              // NNEDI3 neurons=32
    NNEDI3_64,              // NNEDI3 neurons=64
    NNEDI3_128,             // NNEDI3 neurons=128

    // Super resolution
    SUPER_XBIR,             // Super-xBR
    RAVU_LITE,              // RAVU-Lite
    RAVU,                   // RAVU (Rapid and Accurate Video Upscaling)
};

// Chroma upscaling configuration
// Convert YCbCr 4:2:0 or 4:2:2 to 4:4:4 with high-quality interpolation
struct ChromaUpscalingConfig {
    bool enabled = true;

    // Force chroma downsampling before upscaling
    // (undo DeckLink's 4:2:2 → let renderer upscale)
    bool force_420 = false;             // Convert 422 (v210) → 420 (p010)

    // Chroma upscaling algorithm
    ScalingAlgorithm algorithm = ScalingAlgorithm::EWA_LANCZOS;

    // Algorithm parameters
    float antiring = 0.0f;              // Anti-ringing (0.0-1.0)
    float blur = 0.0f;                  // Blur/soften (0.0-1.0)

    // madVR-style supersampling
    bool supersample = false;           // Supersample chroma (slow, high quality)
    float supersample_factor = 2.0f;    // Supersampling factor (1.5-4.0)
};

// Image upscaling configuration
struct ImageUpscalingConfig {
    ScalingAlgorithm luma_algorithm = ScalingAlgorithm::NNEDI3_64;
    ScalingAlgorithm chroma_algorithm = ScalingAlgorithm::EWA_LANCZOS;

    // Separate algorithms for upscaling vs downscaling
    ScalingAlgorithm downscaling_algorithm = ScalingAlgorithm::HERMITE;

    // Sharpening during upscale
    float sharpen_strength = 0.0f;      // 0.0-1.0

    // Algorithm parameters
    float antiring = 0.0f;              // Anti-ringing strength (0.0-1.0)
    float blur = 0.0f;                  // Blur/soften (0.0-1.0)

    // AR (Anti-Ringing) filter
    bool use_ar_filter = true;

    // Sigmoidal upscaling (like madVR)
    bool sigmoid = false;               // Sigmoid color space for upscaling
    float sigmoid_center = 0.75f;       // Sigmoid center point
    float sigmoid_slope = 6.5f;         // Sigmoid slope
};

// Debanding configuration
struct DebandingConfig {
    bool enabled = false;

    // Detection
    int iterations = 1;                 // Number of debanding passes (1-4)
    float threshold = 4.0f;             // Detection threshold (1.0-20.0)

    // Processing
    int radius = 16;                    // Debanding radius (8-32)
    float grain = 6.0f;                 // Add grain to hide banding (0.0-20.0)

    // Quality
    bool temporal = false;              // Temporal debanding (uses multiple frames)
};

// Dithering configuration
struct DitheringConfig {
    bool enabled = true;

    enum class Method {
        NONE,                           // No dithering
        ORDERED,                        // Ordered (Bayer) dithering
        RANDOM,                         // Random dithering
        ERROR_DIFFUSION,                // Error diffusion (Floyd-Steinberg)
        BLUE_NOISE,                     // Blue noise (best quality)
        WHITE_NOISE                     // White noise
    } method = Method::BLUE_NOISE;

    // Dither strength
    float strength = 1.0f;              // 0.0-2.0

    // Temporal dithering (reduces flickering)
    bool temporal = true;

    // LUT size for dithering
    int lut_size = 64;                  // 6-bit (64) or 8-bit (256)
};

// Complete processing configuration
struct ProcessingConfig {
    // Core processing
    ToneMappingConfig tone_mapping;
    ColorConfig color;
    NLSConfig nls;
    BlackBarConfig black_bars;

    // Scaling (madVR-style)
    ChromaUpscalingConfig chroma_upscaling;
    ImageUpscalingConfig image_upscaling;

    // Quality enhancements
    DebandingConfig debanding;
    DitheringConfig dithering;

    // Optional processing
    SharpeningConfig sharpening;
    DeinterlaceConfig deinterlace;

    // Output configuration
    uint32_t output_width = 3840;
    uint32_t output_height = 2160;
    float output_refresh_rate = 60.0f;

    // Performance/quality tradeoffs
    enum class Quality {
        FAST,           // Fastest processing, lower quality
        BALANCED,       // Balance speed and quality
        HIGH_QUALITY    // Best quality, may be slower
    } quality = Quality::BALANCED;

    // Debug/diagnostics
    bool show_stats_overlay = false;
    bool show_color_bars = false;
    bool show_black_bar_detection = false;

    // Preset management
    std::string preset_name = "Default";

    // Validation
    bool validate() const;

    // Load/save presets
    bool loadPreset(const std::string& path);
    bool savePreset(const std::string& path) const;
};

} // namespace ares
