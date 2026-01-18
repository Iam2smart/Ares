# Building Ares

This guide explains how to build Ares from source on Arch Linux.

## Prerequisites

### System Requirements

- **OS:** Arch Linux (headless or desktop)
- **Kernel:** Linux RT kernel recommended for lowest latency
- **CPU:** 4+ cores, x86_64 architecture
- **GPU:** NVIDIA GPU with Vulkan support (RTX 2060 or better recommended)
- **RAM:** 4GB minimum, 8GB recommended
- **Capture Card:** Blackmagic DeckLink (any model with HDMI input)

### Software Dependencies

Install all required packages:

```bash
sudo ./scripts/install_dependencies.sh
```

Or manually:

```bash
# Development tools
sudo pacman -S base-devel cmake ninja git pkg-config

# GPU and Vulkan
sudo pacman -S nvidia nvidia-utils nvidia-dkms \
    vulkan-icd-loader vulkan-tools vulkan-validation-layers \
    vulkan-headers glslang shaderc

# Video processing
sudo pacman -S libplacebo ffmpeg libdrm

# Input/output
sudo pacman -S libevdev libudev.so

# System
sudo pacman -S chrony linux-rt linux-rt-headers
```

### DeckLink SDK

Download and install the Blackmagic DeckLink SDK:

1. Download from [Blackmagic Design](https://www.blackmagicdesign.com/support/download/)
2. Extract the archive
3. Install headers:

```bash
cd Blackmagic_DeckLink_SDK_*/Linux
sudo mkdir -p /usr/local/include/DeckLinkAPI
sudo cp include/*.h /usr/local/include/DeckLinkAPI/
```

4. Load kernel module:

```bash
sudo modprobe decklink
```

5. Verify DeckLink devices:

```bash
ls -l /dev/blackmagic*
```

## Building

### Standard Build

```bash
# Create build directory
mkdir build && cd build

# Configure with CMake
cmake .. -G Ninja

# Build
ninja

# Install (requires root)
sudo ninja install
```

### Build Options

Enable/disable features during CMake configuration:

```bash
# Enable Vulkan validation layers (for debugging)
cmake .. -DARES_ENABLE_VALIDATION=ON

# Enable performance profiling
cmake .. -DARES_ENABLE_PROFILING=ON

# Disable tests
cmake .. -DARES_BUILD_TESTS=OFF

# Custom DeckLink SDK path
cmake .. -DDECKLINK_SDK_PATH=/path/to/sdk
```

### Debug Build

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug -DARES_ENABLE_VALIDATION=ON
ninja
```

### Release Build (Default)

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release
ninja
```

## Installation

After building, install Ares system-wide:

```bash
sudo ninja install
```

This will:
- Install `/usr/bin/ares` executable
- Install configuration files to `/etc/ares/`
- Install systemd service to `/etc/systemd/system/ares.service`

## System Setup

Run the system setup script:

```bash
sudo ./scripts/setup_system.sh
```

This will:
- Create the `ares` user
- Create necessary directories
- Configure CPU governor
- Load kernel modules
- Disable unnecessary services

## Running

### Manual Execution

```bash
# Run with default config
sudo ares

# Run with custom config
sudo ares --config /path/to/config.json

# Validate configuration
ares --validate-config --config /etc/ares/ares.json
```

### Systemd Service

```bash
# Enable auto-start on boot
sudo systemctl enable ares.service

# Start service now
sudo systemctl start ares.service

# Check status
sudo systemctl status ares.service

# View logs
sudo journalctl -u ares.service -f
```

## Troubleshooting

### DeckLink Not Detected

```bash
# Check if module is loaded
lsmod | grep decklink

# Load module manually
sudo modprobe decklink

# Check device nodes
ls -l /dev/blackmagic*

# Check dmesg for errors
dmesg | grep -i decklink
```

### Vulkan Errors

```bash
# Verify Vulkan installation
vulkaninfo

# Check NVIDIA driver
nvidia-smi

# Verify DRM KMS mode
cat /sys/module/nvidia_drm/parameters/modeset
# Should output: Y
```

### Permission Errors

Ensure the `ares` user is in the correct groups:

```bash
sudo usermod -aG video,input ares
```

### Build Errors

If you get linking errors, ensure all dependencies are installed:

```bash
# Check pkg-config
pkg-config --modversion libplacebo
pkg-config --modversion libdrm
pkg-config --modversion libevdev
```

## Development

### Compile Commands

For IDE integration, CMake generates `compile_commands.json`:

```bash
cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

### Running Tests

```bash
# Build with tests enabled (default)
cmake .. -DARES_BUILD_TESTS=ON
ninja

# Run all tests
ctest --output-on-failure

# Run specific test
./tests/unit/test_capture
```

### Code Formatting

```bash
# Format all C++ files
find src include -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```

## Next Steps

- Read [CONFIGURATION.md](CONFIGURATION.md) for configuration options
- Read [HARDWARE.md](HARDWARE.md) for hardware compatibility
- Check the [IMPLEMENTATION_PLAN.md](../IMPLEMENTATION_PLAN.md) for development roadmap
