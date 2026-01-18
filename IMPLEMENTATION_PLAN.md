# Ares Implementation Plan
## Headless Linux HDR Video Processor

---

## 1. Project Overview

**Ares** is a headless Linux video processor designed to replicate madVR-style HDR tone mapping for projectors. It provides professional-grade video processing with:

- Real-time DeckLink video capture
- GPU-accelerated HDR tone mapping via Vulkan/libplacebo
- Dynamic refresh rate switching based on EDID
- IR remote control with OSD overlay
- Sub-millisecond latency with RT kernel support

**Target Platform:** Arch Linux (headless), NVIDIA GPU, Blackmagic DeckLink capture card

---

## 2. System Architecture

### High-Level Data Flow

```
[DeckLink Input] → [Capture Thread] → [Frame Queue] → [Processing Thread] → [Vulkan/libplacebo] → [Display Output]
                                              ↓
                                    [Metadata: PTS, HDR, Color]
                                              ↓
                                    [Master Clock Sync]
                                              ↓
                                    [OSD Overlay] ← [IR Remote Input]
```

### Core Modules

1. **Capture Module** (`src/capture/`)
   - DeckLink SDK integration
   - Frame acquisition with PTS timestamps
   - HDR metadata extraction (HDR10, HLG, Dolby Vision)
   - Buffer management (2-3 frame circular buffer)

2. **Processing Module** (`src/processing/`)
   - libplacebo tone mapping pipeline
   - Vulkan compute shader integration
   - Dynamic HDR parameter adjustment
   - Color space conversion (BT.2020 → target gamut)

3. **Display Module** (`src/display/`)
   - DRM/KMS output management
   - Dynamic refresh rate switching
   - EDID parsing and validation
   - Mode setting and connector management

4. **Clock/Sync Module** (`src/sync/`)
   - Master clock using CLOCK_MONOTONIC_RAW
   - Frame timing and vsync synchronization
   - Jitter compensation
   - Adaptive frame rate matching

5. **OSD Module** (`src/osd/`)
   - Vulkan-based overlay rendering
   - Menu system (settings, presets, diagnostics)
   - Real-time statistics display
   - Transparency and positioning control

6. **Input Module** (`src/input/`)
   - FLIRC/evdev IR receiver integration
   - Key mapping and macro support
   - Remote control protocol
   - Input event queue

7. **Configuration Module** (`src/config/`)
   - JSON-based persistent storage
   - Runtime parameter updates
   - EDID override support
   - Preset management

8. **Core/Utils** (`src/core/`)
   - Logging system
   - Error handling
   - Thread management
   - Performance profiling

---

## 3. Module Details

### 3.1 Capture Module

**Responsibilities:**
- Initialize DeckLink device
- Configure input format (4K, HDR, 23.976/24/60 fps)
- Capture frames with minimal latency
- Extract HDR metadata (MaxCLL, MaxFALL, mastering display)
- Handle format changes dynamically

**Key Classes:**
- `DeckLinkCapture` - Main capture interface
- `FrameBuffer` - Circular buffer with PTS
- `HDRMetadata` - HDR10/HLG/DV metadata parser

**Dependencies:**
- Blackmagic DeckLink SDK
- pthread (real-time thread)

**Configuration:**
```json
{
  "capture": {
    "device_index": 0,
    "input_connection": "HDMI",
    "pixel_format": "10BitYUV",
    "buffer_size": 3
  }
}
```

---

### 3.2 Processing Module

**Responsibilities:**
- Initialize Vulkan device and command queues
- Create libplacebo rendering context
- Apply HDR tone mapping (Reinhard, Hable, BT.2390)
- Perform color gamut mapping
- Apply user-defined tone curve adjustments

**Key Classes:**
- `VulkanContext` - Vulkan device/queue management
- `PlaceboRenderer` - libplacebo integration
- `ToneMappingPipeline` - Tone mapping configuration
- `ColorSpaceConverter` - Gamut mapping

**Tone Mapping Algorithms:**
1. **Reinhard** - Simple, preserves highlights
2. **Hable (Uncharted 2)** - Filmic look
3. **BT.2390 (EETF)** - ITU standard for HDR→SDR
4. **Custom Curves** - User-defined LUT support

**Dependencies:**
- libplacebo (≥6.338.0)
- Vulkan (≥1.3)
- NVIDIA driver with Vulkan support

**Configuration:**
```json
{
  "processing": {
    "tone_mapping": {
      "algorithm": "bt2390",
      "target_nits": 100,
      "source_nits": 1000,
      "contrast": 1.0
    },
    "color": {
      "gamut": "bt709",
      "transfer": "gamma22"
    },
    "nls": {
      "enabled": true,
      "strength": 0.5
    }
  }
}
```

---

### 3.3 Display Module

**Responsibilities:**
- Query available DRM/KMS connectors
- Parse EDID for supported modes
- Set optimal display mode dynamically
- Handle refresh rate switching
- Support EDID overrides for scalers/splitters

**Key Classes:**
- `DRMDisplay` - KMS/DRM interface
- `EDIDParser` - EDID decoding
- `ModeManager` - Dynamic mode switching

**EDID Override:**
```json
{
  "display": {
    "connector": "HDMI-A-1",
    "edid_override": "/etc/ares/custom_edid.bin",
    "preferred_modes": [
      {"width": 3840, "height": 2160, "refresh": 24},
      {"width": 1920, "height": 1080, "refresh": 60}
    ]
  }
}
```

**Dependencies:**
- libdrm
- libudev

---

### 3.4 Clock/Sync Module

**Responsibilities:**
- Maintain master clock reference
- Calculate frame presentation times
- Synchronize capture → display pipeline
- Measure and compensate for jitter

**Key Classes:**
- `MasterClock` - High-resolution monotonic clock
- `FrameScheduler` - PTS-based frame timing
- `JitterCompensator` - Adaptive timing adjustment

**Timing Strategy:**
```
1. Capture frame with PTS_capture
2. Queue frame with target PTS_display = PTS_capture + latency_target
3. Processing thread polls clock:
   - If (current_time >= PTS_display - processing_time):
     - Process frame
     - Submit to display
4. Measure actual display time, adjust latency_target
```

---

### 3.5 OSD Module

**Responsibilities:**
- Render semi-transparent overlay
- Display menu system
- Show real-time statistics (fps, latency, nits)
- Render debug information

**Menu Structure:**
```
Main Menu
├── Picture
│   ├── Tone Mapping
│   ├── Color Space
│   └── Brightness/Contrast
├── Motion
│   ├── Frame Rate Matching
│   └── Judder Reduction
├── Presets
│   ├── Cinema
│   ├── Bright Room
│   └── Custom
└── System
    ├── Display Info
    ├── Statistics
    └── About
```

**Key Classes:**
- `VulkanOSD` - Vulkan overlay renderer
- `MenuSystem` - Hierarchical menu management
- `StatsDisplay` - Real-time metrics

**Dependencies:**
- Vulkan (shared context with processing)
- FreeType (text rendering)

---

### 3.6 Input Module

**Responsibilities:**
- Monitor evdev input devices
- Decode IR remote signals (FLIRC)
- Map keys to OSD actions
- Support macros and shortcuts

**Key Classes:**
- `InputManager` - evdev device monitoring
- `KeyMapper` - Key → action binding
- `RemoteProfile` - Device-specific mappings

**Key Bindings Example:**
```json
{
  "input": {
    "device": "/dev/input/by-id/usb-flirc.tv_flirc-event-kbd",
    "mappings": {
      "KEY_UP": "menu_up",
      "KEY_DOWN": "menu_down",
      "KEY_ENTER": "menu_select",
      "KEY_ESC": "menu_back",
      "KEY_INFO": "toggle_stats"
    }
  }
}
```

**Dependencies:**
- libevdev
- libudev

---

### 3.7 Configuration Module

**Responsibilities:**
- Load/save JSON configuration
- Validate settings
- Provide runtime access to config
- Support hot-reloading for select parameters

**Key Classes:**
- `ConfigManager` - JSON serialization/deserialization
- `ConfigValidator` - Schema validation
- `ConfigWatcher` - File monitoring for hot-reload

**Configuration File Locations:**
- `/etc/ares/ares.json` - System-wide defaults
- `/var/lib/ares/runtime.json` - Runtime state
- `/var/lib/ares/presets/*.json` - User presets

**Dependencies:**
- nlohmann/json or similar JSON library

---

### 3.8 Core/Utils

**Responsibilities:**
- Centralized logging (stdout, syslog, file)
- Error handling and recovery
- Thread pool management
- Performance profiling hooks

**Key Classes:**
- `Logger` - Multi-level logging (DEBUG, INFO, WARN, ERROR)
- `ThreadPool` - Worker thread management
- `Profiler` - Frame timing and GPU profiling

**Logging Example:**
```cpp
LOG_INFO("Ares", "Starting video processor v1.0.0");
LOG_DEBUG("Capture", "DeckLink device detected: %s", device_name);
LOG_ERROR("Display", "Failed to set mode: %s", strerror(errno));
```

---

## 4. Build System (CMake)

### Directory Structure

```
Ares/
├── CMakeLists.txt
├── README.md
├── IMPLEMENTATION_PLAN.md
├── LICENSE
├── src/
│   ├── main.cpp
│   ├── capture/
│   │   ├── CMakeLists.txt
│   │   ├── decklink_capture.h
│   │   ├── decklink_capture.cpp
│   │   ├── frame_buffer.h
│   │   └── frame_buffer.cpp
│   ├── processing/
│   │   ├── CMakeLists.txt
│   │   ├── vulkan_context.h
│   │   ├── vulkan_context.cpp
│   │   ├── placebo_renderer.h
│   │   └── placebo_renderer.cpp
│   ├── display/
│   │   ├── CMakeLists.txt
│   │   ├── drm_display.h
│   │   ├── drm_display.cpp
│   │   ├── edid_parser.h
│   │   └── edid_parser.cpp
│   ├── sync/
│   │   ├── CMakeLists.txt
│   │   ├── master_clock.h
│   │   └── master_clock.cpp
│   ├── osd/
│   │   ├── CMakeLists.txt
│   │   ├── vulkan_osd.h
│   │   └── vulkan_osd.cpp
│   ├── input/
│   │   ├── CMakeLists.txt
│   │   ├── input_manager.h
│   │   └── input_manager.cpp
│   ├── config/
│   │   ├── CMakeLists.txt
│   │   ├── config_manager.h
│   │   └── config_manager.cpp
│   └── core/
│       ├── CMakeLists.txt
│       ├── logger.h
│       ├── logger.cpp
│       ├── thread_pool.h
│       └── thread_pool.cpp
├── include/
│   └── ares/
│       ├── types.h
│       └── version.h
├── shaders/
│   ├── tonemap.comp
│   ├── osd.vert
│   └── osd.frag
├── config/
│   ├── ares.json.example
│   └── presets/
│       ├── cinema.json
│       └── bright_room.json
├── systemd/
│   └── ares.service
├── scripts/
│   ├── install_dependencies.sh
│   └── setup_system.sh
├── tests/
│   ├── CMakeLists.txt
│   └── unit/
└── docs/
    ├── BUILDING.md
    ├── CONFIGURATION.md
    └── HARDWARE.md
```

### Root CMakeLists.txt

```cmake
cmake_minimum_required(VERSION 3.20)
project(Ares VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(ARES_BUILD_TESTS "Build unit tests" ON)
option(ARES_ENABLE_PROFILING "Enable performance profiling" OFF)
option(ARES_ENABLE_VALIDATION "Enable Vulkan validation layers" OFF)

# Find dependencies
find_package(Vulkan REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBPLACEBO REQUIRED libplacebo>=6.338)
pkg_check_modules(LIBDRM REQUIRED libdrm)
pkg_check_modules(LIBUDEV REQUIRED libudev)
pkg_check_modules(LIBEVDEV REQUIRED libevdev)

# DeckLink SDK (manual path)
set(DECKLINK_SDK_PATH "/usr/local/include/DeckLinkAPI" CACHE PATH "Path to DeckLink SDK")
set(DECKLINK_LIB_PATH "/usr/local/lib" CACHE PATH "Path to DeckLink libraries")

# Threads
find_package(Threads REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${Vulkan_INCLUDE_DIRS}
    ${LIBPLACEBO_INCLUDE_DIRS}
    ${LIBDRM_INCLUDE_DIRS}
    ${LIBUDEV_INCLUDE_DIRS}
    ${LIBEVDEV_INCLUDE_DIRS}
    ${DECKLINK_SDK_PATH}
)

# Compiler flags
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Werror=return-type
    -march=native
    -O3
)

if(ARES_ENABLE_PROFILING)
    add_compile_definitions(ARES_ENABLE_PROFILING)
endif()

# Subdirectories
add_subdirectory(src/core)
add_subdirectory(src/capture)
add_subdirectory(src/processing)
add_subdirectory(src/display)
add_subdirectory(src/sync)
add_subdirectory(src/osd)
add_subdirectory(src/input)
add_subdirectory(src/config)

# Main executable
add_executable(ares src/main.cpp)
target_link_libraries(ares
    ares_core
    ares_capture
    ares_processing
    ares_display
    ares_sync
    ares_osd
    ares_input
    ares_config
    ${Vulkan_LIBRARIES}
    ${LIBPLACEBO_LIBRARIES}
    ${LIBDRM_LIBRARIES}
    ${LIBUDEV_LIBRARIES}
    ${LIBEVDEV_LIBRARIES}
    ${CMAKE_DL_LIBS}
    Threads::Threads
)

# Install targets
install(TARGETS ares DESTINATION bin)
install(DIRECTORY config/ DESTINATION /etc/ares)
install(FILES systemd/ares.service DESTINATION /etc/systemd/system)

# Tests
if(ARES_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
```

---

## 5. Dependencies and Installation

### Arch Linux Package List

```bash
# Base system
base linux-rt linux-firmware networkmanager chrony

# GPU/Vulkan
nvidia nvidia-utils nvidia-dkms
vulkan-icd-loader lib32-vulkan-icd-loader
vulkan-tools vulkan-validation-layers vulkan-headers

# Video processing
libplacebo ffmpeg libdrm

# Input/Output
libevdev libudev

# Development tools
base-devel cmake git ninja pkg-config
glslang shaderc

# Optional
python python-pip
```

### DeckLink SDK Installation

```bash
# Download from Blackmagic website
wget https://[decklink-sdk-url]/Blackmagic_DeckLink_SDK_12.4.tar.gz
tar -xzf Blackmagic_DeckLink_SDK_12.4.tar.gz
cd Blackmagic_DeckLink_SDK_12.4/Linux

# Install headers
sudo mkdir -p /usr/local/include/DeckLinkAPI
sudo cp include/*.h /usr/local/include/DeckLinkAPI/

# Load kernel module
sudo modprobe decklink
```

### System Configuration

```bash
# Enable NVIDIA DRM KMS
echo "options nvidia-drm modeset=1" | sudo tee /etc/modprobe.d/nvidia.conf

# Set CPU governor to performance
sudo cpupower frequency-set -g performance

# Increase real-time priority limits
echo "@ares hard rtprio 99" | sudo tee -a /etc/security/limits.conf
echo "@ares hard nice -20" | sudo tee -a /etc/security/limits.conf

# Disable unnecessary services
sudo systemctl disable bluetooth.service
sudo systemctl disable cups.service
```

---

## 6. Implementation Phases

### Phase 1: Foundation (Week 1)
- [ ] Set up CMake build system
- [ ] Implement core utilities (Logger, ThreadPool)
- [ ] Create configuration module with JSON support
- [ ] Write systemd service file
- [ ] Create installation scripts

**Deliverable:** Minimal executable that logs startup and reads config

---

### Phase 2: DeckLink Capture (Week 2)
- [ ] Integrate DeckLink SDK
- [ ] Implement frame capture with PTS timestamps
- [ ] Create circular frame buffer
- [ ] Parse HDR metadata (HDR10, HLG)
- [ ] Handle format detection and changes
- [ ] Unit tests for capture module

**Deliverable:** Captures frames from DeckLink and logs metadata

---

### Phase 3: Vulkan/libplacebo Processing (Week 3)
- [ ] Initialize Vulkan device and queues
- [ ] Create libplacebo rendering context
- [ ] Implement basic tone mapping pipeline (BT.2390)
- [ ] Add color space conversion
- [ ] Upload captured frames to GPU
- [ ] Unit tests for processing pipeline

**Deliverable:** Processes captured frames through tone mapping

---

### Phase 4: Display Output (Week 4)
- [ ] Implement DRM/KMS output
- [ ] Parse EDID for supported modes
- [ ] Dynamic mode switching based on input
- [ ] Output processed frames to display
- [ ] Measure display latency
- [ ] EDID override support

**Deliverable:** Full capture → process → display pipeline working

---

### Phase 5: Clock Synchronization (Week 5)
- [ ] Implement master clock with CLOCK_MONOTONIC_RAW
- [ ] Frame scheduler with PTS-based timing
- [ ] Jitter measurement and compensation
- [ ] Frame drop/duplicate detection
- [ ] Adaptive latency tuning

**Deliverable:** Smooth playback with minimal jitter

---

### Phase 6: OSD and Input (Week 6)
- [ ] Vulkan overlay renderer
- [ ] Menu system (hierarchical navigation)
- [ ] Real-time statistics display
- [ ] FLIRC/evdev integration
- [ ] Key mapping configuration
- [ ] Remote control support

**Deliverable:** Interactive OSD with remote control

---

### Phase 7: Advanced Features (Week 7-8)
- [ ] Multiple tone mapping algorithms (Reinhard, Hable, custom)
- [ ] NLS (Natural Light Simulation)
- [ ] Black bar detection and cropping
- [ ] Preset system (save/load configurations)
- [ ] Hot-reload for select parameters
- [ ] Performance profiling and optimization

**Deliverable:** Feature-complete with advanced tone mapping

---

### Phase 8: Testing and Hardening (Week 9-10)
- [ ] Comprehensive unit tests
- [ ] Integration tests
- [ ] Stress testing (24-hour runs)
- [ ] Memory leak detection (valgrind)
- [ ] GPU profiling (nsys, nvprof)
- [ ] Error recovery and fallback mechanisms
- [ ] Documentation (user guide, API docs)

**Deliverable:** Production-ready system

---

## 7. Testing Strategy

### Unit Tests
- Each module has dedicated unit tests
- Mock DeckLink SDK for capture tests
- Mock Vulkan for processing tests
- Test coverage target: >80%

### Integration Tests
- Full pipeline tests with synthetic input
- EDID parsing with known-good samples
- Configuration loading/saving
- Error injection and recovery

### Performance Tests
- Frame timing accuracy (<1ms jitter)
- GPU utilization (target: <80% at 4K60)
- CPU usage (target: <30% on 8-core system)
- Memory footprint (target: <500MB)

### Hardware Tests
- Test with multiple DeckLink models
- Test with various input formats (1080p, 4K, HDR10, HLG)
- Test with different projectors/displays
- Test with FLIRC and generic IR receivers

---

## 8. Performance Targets

| Metric | Target | Notes |
|--------|--------|-------|
| Latency (capture to display) | <16ms @ 60Hz | Sub-frame latency |
| Frame drops | 0 per hour | Perfect frame delivery |
| Jitter | <1ms | Smooth motion |
| CPU usage | <30% | 8-core system |
| GPU usage | <80% | NVIDIA RTX 3060+ |
| Memory | <500MB | Resident set size |
| Startup time | <2s | From systemd start |

---

## 9. Configuration Schema

### Main Configuration File (`/etc/ares/ares.json`)

```json
{
  "version": "1.0",
  "capture": {
    "device_index": 0,
    "input_connection": "HDMI",
    "pixel_format": "10BitYUV",
    "buffer_size": 3
  },
  "processing": {
    "tone_mapping": {
      "algorithm": "bt2390",
      "target_nits": 100,
      "source_nits": 1000,
      "contrast": 1.0,
      "saturation": 1.0
    },
    "color": {
      "gamut": "bt709",
      "transfer": "gamma22"
    },
    "nls": {
      "enabled": true,
      "strength": 0.5,
      "adapt_speed": 0.3
    },
    "black_bars": {
      "enabled": true,
      "threshold": 16,
      "min_content_height": 0.5
    }
  },
  "display": {
    "connector": "HDMI-A-1",
    "edid_override": null,
    "preferred_modes": [
      {"width": 3840, "height": 2160, "refresh": 24},
      {"width": 3840, "height": 2160, "refresh": 60},
      {"width": 1920, "height": 1080, "refresh": 60}
    ],
    "refresh_rate_matching": true
  },
  "sync": {
    "target_latency_ms": 16,
    "jitter_compensation": true,
    "vsync_mode": "adaptive"
  },
  "osd": {
    "enabled": true,
    "position": "bottom_right",
    "opacity": 0.8,
    "show_stats": false
  },
  "input": {
    "device": "/dev/input/by-id/usb-flirc.tv_flirc-event-kbd",
    "mappings": {
      "KEY_UP": "menu_up",
      "KEY_DOWN": "menu_down",
      "KEY_LEFT": "menu_left",
      "KEY_RIGHT": "menu_right",
      "KEY_ENTER": "menu_select",
      "KEY_ESC": "menu_back",
      "KEY_INFO": "toggle_stats",
      "KEY_MENU": "toggle_menu"
    }
  },
  "logging": {
    "level": "INFO",
    "output": "syslog",
    "file_path": "/var/log/ares/ares.log"
  }
}
```

---

## 10. Systemd Integration

### Service File (`/etc/systemd/system/ares.service`)

```ini
[Unit]
Description=Ares HDR Video Processor
After=network.target systemd-modules-load.service
Wants=network-online.target
Requires=systemd-modules-load.service

[Service]
Type=simple
User=ares
Group=video
ExecStartPre=/usr/bin/modprobe decklink
ExecStartPre=/usr/bin/modprobe nvidia-drm
ExecStart=/usr/bin/ares --config /etc/ares/ares.json
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal

# Performance
CPUSchedulingPolicy=fifo
CPUSchedulingPriority=80
IOSchedulingClass=realtime
IOSchedulingPriority=2
LimitRTPRIO=99
LimitNICE=-20

# Security
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/lib/ares /var/log/ares
DeviceAllow=/dev/dri rw
DeviceAllow=/dev/blackmagic rw
DeviceAllow=/dev/input rw

[Install]
WantedBy=multi-user.target
```

### Installation Commands

```bash
# Create ares user
sudo useradd -r -s /bin/false -G video,input ares

# Create directories
sudo mkdir -p /etc/ares /var/lib/ares /var/log/ares
sudo chown ares:ares /var/lib/ares /var/log/ares

# Install and enable service
sudo systemctl daemon-reload
sudo systemctl enable ares.service
sudo systemctl start ares.service
```

---

## 11. Error Handling and Recovery

### Critical Errors (Restart Required)
- DeckLink device disconnected
- Vulkan device lost
- Display connector unplugged

**Action:** Log error, attempt reconnection 3 times, then exit (systemd will restart)

### Non-Critical Errors (Recoverable)
- Temporary frame drop
- Input device disconnected
- Configuration file corruption

**Action:** Log warning, use fallback/defaults, continue operation

### Watchdog
- Implement heartbeat monitoring
- If no frames processed for >5 seconds, trigger restart
- Systemd watchdog integration

---

## 12. Future Enhancements (Post-v1.0)

- [ ] Web interface for remote configuration
- [ ] Multiple input/output support
- [ ] Motion interpolation (frame blending)
- [ ] 3D LUT support
- [ ] Dolby Vision dynamic metadata support
- [ ] HDR10+ support
- [ ] Automatic calibration wizard
- [ ] Integration with home automation (Home Assistant)
- [ ] Ansible playbook for automated deployment
- [ ] Docker container support

---

## 13. Success Criteria

**Version 1.0 is complete when:**

1. ✅ Captures 4K HDR video from DeckLink with zero drops
2. ✅ Applies configurable tone mapping (BT.2390 minimum)
3. ✅ Outputs to projector with correct refresh rate
4. ✅ Sub-16ms latency at 60Hz
5. ✅ OSD functional with IR remote control
6. ✅ Systemd service runs reliably (>99.9% uptime)
7. ✅ Configuration persists across reboots
8. ✅ Documentation complete (building, configuration, troubleshooting)
9. ✅ Passes 24-hour stress test without crashes or memory leaks
10. ✅ User feedback from at least 3 beta testers

---

## 14. Resources and References

### Documentation
- [libplacebo Documentation](https://libplacebo.org/)
- [Vulkan Tutorial](https://vulkan-tutorial.com/)
- [DeckLink SDK Manual](https://www.blackmagicdesign.com/developer/product/capture-and-playback)
- [DRM/KMS Documentation](https://www.kernel.org/doc/html/latest/gpu/drm-kms.html)

### Reference Implementations
- mpv (libplacebo integration)
- OBS Studio (video capture and processing)
- Kodi (HDR passthrough)

### Standards
- ITU-R BT.2390 - HDR to SDR conversion
- ITU-R BT.2100 - HDR TV standard
- SMPTE ST 2084 - PQ EOTF
- HDMI 2.0/2.1 - HDR metadata

---

## End of Implementation Plan

**Next Steps:**
1. Review and approve this plan
2. Set up development environment
3. Begin Phase 1: Foundation
4. Establish git workflow and commit conventions
5. Schedule weekly progress reviews

**Estimated Timeline:** 10 weeks to v1.0

**Questions or modifications?** Let me know!
