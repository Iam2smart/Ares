# Ares Features - madVR-Style HDR Processing

## Overview

Ares provides professional-grade HDR tone mapping and video processing similar to madVR, optimized for headless Linux operation with projectors.

**Status:** Phases 1-6 Complete - Full feature parity with madVR Envy for core HDR processing

---

## HDR Tone Mapping

### Supported Algorithms

1. **BT.2390 (EETF)** *(Default, Most Accurate)*
   - ITU-R BT.2390 standard tone mapping
   - Preserves creative intent
   - Configurable knee point and boost
   - Best for accurate HDR reproduction

2. **Reinhard**
   - Simple and fast
   - Good highlight preservation
   - Configurable peak luminance

3. **Hable (Uncharted 2)**
   - Filmic tone mapping
   - Shoulder/toe control
   - Great for cinematic content

4. **Mobius**
   - Preserves bright highlights
   - Smooth transition to peak
   - Good for high-brightness scenes

5. **Clip**
   - No tone mapping (simple clipping)
   - For testing/comparison

6. **Custom LUT**
   - Load 3D LUT from file
   - Full creative control

### Configurable Parameters

- **Target Nits**: Projector peak brightness (typically 48-150 nits)
- **Source Nits**: Content mastering brightness (auto-detected from metadata)
- **Contrast/Saturation**: Post-processing adjustments
- **Shadow Lift**: Enhance shadow detail
- **Highlight Compression**: Preserve highlight detail
- **Brightness/Gamma**: Fine-tune output

---

## NLS+ (Natural Light Simulation)

Based on madVR Envy's NLS algorithm with enhancements:

### Core Features

- **Dynamic Brightness Adaptation**
  - Analyzes scene content in real-time
  - Adapts brightness to match perceived lighting
  - Makes bright scenes brighter, dark scenes darker
  - Creates more "3D" look similar to OLED contrast

- **Separate Adaptation Speeds**
  - Bright scenes: Faster adaptation (more responsive)
  - Dark scenes: Slower adaptation (more cinematic)
  - Configurable per-scene type

- **Scene Change Detection**
  - Detects hard cuts and scene transitions
  - Optional adaptation reset on scene change
  - Prevents jarring transitions

- **Temporal Smoothing**
  - Smooth transitions between brightness levels
  - No flickering or pulsing
  - Configurable smoothing strength

- **Black Level Preservation**
  - Preserves true blacks
  - Configurable black threshold
  - Prevents crushed blacks

### Configuration

```json
{
  "nls": {
    "enabled": true,
    "strength": 0.5,                    // Overall effect (0.0-1.0)
    "adapt_speed": 0.3,                 // Base adaptation speed
    "bright_adapt_speed": 0.5,          // Bright scenes
    "dark_adapt_speed": 0.2,            // Dark scenes
    "scene_change_threshold": 0.15,     // Scene detection sensitivity
    "reset_on_scene_change": true,      // Reset on cuts
    "temporal_smoothing": 0.5,          // Smoothing strength
    "preserve_black": true,             // Preserve blacks
    "black_threshold": 0.05             // Black level
  }
}
```

---

## Auto Black Bar Detection

Intelligent letterbox/pillarbox detection and removal:

### Detection Features

- **Automatic Detection**
  - Analyzes multiple frames for stability
  - Detects top/bottom letterbox bars
  - Detects left/right pillarbox bars
  - Confidence-based activation

- **Symmetric Detection**
  - Ensures bars are symmetric
  - Prevents false detections from dark scenes
  - Configurable symmetry requirement

- **Temporal Stability**
  - Requires consistent detection across frames
  - Configurable detection frame count
  - Smooth crop transitions

### Cropping Behavior

- **Auto Crop** - Automatically crop detected bars
- **Zoom to Fit** - Zoom to fill screen (may crop content)
- **Smooth Transitions** - Gradual crop changes
- **Manual Override** - Force specific crop values

### Configuration

```json
{
  "black_bars": {
    "enabled": true,
    "threshold": 16,                    // Pixel brightness threshold
    "min_content_height": 0.5,          // Minimum 50% content
    "detection_frames": 10,             // Analyze 10 frames
    "confidence_threshold": 0.8,        // 80% confidence needed
    "symmetric_only": true,             // Only symmetric bars
    "auto_crop": true,                  // Auto crop when detected
    "crop_smoothing": 0.3,              // Smooth transitions
    "manual_crop": {
      "enabled": false,
      "top": 0, "bottom": 0,
      "left": 0, "right": 0
    }
  }
}
```

---

## Color Processing

### Gamut Mapping

- **Input Gamut**: BT.2020, BT.709, DCI-P3, Adobe RGB
- **Output Gamut**: BT.709, BT.2020, DCI-P3
- **Soft Clipping**: Preserve out-of-gamut colors
- **Desaturation**: Smooth gamut compression

### Color Adjustments

- **Hue Shift**: -180° to +180°
- **Temperature**: Warm/cool adjustment
- **Tint**: Magenta/green adjustment
- **Saturation**: Global saturation control

---

## Frame Rate Detection & Refresh Rate Matching

### Automatic Frame Rate Detection

Ares automatically detects the source frame rate from DeckLink PTS timestamps:

- **Supported Rates**: 23.976, 24.000, 25.000, 29.970, 30.000, 50.000, 59.940, 60.000, 119.880, 120.000 fps
- **Detection Method**: 30-frame rolling window analysis
- **Stability Requirement**: <5% deviation across all frames
- **Update Frequency**: Real-time detection with smooth transitions

**23.976 fps (Film) Support**: Fully supported and automatically detected. Ares will match 23.976 fps content to 24 Hz display modes (or 48 Hz, 96 Hz, 120 Hz multiples if available).

### Automatic Refresh Rate Matching

Eliminates judder by matching display refresh rate to source content:

**Matching Strategy**:
1. **Exact Match**: 24 fps → 24 Hz (preferred)
2. **Integer Multiples**: 24 fps → 48 Hz, 96 Hz, 120 Hz
3. **Integer Divisors**: 120 fps → 60 Hz (with frame blending)
4. **Closest Available**: Falls back to nearest refresh rate

**Examples**:
- 23.976 fps film → 24 Hz display (zero judder)
- 24.000 fps film → 24 Hz or 48 Hz display
- 60.000 fps video → 60 Hz or 120 Hz display
- 25.000 fps PAL → 25 Hz or 50 Hz display

**Configuration**:
```json
{
  "display": {
    "refresh_rate_matching": true,
    "prefer_exact_match": true,
    "allow_multiples": true,
    "max_multiple": 5
  }
}
```

### OSD Statistics

The Info tab displays real-time frame rate information:
- Detected source frame rate (e.g., "23.976 fps")
- Frame rate stability status ("Stable" / "Detecting")
- Current display refresh rate (e.g., "24.00 Hz")
- Match status ("Matched" / "Not Matched" / "Multiple")

---

## Dithering

Professional dithering to eliminate banding in gradients:

### Methods

1. **Blue Noise** *(Default, Best Quality)*
   - Psychovisually optimized noise pattern
   - Minimal perceived noise
   - Best for gradients and HDR content

2. **White Noise**
   - Random noise pattern
   - Simple and fast
   - Good for general use

3. **Ordered (Bayer)**
   - Deterministic pattern
   - Consistent results
   - Good for testing

4. **Error Diffusion (Floyd-Steinberg)**
   - Distributes quantization error
   - Best for 8-bit output
   - Higher CPU/GPU usage

### Configuration

```json
{
  "dithering": {
    "enabled": true,
    "method": "blue_noise",
    "lut_size": 64,
    "temporal": true
  }
}
```

**OSD Control**: Enhance tab → Dithering section with method dropdown

---

## Debanding

Removes banding artifacts from compressed video:

### Features

- **Iterations**: 1-4 passes (more = stronger effect)
- **Threshold**: Detection sensitivity (4-32)
- **Radius**: Sampling radius in pixels (8-32)
- **Grain**: Adds film grain to mask remaining artifacts (0.0-1.0)

### Configuration

```json
{
  "debanding": {
    "enabled": true,
    "iterations": 2,
    "threshold": 16,
    "radius": 16,
    "grain": 0.3
  }
}
```

**OSD Control**: Enhance tab → Debanding section with sliders

---

## Chroma Upsampling

Converts 4:2:0 chroma to 4:4:4 for improved color resolution:

### Algorithms

1. **EWA Lanczos** *(Best Quality)*
   - Elliptical Weighted Average Lanczos
   - Highest quality, minimal ringing
   - Recommended for HDR content

2. **Lanczos**
   - Standard Lanczos windowed sinc
   - High quality with sharp edges
   - Good all-around choice

3. **Spline16/36/64**
   - Polynomial interpolation
   - Smooth results
   - Spline36 recommended for balance

4. **Mitchell-Netravali**
   - Mitchell cubic filter
   - Good sharpness/smoothness balance

5. **Catmull-Rom**
   - Cubic interpolation
   - Sharp results

### Anti-Ringing

Configurable anti-ringing strength (0.0-1.0) to reduce overshoots near edges.

### Configuration

```json
{
  "chroma_upsampling": {
    "enabled": true,
    "algorithm": "ewa_lanczos",
    "antiring": 0.7
  }
}
```

**OSD Control**: Enhance tab → Chroma Upsampling section with algorithm dropdown

---

## Additional Features

### Sharpening

- **Adaptive Sharpening**
  - Content-aware
  - Configurable strength and radius
  - Threshold to avoid noise

### Deinterlacing

- **Auto-Detection**: Detects interlaced content
- **Methods**:
  - Weave (no deinterlace)
  - Bob (double framerate)
  - YADIF (high quality)
  - NNEDI3 (neural network, best quality)

### Output Configuration

- **Resolution**: Any supported resolution
- **Refresh Rate**: Automatic matching to source
- **Quality Modes**:
  - Fast: Fastest processing
  - Balanced: Speed/quality balance
  - High Quality: Best quality

---

## OSD (On-Screen Display)

### madVR Envy-Style Interface

Ares features a comprehensive OSD with 4 main tabs, accessible via IR remote:

### Tab 1: Picture (Tone Mapping)

```
Picture
├─ Tone Mapping Algorithm
│  └─ BT.2390 / Reinhard / Hable / Mobius / Clip / Custom LUT
├─ Target Nits (Projector brightness)
│  └─ Slider: 48-150 nits (default: 100)
├─ Source Nits (Content max brightness)
│  └─ Slider: 100-4000 nits (default: 1000)
├─ Contrast
│  └─ Slider: 0.5-2.0 (default: 1.0)
├─ Saturation
│  └─ Slider: 0.5-2.0 (default: 1.0)
├─ Shadow Lift
│  └─ Slider: 0.0-0.5 (default: 0.0)
└─ Highlight Compression
   └─ Slider: 0.0-1.0 (default: 0.5)
```

### Tab 2: NLS (Non-Linear Stretch)

```
NLS
├─ Enable NLS
│  └─ Toggle: On / Off
├─ Horizontal Stretch
│  └─ Slider: 0.0-1.0 (default: 0.5)
├─ Vertical Stretch
│  └─ Slider: 0.0-1.0 (default: 0.5)
├─ Horizontal Power
│  └─ Slider: 1.0-4.0 (default: 2.0)
├─ Vertical Power
│  └─ Slider: 1.0-4.0 (default: 2.0)
├─ Center Protection (Horizontal)
│  └─ Slider: 0.0-2.0 (default: 1.0)
└─ Center Protection (Vertical)
   └─ Slider: 0.0-2.0 (default: 1.0)
```

### Tab 3: Enhance (Dithering, Debanding, Chroma)

```
Enhance
├─ Dithering
│  ├─ Enable: Toggle
│  ├─ Method: Blue Noise / White Noise / Ordered / Error Diffusion
│  ├─ Temporal: Toggle
│  └─ LUT Size: 16-256
├─ Debanding
│  ├─ Enable: Toggle
│  ├─ Iterations: 1-4
│  ├─ Threshold: 4-32
│  ├─ Radius: 8-32
│  └─ Grain: 0.0-1.0
└─ Chroma Upsampling
   ├─ Enable: Toggle
   ├─ Algorithm: EWA Lanczos / Lanczos / Spline16/36/64 / Mitchell / Catmull-Rom
   └─ Anti-Ringing: 0.0-1.0
```

### Tab 4: Info (Statistics & System)

```
Info
├─ System Information
│  ├─ Version: 1.0.0-rc1
│  ├─ IP Address: [auto-detected]
│  └─ Uptime: [real-time]
├─ Source Information
│  ├─ Resolution: [detected, e.g., 3840x2160]
│  ├─ HDR Type: HDR10 / HLG / SDR
│  ├─ Color Space: BT.2020 / BT.709
│  ├─ Detected Frame Rate: [e.g., "23.976 fps"]
│  └─ Frame Rate Stable: Yes / No / Detecting
├─ Display Information
│  ├─ Output Resolution: [e.g., 3840x2160]
│  ├─ Display Refresh Rate: [e.g., "24.00 Hz"]
│  ├─ Refresh Matched: Yes / No / Multiple
│  └─ Mode Switches: [count]
└─ Performance
   ├─ Processing Time: [ms per frame]
   ├─ GPU Usage: [%]
   └─ Frame Drops: [count]
```

### IR Remote Navigation

- **Up/Down**: Navigate menu items
- **Left/Right**: Adjust values / Change tabs
- **Select/Enter**: Toggle settings / Enter submenus
- **Back/Exit**: Exit OSD / Go back
- **Menu**: Toggle OSD on/off
- **Info**: Show/hide Info tab overlay

**Supported Remotes**: FLIRC USB, generic MCE IR receivers

---

## Presets

### Built-in Presets

1. **Cinema (Dark Room)**
   - Target: 48 nits
   - NLS+: Strong (0.7)
   - Tone Mapping: BT.2390
   - Optimized for complete darkness

2. **Bright Room**
   - Target: 150 nits
   - NLS+: Moderate (0.3)
   - Tone Mapping: Hable
   - Enhanced brightness/contrast

3. **Gaming**
   - Target: 100 nits
   - NLS+: Disabled
   - Low latency mode
   - Fast quality preset

### Custom Presets

- Save up to 5 custom presets
- Per-content presets (automatically load based on metadata)
- Export/import presets via JSON

---

## Performance Targets

| Metric | Target | Notes |
|--------|--------|-------|
| Latency | <16ms @ 60Hz | Sub-frame latency |
| 4K60 Processing | <8ms | GPU processing time |
| NLS+ Overhead | <1ms | Minimal impact |
| Black Bar Detection | <0.5ms | Per-frame analysis |
| Frame Drops | 0 per hour | Perfect delivery |

---

## Hardware Requirements

### Minimum

- CPU: 4 cores @ 2.5 GHz
- GPU: NVIDIA RTX 2060 (6GB VRAM)
- RAM: 4GB
- Capture: DeckLink Mini Monitor 4K

### Recommended

- CPU: 6+ cores @ 3.0 GHz
- GPU: NVIDIA RTX 3060 (12GB VRAM) or better
- RAM: 8GB
- Capture: DeckLink 4K Extreme 12G

### For 4K120 / 8K

- CPU: 8+ cores (X3D preferred)
- GPU: NVIDIA RTX 4070 or better
- RAM: 16GB
- Capture: DeckLink 8K Pro

---

## Comparison to madVR

| Feature | madVR | Ares |
|---------|-------|------|
| Platform | Windows | Linux |
| HDR Tone Mapping | ✅ | ✅ |
| NLS | ✅ Envy | ✅ NLS+ (Envy-style) |
| Black Bar Detection | ✅ | ✅ |
| Multiple Algorithms | ✅ | ✅ |
| Custom LUTs | ✅ | ✅ |
| Presets | ✅ | ✅ |
| OSD Control | ✅ | ✅ |
| RT Kernel Support | ❌ | ✅ |
| Headless Operation | ❌ | ✅ |
| Open Source | ❌ | ✅ (MIT) |

---

## Future Enhancements

- [ ] Motion interpolation (frame blending)
- [ ] Dolby Vision dynamic metadata
- [ ] HDR10+ support
- [ ] Multiple source inputs
- [ ] Web interface for remote control
- [ ] Automatic calibration wizard
- [ ] 3D LUT generation tool
- [ ] Frame-by-frame Dolby Vision metadata
