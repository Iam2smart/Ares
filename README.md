# Ares

**Headless Linux HDR Video Processor with madVR-style Tone Mapping**

Ares is a professional video processing application for Linux that provides GPU-accelerated HDR tone mapping for projectors, similar to madVR on Windows. It captures video from Blackmagic DeckLink devices, applies sophisticated tone mapping and color processing via libplacebo/Vulkan, and outputs to projectors with minimal latency.

## Features

- **Real-time HDR Tone Mapping**
  - Multiple algorithms: BT.2390, Reinhard, Hable, custom LUTs
  - GPU-accelerated via Vulkan and libplacebo
  - Sub-frame latency (<16ms at 60Hz)

- **Professional Capture**
  - Blackmagic DeckLink integration
  - 4K60 HDR10/HLG support
  - 10-bit color depth

- **Dynamic Display Management**
  - Automatic refresh rate matching
  - EDID parsing and override support
  - Black bar detection and cropping

- **Natural Light Simulation (NLS)**
  - Adaptive brightness adjustment
  - Smooth scene transitions
  - Configurable adaptation speed

- **On-Screen Display (OSD)**
  - Interactive menu system
  - Real-time statistics
  - Preset management

- **IR Remote Control**
  - FLIRC and generic MCE receiver support
  - Customizable key mappings
  - Macro support

- **System Integration**
  - Systemd service with watchdog
  - Real-time kernel support
  - Automatic startup and recovery

## System Requirements

### Hardware
- **CPU:** 4+ cores, x86_64
- **GPU:** NVIDIA with Vulkan 1.3 (RTX 3060+ recommended)
- **RAM:** 4GB minimum, 8GB recommended
- **Capture:** Blackmagic DeckLink with HDMI input
- **Display:** 4K HDR projector or TV with HDMI 2.0

### Software
- **OS:** Arch Linux (headless or desktop)
- **Kernel:** Linux RT recommended
- **Driver:** NVIDIA proprietary (nvidia-dkms)

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

**Current Version:** 1.0.0 (In Development)

### Phase 1: Foundation ✅
- [x] Project structure and build system
- [x] CMake configuration
- [x] Systemd integration
- [x] Documentation

### Phase 2: DeckLink Capture (In Progress)
- [ ] DeckLink SDK integration
- [ ] Frame capture with PTS
- [ ] HDR metadata parsing
- [ ] Circular buffer implementation

### Phase 3-8: See [IMPLEMENTATION_PLAN.md](IMPLEMENTATION_PLAN.md)

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

### Version 1.0 (Current)
- Basic HDR tone mapping (BT.2390)
- DeckLink capture
- OSD and IR remote control
- Systemd integration

### Version 1.1 (Future)
- Multiple tone mapping algorithms
- 3D LUT support
- Web interface for remote configuration
- Motion interpolation

### Version 2.0 (Future)
- AMD GPU support
- Dolby Vision support
- Multiple input/output support
- Home automation integration

---

**Built with ❤️ for home theater enthusiasts**
