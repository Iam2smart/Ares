# Building Ares

This guide explains how to build Ares from source on Ubuntu LTS.

## Prerequisites

### System Requirements

- **OS:** Ubuntu 22.04 LTS or 24.04 LTS (recommended)
  - **Note:** Arch Linux is also supported but Ubuntu is recommended for DeckLink compatibility
- **Kernel:** Low-latency kernel recommended for best performance
- **CPU:** 4+ cores, x86_64 architecture
- **GPU:** NVIDIA GPU with Vulkan support (RTX 2060 or better recommended)
- **RAM:** 4GB minimum, 8GB recommended
- **Capture Card:** Blackmagic DeckLink (any model with HDMI input)

### Software Dependencies

The easiest way to install all dependencies is using the provided script:

```bash
sudo ./scripts/install_dependencies.sh
```

This will:
- Install build tools (gcc, cmake, ninja, pkg-config)
- Install NVIDIA drivers (version 550)
- Install Vulkan development libraries
- Install video processing libraries (libplacebo, FFmpeg)
- Install OSD libraries (Cairo, Pango)
- Configure NVIDIA DRM KMS mode
- Optionally install low-latency kernel

Or manually install Ubuntu packages:

```bash
# Development tools
sudo apt install build-essential cmake ninja-build git pkg-config

# NVIDIA drivers and Vulkan
sudo add-apt-repository ppa:graphics-drivers/ppa
sudo apt update
sudo apt install nvidia-driver-550 nvidia-utils-550 nvidia-dkms-550 \
    vulkan-tools libvulkan-dev mesa-vulkan-drivers

# Video processing
sudo apt install libplacebo-dev ffmpeg \
    libavcodec-dev libavformat-dev libavutil-dev \
    libswscale-dev libdrm-dev libgbm-dev

# Input/output
sudo apt install libevdev-dev libudev-dev libinput-dev

# OSD rendering
sudo apt install libcairo2-dev libpango1.0-dev

# System utilities
sudo apt install chrony openssh-server cpufrequtils
```

### DeckLink SDK

Download and install Blackmagic Desktop Video for Ubuntu:

1. Download from [Blackmagic Design Support](https://www.blackmagicdesign.com/support/download/)
   - Look for "Desktop Video" for Linux
   - Get version 12.8 or later

2. Install the .deb packages:

```bash
# Install drivers
sudo dpkg -i desktopvideo_12.8a3_amd64.deb

# Install GUI tools (optional but helpful for testing)
sudo dpkg -i desktopvideo-gui_12.8a3_amd64.deb

# Fix any dependency issues
sudo apt --fix-broken install
```

3. Reboot to load the kernel modules:

```bash
sudo reboot
```

4. After reboot, verify installation:

```bash
# Check if card is detected
lspci | grep -i blackmagic

# Check for device nodes
ls -l /dev/blackmagic*

# Should show something like:
# /dev/blackmagic/dv0
# /dev/blackmagic/io0
```

## Building

### Quick Build

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
cmake .. -DARES_ENABLE_VALIDATION=ON -G Ninja

# Enable performance profiling
cmake .. -DARES_ENABLE_PROFILING=ON -G Ninja

# Disable tests
cmake .. -DARES_BUILD_TESTS=OFF -G Ninja

# Custom DeckLink SDK path
cmake .. -DDECKLINK_SDK_PATH=/path/to/sdk -G Ninja
```

### Debug Build

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug -DARES_ENABLE_VALIDATION=ON -G Ninja
ninja
```

### Release Build (Default)

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -G Ninja
ninja
```

## Installation

After building, install Ares system-wide:

```bash
sudo ninja install
```

This will:
- Install `/usr/local/bin/ares` executable
- Install configuration files to `/etc/ares/`
- Install headers to `/usr/local/include/ares/`
- Install libraries to `/usr/local/lib/`

## System Setup

Run the system setup script to configure your system:

```bash
sudo ./scripts/setup_system.sh
```

This will:
- Create the `ares` user with proper groups (video, input, render)
- Create necessary directories (`/etc/ares`, `/var/lib/ares`, `/var/log/ares`)
- Configure CPU governor for performance
- Check for NVIDIA GPU and DeckLink card
- Enable SSH for remote access
- Disable unnecessary services (bluetooth, cups)
- Create systemd service template

## Running

### Manual Execution

```bash
# Run with default config
sudo ares

# Run with custom config
sudo ares --config /path/to/config.json

# Validate configuration
ares --validate-config --config /etc/ares/ares.json

# Run in daemon mode (suppress console output)
sudo ares --daemon
```

### Systemd Service

```bash
# Copy service file
sudo cp /tmp/ares.service.template /etc/systemd/system/ares.service

# Reload systemd
sudo systemctl daemon-reload

# Enable auto-start on boot
sudo systemctl enable ares.service

# Start service now
sudo systemctl start ares.service

# Check status
sudo systemctl status ares.service

# View logs (follow mode)
sudo journalctl -u ares.service -f

# View logs (last 100 lines)
sudo journalctl -u ares.service -n 100
```

## Troubleshooting

### DeckLink Not Detected

```bash
# Check if card is physically detected
lspci | grep -i blackmagic
# Should output: "Blackmagic Design DeckLink..."

# Check if module is loaded
lsmod | grep blackmagic

# Load module manually if needed
sudo modprobe blackmagic

# Check device nodes
ls -l /dev/blackmagic*

# Check dmesg for errors
dmesg | grep -i decklink

# If no device nodes, reinstall Desktop Video
sudo dpkg -i desktopvideo_*.deb
sudo reboot
```

### Vulkan Errors

```bash
# Verify Vulkan installation
vulkaninfo | head -20

# Check NVIDIA driver
nvidia-smi

# Verify DRM KMS mode is enabled
cat /sys/module/nvidia_drm/parameters/modeset
# Should output: Y

# If not enabled:
echo 'options nvidia-drm modeset=1' | sudo tee /etc/modprobe.d/nvidia.conf
sudo update-initramfs -u
sudo reboot
```

### Permission Errors

Ensure the `ares` user has access to required devices:

```bash
# Add user to groups
sudo usermod -aG video,input,render ares

# Check group membership
groups ares

# Check device permissions
ls -l /dev/dri/
ls -l /dev/input/
ls -l /dev/blackmagic/
```

### Build Errors

If you get linking errors or missing dependencies:

```bash
# Check pkg-config
pkg-config --modversion libplacebo
pkg-config --modversion libdrm
pkg-config --modversion libevdev
pkg-config --modversion cairo
pkg-config --modversion pango

# If any are missing, install them:
sudo apt install libplacebo-dev libdrm-dev libevdev-dev libcairo2-dev libpango1.0-dev

# Clean and rebuild
cd build
rm -rf *
cmake .. -G Ninja
ninja
```

### NVIDIA Driver Issues

```bash
# Check if driver is loaded
nvidia-smi

# If not loaded, check installation
dpkg -l | grep nvidia-driver

# Reinstall if needed
sudo apt remove --purge nvidia-*
sudo apt autoremove
sudo add-apt-repository ppa:graphics-drivers/ppa
sudo apt update
sudo apt install nvidia-driver-550
sudo reboot
```

### Performance Issues

If GPU frame time is >16ms:

```bash
# Check CPU governor
cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor
# Should all say "performance"

# Set to performance if not
for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    echo "performance" | sudo tee $cpu
done

# Check GPU clocks
nvidia-smi -q -d CLOCK

# Enable persistence mode
sudo nvidia-smi -pm 1

# Check for thermal throttling
nvidia-smi -q -d TEMPERATURE
```

## Development

### Compile Commands

For IDE integration (VSCode, CLion, etc.):

```bash
cmake .. -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -G Ninja
# Use build/compile_commands.json in your IDE
```

### Running Tests

```bash
# Build with tests enabled (default)
cmake .. -DARES_BUILD_TESTS=ON -G Ninja
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

### Debugging

```bash
# Build debug version
cmake .. -DCMAKE_BUILD_TYPE=Debug -DARES_ENABLE_VALIDATION=ON -G Ninja
ninja

# Run with GDB
sudo gdb --args ./ares --config /etc/ares/ares.json

# Run with Valgrind (memory leaks)
sudo valgrind --leak-check=full ./ares --config /etc/ares/ares.json
```

## Testing on USB Drive

For testing before full installation:

1. **Install Ubuntu 24.04 LTS on USB** (64GB+ USB 3.1 recommended)
2. **Boot from USB** on your theater PC
3. **Run installation scripts** as described above
4. **Test with real hardware** (DeckLink, projector, IR remote)
5. **Clone to SSD** once verified working

See [README.md](../README.md) for more details on USB testing.

## Next Steps

- Read [CONFIGURATION.md](CONFIGURATION.md) for configuration options
- Read [HARDWARE_SETUP_STATUS.md](HARDWARE_SETUP_STATUS.md) for hardware requirements
- Check [FEATURE_COMPARISON.md](FEATURE_COMPARISON.md) for feature comparison with madVR
- Read [REMOTE_ACCESS.md](REMOTE_ACCESS.md) for remote management setup
