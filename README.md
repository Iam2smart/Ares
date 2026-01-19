# Ares

**Professional Linux HDR Video Processor - madVR for Linux**

Ares is a complete HDR video processing pipeline for Linux that rivals madVR on Windows. Built specifically for headless home theater systems, it captures video from Blackmagic DeckLink devices, applies GPU-accelerated HDR tone mapping via libplacebo/Vulkan, and outputs to projectors with cinema-grade processing and sub-frame latency.

**Current Status:** Phases 1-6 Complete - Full HDR processing pipeline with OSD control

## Features

### Video Processing (Phase 1-6 ✅)

- **Automatic HDR/SDR Detection**
  - Detects HDR10, HLG, and SDR content automatically
  - SDR content → BT.709 color space (no tone mapping)
  - HDR content → BT.2020 color space (apply tone mapping)
  - Prevents incorrect processing of SDR sources

- **Professional HDR Tone Mapping**
  - Multiple algorithms: BT.2390, Reinhard, Hable, Mobius, custom LUTs
  - GPU-accelerated via Vulkan and libplacebo
  - Sub-frame latency (<16ms at 60Hz)
  - Configurable target/source nits
  - Real-time algorithm switching via OSD

- **NLS-Next (Non-Linear Stretch)**
  - Based on NotMithical's mpv NLS-Next shader
  - Advanced power curve stretching (horizontal & vertical)
  - Bidirectional stretch with center protection
  - Maintains cinemascope screen geometry
  - Optimized for 2.35:1 constant height projection

- **Advanced Enhancement Features**
  - **Dithering**: Blue noise, white noise, ordered, error diffusion
  - **Debanding**: Configurable iterations, threshold, grain injection
  - **Chroma Upsampling**: 4:2:0 → 4:4:4 with EWA Lanczos, Spline36/64, etc.
  - **Sharpening**: Adaptive content-aware sharpening
  - All features controllable via OSD with algorithm selection

### Display & Capture

- **Blackmagic DeckLink Capture**
  - 4K/8K HDR10/HLG support at 60fps
  - 10-bit color depth
  - Automatic frame rate detection (23.976, 24, 25, 29.97, 30, 50, 59.94, 60 fps)
  - PTS-based timing with 30-frame rolling window analysis
  - Stable frame rate detection with <5% deviation threshold

- **Automatic Refresh Rate Matching**
  - Eliminates judder by matching display refresh to source
  - Intelligent mode selection (exact match → integer multiples → closest)
  - Supports 23.976→24Hz, 24→48Hz/96Hz/120Hz, 60→60Hz/120Hz
  - DRM/KMS mode switching with EDID capability detection
  - Real-time switching with stability requirements

- **Black Bar Detection & Cropping**
  - Automatic letterbox/pillarbox detection
  - Applies before NLS to prevent stretching black bars
  - Confidence-based with temporal stability
  - Smooth crop transitions

### User Interface

- **madVR Envy-Style OSD**
  - Multi-tab interface: Picture, NLS, Enhance, Info
  - Real-time adjustment of all processing parameters
  - Live statistics display (frame rate, HDR type, resolution, etc.)
  - IP address display for remote access
  - Algorithm selection dropdowns for all features

- **IR Remote Control**
  - FLIRC and generic MCE receiver support
  - Full OSD navigation (up/down/left/right/select/back)
  - Customizable key mappings

- **Receiver Volume Integration**
  - Network control for Integra/Onkyo receivers (EISCP protocol)
  - Volume overlay display (bottom right corner)
  - Auto-hide with fade animation
  - Real-time volume monitoring

### System Integration

- **Headless Operation**
  - Systemd service with watchdog
  - Real-time kernel support (Arch Linux + RT kernel)
  - Automatic startup and recovery
  - OpenSSH remote access
  - Zero-copy frame presentation

## System Requirements

### Hardware
- **CPU:** 4+ cores, x86_64 (6+ cores recommended)
- **GPU:** NVIDIA RTX series with Vulkan 1.3 (RTX 4070 tested and recommended)
- **RAM:** 4GB minimum, 8GB recommended
- **Capture:** Blackmagic DeckLink 4K/8K series (DeckLink 4K Extreme 12G, DeckLink 8K Pro)
- **Display:** 4K HDR projector with HDMI 2.0+ (JVC, Sony, Epson tested)
- **IR Remote:** FLIRC USB or MCE IR receiver

### Software
- **OS:** Arch Linux (headless preferred)
- **Kernel:** Linux RT (PREEMPT_RT) strongly recommended for sub-frame latency
- **Driver:** NVIDIA proprietary (nvidia-dkms)
- **Desktop:** Not required (runs headless)

## Quick Start

### 1. Install Dependencies

```bash
sudo ./scripts/install_dependencies.sh
```

### 2. Install DeckLink SDK

Download from [Blackmagic Design](https://www.blackmagicdesign.com/support/download/) and install:

```bash
cd Blackmagic_DeckLink_SDK_*/Linux
sudo mkdir -p /usr/local/include/DeckLinkAPI
sudo cp include/*.h /usr/local/include/DeckLinkAPI/
sudo modprobe decklink
```

### 3. Build Ares

```bash
mkdir build && cd build
cmake .. -G Ninja
ninja
sudo ninja install
```

### 4. Configure System

```bash
sudo ./scripts/setup_system.sh
```

### 5. Start Service

```bash
sudo systemctl enable ares.service
sudo systemctl start ares.service
```

## Documentation

- [**IMPLEMENTATION_PLAN.md**](IMPLEMENTATION_PLAN.md) - Comprehensive development roadmap and architecture
- [**docs/BUILDING.md**](docs/BUILDING.md) - Detailed build instructions and troubleshooting
- [**docs/CONFIGURATION.md**](docs/CONFIGURATION.md) - Complete configuration reference
- [**docs/HARDWARE.md**](docs/HARDWARE.md) - Hardware compatibility and recommendations
- [**docs/TESTING.md**](docs/TESTING.md) - Testing strategies without full hardware setup
- [**docs/FEATURES.md**](docs/FEATURES.md) - Detailed feature documentation and comparisons
- [**docs/MADVR_FEATURES.md**](docs/MADVR_FEATURES.md) - madVR feature parity analysis
- [**docs/REMOTE_ACCESS.md**](docs/REMOTE_ACCESS.md) - SSH and remote management setup
- [**docs/RECEIVER_INTEGRATION.md**](docs/RECEIVER_INTEGRATION.md) - Integra/Onkyo receiver volume display

## Configuration

Edit `/etc/ares/ares.json` to customize settings:

```json
{
  "processing": {
    "tone_mapping": {
      "algorithm": "bt2390",
      "target_nits": 100,
      "source_nits": 1000
    },
    "nls": {
      "enabled": true,
      "strength": 0.5
    }
  },
  "display": {
    "connector": "HDMI-A-1",
    "refresh_rate_matching": true
  }
}
```

See [docs/CONFIGURATION.md](docs/CONFIGURATION.md) for all options.

## Architecture

Ares uses a modular architecture with specialized components:

```
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│   DeckLink  │ --> │   Capture   │ --> │   Frame     │
│   Hardware  │     │   Module    │     │   Queue     │
└─────────────┘     └─────────────┘     └─────────────┘
                                               │
                                               v
┌─────────────┐     ┌─────────────┐     ┌─────────────┐
│  Projector  │ <-- │   Display   │ <-- │  Processing │
│   Output    │     │   Module    │     │   (Vulkan)  │
└─────────────┘     └─────────────┘     └─────────────┘
                                               ^
                                               │
                          ┌────────────────────┴────────────┐
                          │                                  │
                    ┌─────────────┐                  ┌─────────────┐
                    │     OSD     │                  │  IR Remote  │
                    │   Overlay   │                  │    Input    │
                    └─────────────┘                  └─────────────┘
```

### Core Modules

- **Capture** - DeckLink frame acquisition and HDR metadata parsing
- **Processing** - Vulkan/libplacebo tone mapping pipeline
- **Display** - DRM/KMS output and EDID management
- **Sync** - Master clock and frame timing
- **OSD** - Vulkan overlay and menu system
- **Input** - IR remote and keyboard handling
- **Config** - JSON configuration management

## Development Status

**Current Version:** 1.0.0-rc1 (Release Candidate)

### Phase 1: Foundation ✅
- [x] Project structure and build system
- [x] CMake/Ninja configuration
- [x] Systemd integration with watchdog
- [x] Core logging and error handling
- [x] Documentation framework

### Phase 2: DeckLink Capture ✅
- [x] DeckLink SDK integration (HDMI/SDI input)
- [x] Frame capture with PTS timestamps
- [x] HDR10/HLG metadata parsing
- [x] Circular buffer with zero-copy

### Phase 3: HDR Metadata & Color Processing ✅
- [x] HDR10 static metadata (MaxCLL, MaxFALL, mastering display)
- [x] HLG transfer function support
- [x] Automatic HDR/SDR content detection
- [x] BT.709 (SDR) vs BT.2020 (HDR) automatic selection
- [x] Color space conversion pipeline

### Phase 4: Processing Pipeline ✅
- [x] Vulkan/libplacebo renderer integration
- [x] Multi-stage processing pipeline
- [x] Tone mapping (BT.2390, Reinhard, Hable, Mobius)
- [x] Black bar detection and cropping (before NLS)
- [x] NLS-Next shader (NotMithical's algorithm)
- [x] Dithering (blue noise, white noise, ordered, error diffusion)
- [x] Debanding with grain injection
- [x] Chroma upsampling (4:2:0 → 4:4:4)

### Phase 5: OSD & IR Remote ✅
- [x] IR remote input handler (FLIRC/MCE support)
- [x] Cairo/Pango OSD renderer with Vulkan compositing
- [x] madVR Envy-style multi-tab menu system
- [x] Picture, NLS, Enhance, Info tabs
- [x] Real-time parameter adjustment
- [x] IP address display in OSD

### Phase 6: Frame Rate Matching & EDID ✅
- [x] Frame rate detection from DeckLink PTS (23.976, 24, 25, 29.97, 30, 50, 59.94, 60 fps)
- [x] Automatic display mode switching
- [x] Intelligent refresh rate matching (eliminates judder)
- [x] EDID structures and HDR capability parsing
- [x] Enhanced OSD controls for all features
- [x] Algorithm selection dropdowns (dithering, debanding, chroma)
- [x] Live refresh rate info in Info tab

### What's Next
- [ ] Connect OSD changes to live processing config
- [ ] EDID implementation for auto-configuration
- [ ] Web interface for remote management
- [ ] Dolby Vision dynamic metadata support
- [ ] Motion interpolation (optional)

## Performance Targets

| Metric | Target | Notes |
|--------|--------|-------|
| Latency | <16ms @ 60Hz | Sub-frame latency |
| Frame drops | 0 per hour | Perfect delivery |
| Jitter | <1ms | Smooth motion |
| CPU usage | <30% | 8-core system |
| GPU usage | <80% | RTX 3060+ |
| Memory | <500MB | Resident set |

## Contributing

Contributions are welcome! Please:
1. Read the [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md)
2. Check existing issues and PRs
3. Follow the existing code style
4. Write tests for new features
5. Update documentation

## License

MIT License - See [LICENSE](LICENSE) for details

## Acknowledgments

- **libplacebo** - GPU-accelerated image processing library
- **madVR** - Inspiration for HDR tone mapping approach
- **Blackmagic Design** - DeckLink SDK and hardware
- **NVIDIA** - Vulkan drivers and GPU support

## Support

For issues, questions, or feature requests, please open an issue on GitHub.

## Roadmap

### Version 1.0-rc1 (Current - Ready for Testing)
- ✅ Full HDR/SDR tone mapping (BT.2390, Reinhard, Hable, Mobius)
- ✅ DeckLink capture with frame rate detection (23.976, 24, 25, 29.97, 30, 50, 59.94, 60 fps)
- ✅ Automatic refresh rate matching (eliminates judder)
- ✅ NLS-Next shader (NotMithical's algorithm)
- ✅ Dithering, debanding, chroma upsampling
- ✅ madVR Envy-style OSD with IR remote control
- ✅ Systemd integration with RT kernel support
- ⏳ Final testing and integration polish

### Version 1.1 (Planned)
- Live OSD → processing config updates
- EDID-based auto-configuration
- 3D LUT support
- Web interface for remote configuration
- Per-content preset auto-loading
- Calibration wizard

### Version 2.0 (Future)
- Dolby Vision dynamic metadata
- AMD GPU support (via Mesa/RadV)
- Motion interpolation (optional)
- Multiple input/output support
- Home automation integration (MQTT, etc.)

---

**Built with ❤️ for home theater enthusiasts**
