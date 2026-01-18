# madVR Feature Comparison

This document details how Ares implements madVR-style features using libplacebo.

## Feature Overview

| Feature | madVR | Ares (libplacebo) | Status |
|---------|-------|-------------------|--------|
| Chroma Upscaling | ✓ | ✓ | Implemented |
| Image Upscaling | ✓ | ✓ | Implemented |
| HDR Tone Mapping | ✓ | ✓ | Implemented |
| Debanding | ✓ | ✓ | Implemented |
| Dithering | ✓ | ✓ | Implemented |
| Black Bar Detection | ✓ | ✓ | Implemented |
| NLS (Non-Linear Stretch) | ✓ | ✓ | Implemented |
| OSD Control | ✓ | ✓ | Phase 5 |
| Custom LUTs | ✓ | ✓ | Phase 5 |
| ICC Profiles | ✓ | ✓ | Phase 5 |

## 1. Pixel Format Conversion

### YCbCr 4:2:2 → 4:2:0 Conversion

**Why?** DeckLink outputs 10-bit 4:2:2 (v210 format), which has chroma sampled at half horizontal resolution. By converting to 4:2:0 (p010), we can let the high-quality renderer upscale chroma instead of using DeckLink's hardware upscaler.

**Configuration:**
```cpp
config.chroma_upscaling.force_420 = true;  // Convert v210 → p010
```

**Benefits:**
- Control over chroma upscaling algorithm
- Better quality than hardware upscaling
- Consistent with madVR workflow
- Reduces bandwidth for processing

## 2. Chroma Upscaling

Convert 4:2:0 or 4:2:2 to 4:4:4 with high-quality algorithms (like madVR's chroma upscaling).

### Available Algorithms

| Algorithm | Speed | Quality | madVR Equivalent |
|-----------|-------|---------|------------------|
| Bilinear | Fast | Low | Bilinear |
| Bicubic | Medium | Good | Bicubic |
| Mitchell-Netravali | Medium | Good | SoftCubic |
| Catmull-Rom | Medium | Good | SoftCubic Sharp |
| Lanczos | Slow | High | Lanczos |
| EWA Lanczos | Slowest | Best | Jinc/EWA Lanczos |
| Spline36 | Medium | High | Spline36 |
| Spline64 | Slow | Very High | Spline64 |

### Configuration

```cpp
// Basic configuration
config.chroma_upscaling.enabled = true;
config.chroma_upscaling.algorithm = ScalingAlgorithm::EWA_LANCZOS;
config.chroma_upscaling.antiring = 0.5f;     // Anti-ringing (like madVR)

// Advanced (madVR-style supersampling)
config.chroma_upscaling.supersample = true;
config.chroma_upscaling.supersample_factor = 2.0f;
```

### Comparison to madVR

- **madVR NGU Low**: ~`Spline36` with AR filter
- **madVR NGU Medium**: ~`EWA_LANCZOS` with supersampling
- **madVR NGU High**: ~`EWA_LANCZOS` with 2x supersampling + AR

## 3. Image Upscaling

Scale luma (brightness) with high-quality algorithms.

### Available Algorithms

#### Fast (< 5ms)
- **Bilinear** - Basic, fast
- **Bicubic** - Standard cubic interpolation
- **Mitchell** - Mitchell-Netravali balanced
- **Catmull-Rom** - Sharper cubic

#### Medium Quality (5-10ms)
- **Lanczos** - Windowed sinc (3-tap)
- **Spline16** - 2-tap spline
- **Spline36** - 3-tap spline (madVR quality)
- **Spline64** - 4-tap spline

#### High Quality (10-30ms)
- **EWA Lanczos** - Elliptical weighted average
- **EWA Lanczos Sharp** - Sharpened version
- **Jinc** - Bessel function (madVR Jinc equivalent)

#### Neural Network (30-100ms)
- **NNEDI3** - Neural network edge-directed interpolation
  - `NNEDI3_16` - 16 neurons (fast)
  - `NNEDI3_32` - 32 neurons (balanced)
  - `NNEDI3_64` - 64 neurons (high quality)
  - `NNEDI3_128` - 128 neurons (best quality, slowest)

#### Super Resolution (50-200ms)
- **Super-xBR** - Extreme super resolution
- **RAVU** - Rapid and Accurate Video Upscaling
- **RAVU Lite** - Faster RAVU variant

### Configuration

```cpp
// Separate algorithms for luma and chroma
config.image_upscaling.luma_algorithm = ScalingAlgorithm::NNEDI3_64;
config.image_upscaling.chroma_algorithm = ScalingAlgorithm::EWA_LANCZOS;

// Downscaling (when output < input)
config.image_upscaling.downscaling_algorithm = ScalingAlgorithm::HERMITE;

// Advanced options
config.image_upscaling.antiring = 0.5f;       // Reduce ringing artifacts
config.image_upscaling.sharpen_strength = 0.3f; // Post-upscale sharpening
config.image_upscaling.use_ar_filter = true;  // Anti-ringing filter

// Sigmoidal upscaling (madVR feature)
config.image_upscaling.sigmoid = true;        // Upscale in sigmoid color space
config.image_upscaling.sigmoid_center = 0.75f;
config.image_upscaling.sigmoid_slope = 6.5f;
```

### Comparison to madVR

| madVR Setting | Ares Equivalent |
|---------------|-----------------|
| DXVA2 | `Bilinear` or `Bicubic` |
| Lanczos | `Lanczos` (3-tap) |
| Spline36 | `Spline36` |
| Spline64 | `Spline64` |
| Jinc | `Jinc` or `EWA_LANCZOS` |
| NGU Standard | `NNEDI3_32` |
| NGU High | `NNEDI3_64` |
| NGU Very High | `NNEDI3_128` |
| Super-xBR | `SUPER_XBIR` |

## 4. HDR Tone Mapping

Convert HDR (PQ/HLG) to SDR display.

### Available Algorithms

```cpp
enum class ToneMappingAlgorithm {
    BT2390,      // ITU-R BT.2390 EETF - Industry standard, most accurate
    REINHARD,    // Reinhard - Simple, fast
    HABLE,       // Hable (Uncharted 2) - Filmic look
    MOBIUS,      // Mobius - Preserves bright highlights
    CLIP,        // Hard clip (no tone mapping)
    CUSTOM       // Load custom LUT
};
```

### Configuration

```cpp
config.tone_mapping.algorithm = ToneMappingAlgorithm::BT2390;
config.tone_mapping.target_nits = 100.0f;     // Your projector's peak brightness
config.tone_mapping.source_nits = 1000.0f;    // Content peak brightness
config.tone_mapping.use_metadata = true;      // Use HDR10 metadata

// BT.2390 specific
config.tone_mapping.params.knee_point = 0.75f;
config.tone_mapping.params.max_boost = 1.2f;

// Post-processing
config.tone_mapping.contrast = 1.0f;
config.tone_mapping.saturation = 1.0f;
config.tone_mapping.brightness = 0.0f;
config.tone_mapping.gamma = 1.0f;

// Shadow/highlight control
config.tone_mapping.shadow_lift = 0.0f;           // Lift shadows (0.0-0.3)
config.tone_mapping.highlight_compression = 0.0f; // Compress highlights
```

### Comparison to madVR

| madVR Algorithm | Ares Equivalent | Notes |
|-----------------|-----------------|-------|
| madVR HDR → SDR | `BT2390` | ITU standard |
| Clip | `CLIP` | Hard clip |
| Reinhard | `REINHARD` | Simple tone mapping |
| Hable | `HABLE` | Filmic curve |
| Custom curve | `CUSTOM` | Load LUT file |

## 5. Debanding

Remove color banding artifacts from compressed sources.

### Configuration

```cpp
config.debanding.enabled = true;
config.debanding.iterations = 1;       // Number of passes (1-4)
config.debanding.threshold = 4.0f;     // Detection threshold (1.0-20.0)
config.debanding.radius = 16;          // Processing radius (8-32)
config.debanding.grain = 6.0f;         // Add grain to hide banding (0-20)
config.debanding.temporal = false;     // Use multiple frames (slower)
```

### Comparison to madVR

Similar to madVR's debanding but using libplacebo's implementation:
- **madVR Low**: `iterations=1, threshold=6, radius=12`
- **madVR Medium**: `iterations=2, threshold=4, radius=16`
- **madVR High**: `iterations=4, threshold=2, radius=24`

## 6. Dithering

Add dither to prevent quantization artifacts when converting to lower bit depths.

### Available Methods

```cpp
enum class DitheringMethod {
    NONE,               // No dithering
    ORDERED,            // Ordered (Bayer) - Fast, visible pattern
    RANDOM,             // Random - Better than ordered
    ERROR_DIFFUSION,    // Floyd-Steinberg - Good quality
    BLUE_NOISE,         // Blue noise - Best quality (madVR equivalent)
    WHITE_NOISE         // White noise - Alternative
};
```

### Configuration

```cpp
config.dithering.enabled = true;
config.dithering.method = DitheringMethod::BLUE_NOISE; // Best quality
config.dithering.strength = 1.0f;      // 0.0-2.0
config.dithering.temporal = true;      // Temporal dithering (reduces flicker)
config.dithering.lut_size = 64;        // 6-bit LUT (64) or 8-bit (256)
```

### Comparison to madVR

- **madVR Error Diffusion**: ~`ERROR_DIFFUSION`
- **madVR Random**: ~`RANDOM` or `WHITE_NOISE`
- **madVR Best Quality**: ~`BLUE_NOISE` with temporal

## 7. Color Management

### Gamut Mapping

```cpp
config.color.input_gamut = ColorGamut::BT2020;   // HDR input
config.color.output_gamut = ColorGamut::BT709;   // SDR output
config.color.soft_clip = true;                   // Soft clip out-of-gamut
config.color.desaturation = 0.0f;                // Desaturate out-of-gamut
```

### Color Adjustments

```cpp
config.color.hue = 0.0f;            // Hue shift (-180 to +180)
config.color.temperature = 0.0f;    // Color temperature (-1.0 to +1.0)
config.color.tint = 0.0f;           // Tint adjustment
```

## 8. Black Bar Detection

Automatic letterbox/pillarbox detection and cropping.

### Configuration

```cpp
config.black_bars.enabled = true;
config.black_bars.threshold = 16;               // Brightness threshold
config.black_bars.detection_frames = 10;        // Frames to analyze
config.black_bars.confidence_threshold = 0.8f;  // 80% confidence
config.black_bars.auto_crop = true;             // Auto-apply crop
config.black_bars.symmetric_only = true;        // Only symmetric bars
config.black_bars.crop_smoothing = 0.3f;        // Smooth transitions
```

### Comparison to madVR

Similar functionality with confidence-based detection and smooth transitions.

## 9. NLS (Non-Linear Stretch)

Warp 16:9 content to fit 2.35:1 cinemascope screens with minimal center distortion.

### Configuration

```cpp
config.nls.enabled = true;
config.nls.target_aspect = NLSConfig::TargetAspect::SCOPE_235; // 2.35:1
config.nls.center_strength = 0.0f;   // No stretch in center
config.nls.edge_strength = 1.0f;     // Full stretch at edges
config.nls.transition_width = 0.3f;  // Smooth transition zone
config.nls.interpolation = NLSConfig::InterpolationQuality::BICUBIC;
```

### Comparison to madVR Envy

Identical concept - less distortion in center where focus is, more at edges.

## 10. OSD System (Phase 5)

Multi-tab interface like madVR Envy with IR remote control.

### Tab Structure

1. **Processing Tab**
   - Input format
   - Frame rate conversion
   - Deinterlacing
   - Black bar detection

2. **Scaling Tab**
   - Chroma upscaling algorithm
   - Luma upscaling algorithm
   - Downscaling algorithm
   - AR filter settings
   - Sigmoidal upscaling

3. **Tone Mapping Tab**
   - Algorithm selection
   - Target/source nits
   - Algorithm parameters
   - Contrast/saturation/brightness
   - Shadow/highlight control

4. **Color Tab**
   - Input/output gamut
   - Gamut mapping
   - Hue/temperature/tint
   - Custom LUTs
   - ICC profiles

5. **Enhancements Tab**
   - Debanding (iterations, threshold, radius)
   - Dithering (method, strength)
   - Sharpening
   - NLS configuration

6. **Display Tab**
   - Output resolution
   - Refresh rate
   - VSync settings
   - HDR output mode

7. **Misc Tab**
   - Preset management
   - Statistics overlay
   - Debug options
   - System info

8. **Info Tab**
   - Current settings
   - Performance stats
   - Input/output info
   - Version/license

### IR Remote Control

- **Arrow Keys**: Navigate menu
- **OK/Enter**: Select/toggle
- **Back**: Go back / Close menu
- **Menu**: Open OSD
- **Number Keys**: Quick preset selection
- **Red/Green/Yellow/Blue**: Tab shortcuts

## Performance Comparison

### RTX 4070 Performance Estimates (4K → 4K)

| Configuration | Frame Time | FPS | madVR Equivalent |
|---------------|------------|-----|------------------|
| Fast | ~4ms | 250fps | madVR Low |
| Balanced | ~8ms | 125fps | madVR Medium |
| High Quality (Spline64) | ~12ms | 83fps | madVR High |
| Very High (NNEDI3_64) | ~20ms | 50fps | madVR Very High |
| Ultra (NNEDI3_128 + Super) | ~40ms | 25fps | madVR Ultra |

**Note**: Times include full pipeline (chroma upscaling, image upscaling, tone mapping, debanding, dithering).

## Recommended Settings

### For 4K SDR Projector (100 nits)

```cpp
// Scaling
config.chroma_upscaling.algorithm = ScalingAlgorithm::EWA_LANCZOS;
config.chroma_upscaling.antiring = 0.5f;
config.image_upscaling.luma_algorithm = ScalingAlgorithm::NNEDI3_64;
config.image_upscaling.sigmoid = true;

// Tone mapping
config.tone_mapping.algorithm = ToneMappingAlgorithm::BT2390;
config.tone_mapping.target_nits = 100.0f;
config.tone_mapping.source_nits = 1000.0f;

// Quality
config.debanding.enabled = true;
config.debanding.iterations = 2;
config.dithering.method = DitheringMethod::BLUE_NOISE;
```

### For 4K HDR Projector (500 nits)

```cpp
// Minimal processing (direct HDR output)
config.chroma_upscaling.algorithm = ScalingAlgorithm::EWA_LANCZOS;
config.image_upscaling.luma_algorithm = ScalingAlgorithm::SPLINE36;
config.tone_mapping.algorithm = ToneMappingAlgorithm::CLIP; // No tone mapping
config.tone_mapping.target_nits = 500.0f;
```

## Future Enhancements

Features planned for future releases:

1. **Custom LUTs** - Load 3D LUT files (.cube, .3dl)
2. **ICC Profiles** - Display calibration profiles
3. **Motion Interpolation** - Frame rate conversion
4. **Film Grain Synthesis** - Add film grain to match source
5. **Temporal Denoising** - Multi-frame noise reduction
6. **Edge Enhancement** - Content-aware sharpening
7. **Dynamic Tone Mapping** - Per-scene HDR adaptation
8. **Subtitle Rendering** - Integrated subtitle support

## References

- libplacebo documentation: https://libplacebo.org/
- madVR forum: https://forum.doom9.org/forumdisplay.php?f=146
- ITU-R BT.2390: HDR tone mapping standard
- DeckLink SDK: Blackmagic capture card API
