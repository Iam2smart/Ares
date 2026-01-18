# Ares Features - madVR-Style HDR Processing

## Overview

Ares provides professional-grade HDR tone mapping and video processing similar to madVR, optimized for headless Linux operation with projectors.

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

### Main Menu Structure

```
┌─ Picture
│  ├─ Tone Mapping Algorithm
│  ├─ Target Nits
│  ├─ Contrast/Saturation
│  ├─ Shadow/Highlight
│  └─ Advanced...
│
├─ NLS+ (Natural Light Simulation)
│  ├─ Enable/Disable
│  ├─ Strength
│  ├─ Adaptation Speed
│  ├─ Scene Detection
│  └─ Advanced...
│
├─ Black Bars
│  ├─ Auto Detection On/Off
│  ├─ Manual Crop
│  ├─ Zoom to Fit
│  └─ Settings...
│
├─ Color
│  ├─ Gamut
│  ├─ Hue/Saturation
│  ├─ Temperature/Tint
│  └─ Advanced...
│
├─ Output
│  ├─ Resolution
│  ├─ Refresh Rate
│  └─ Quality Mode
│
├─ Presets
│  ├─ Cinema (Dark Room)
│  ├─ Bright Room
│  ├─ Gaming
│  ├─ Custom 1-5
│  ├─ Save Current
│  └─ Load...
│
├─ Diagnostics
│  ├─ Show Statistics
│  ├─ Show Black Bar Detection
│  ├─ Show Color Bars
│  └─ Performance Monitor
│
└─ System
   ├─ About
   ├─ Version Info
   └─ Settings
```

### Navigation

- **Up/Down**: Navigate menu items
- **Left/Right**: Adjust values
- **Enter**: Select/confirm
- **Back/Escape**: Go back
- **Menu**: Toggle OSD
- **Info**: Toggle statistics overlay

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
