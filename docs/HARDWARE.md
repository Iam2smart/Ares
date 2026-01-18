# Hardware Compatibility Guide

This document lists tested and recommended hardware for Ares.

## Minimum Requirements

### CPU
- **Architecture:** x86_64
- **Cores:** 4 physical cores
- **Speed:** 2.5 GHz or higher
- **Recommended:** 6+ cores with SMT disabled for real-time performance

### GPU
- **Vendor:** NVIDIA only (AMD support planned)
- **Minimum:** GTX 1660 Ti or equivalent
- **Recommended:** RTX 3060 or better
- **Requirements:**
  - Vulkan 1.3 support
  - 4GB+ VRAM
  - HDMI 2.0 or DisplayPort 1.4 output

### Memory
- **Minimum:** 4GB RAM
- **Recommended:** 8GB+ RAM
- **Type:** DDR4-2400 or faster

### Storage
- **Minimum:** 100MB for installation
- **Recommended:** SSD for system drive (logging and config)

## Capture Cards

### Blackmagic Design DeckLink

All DeckLink cards with HDMI or SDI input are supported.

#### Tested Models

| Model | Input | Max Resolution | HDR Support | Notes |
|-------|-------|----------------|-------------|-------|
| DeckLink Mini Monitor 4K | HDMI 2.0 | 4K60 | HDR10 | Budget option |
| DeckLink 4K Extreme 12G | HDMI 2.0 / 12G-SDI | 4K60 | HDR10, HLG | Professional |
| DeckLink 8K Pro | HDMI 2.0 / Quad 12G-SDI | 8K30 | HDR10, HLG | High-end |
| DeckLink Duo 2 | HDMI 2.0 (x2) | 4K60 per input | HDR10 | Dual input |

#### Driver Installation

```bash
# Download DeckLink SDK from Blackmagic website
# Extract and load kernel module
sudo modprobe decklink

# Verify device detection
ls -l /dev/blackmagic*
```

#### Unsupported Devices

- Intensity series (designed for analog capture)
- UltraStudio series (Thunderbolt-based, driver compatibility issues)

## Display Devices

### Projectors

Tested with various 4K HDR projectors. Requirements:
- HDMI 2.0 input
- HDR10 or HLG support
- 1080p, 1440p, or 4K resolution
- 24 Hz, 50 Hz, 60 Hz, or higher refresh rate

#### Tested Projectors

| Brand | Model | Resolution | Max Nits | Notes |
|-------|-------|------------|----------|-------|
| Sony | VPL-VW915ES | 4K | 150 | Excellent HDR |
| JVC | DLA-NZ9 | 4K | 100 | Laser light source |
| Epson | LS12000 | 4K | 120 | Bright room capable |
| BenQ | X3000i | 4K | 150 | Gaming optimized |

### TVs

Any 4K HDR TV with HDMI 2.0 input should work. Ensure:
- "Game Mode" or "PC Mode" enabled for low latency
- HDR support enabled in TV settings
- Correct EDID detection (or use EDID override)

## Input Devices

### IR Receivers

#### FLIRC USB (Recommended)

- **Model:** FLIRC USB (any version)
- **Connection:** USB 2.0/3.0
- **Setup:**
  1. Plug FLIRC into USB port
  2. Program with your IR remote using FLIRC app
  3. Device appears as keyboard at `/dev/input/by-id/usb-flirc.tv_flirc-event-kbd`

#### Generic MCE IR Receivers

Most Media Center Edition (MCE) IR receivers work:
- Microsoft MCE Remote
- Any MCE-compatible USB IR receiver

### Keyboards

Any USB or Bluetooth keyboard can be used for OSD control.

## Network Hardware

### Ethernet (Optional)

While Ares runs headless and doesn't require network access during operation, network connectivity is useful for:
- Remote configuration (SSH)
- Log monitoring
- System updates

**Recommended:** 1 Gbps Ethernet for reliable SSH access

## System Configuration

### BIOS/UEFI Settings

Recommended BIOS settings for optimal performance:

#### CPU
- **Hyperthreading/SMT:** Disabled (for RT performance)
- **CPU Power Management:** Disabled or set to "Maximum Performance"
- **Turbo Boost:** Enabled
- **Intel SpeedStep / AMD Cool'n'Quiet:** Disabled

#### GPU
- **Primary Display:** Set to PCIe graphics (not iGPU)
- **Above 4G Decoding:** Enabled (for large VRAM)
- **Re-Size BAR:** Enabled (NVIDIA RTX 3000+)

#### Power
- **Power Profile:** High Performance
- **ASPM:** Disabled
- **USB Power Management:** Disabled

#### Boot
- **Fast Boot:** Disabled
- **Secure Boot:** Disabled (for RT kernel)

### Kernel

#### Real-Time Kernel (Recommended)

```bash
# Install RT kernel
sudo pacman -S linux-rt linux-rt-headers

# Select RT kernel in GRUB
# Edit /etc/default/grub:
GRUB_DEFAULT="Advanced options for Arch Linux>Arch Linux, with Linux linux-rt"

# Regenerate GRUB config
sudo grub-mkconfig -o /boot/grub/grub.cfg

# Reboot
sudo reboot
```

#### Kernel Parameters

Add these to `/etc/default/grub` in `GRUB_CMDLINE_LINUX`:

```
nvidia-drm.modeset=1 isolcpus=2,3 nohz_full=2,3 rcu_nocbs=2,3
```

Explanation:
- `nvidia-drm.modeset=1` - Enable NVIDIA KMS
- `isolcpus=2,3` - Isolate CPUs 2-3 for Ares (adjust for your CPU count)
- `nohz_full=2,3` - Disable timer ticks on isolated CPUs
- `rcu_nocbs=2,3` - Move RCU callbacks off isolated CPUs

### PCIe Slot Placement

For best performance:
- Install GPU in the top PCIe x16 slot (usually PCIe 4.0/5.0)
- Install DeckLink in a PCIe x4 or x8 slot (avoid sharing lanes with GPU)

### Cooling

Ensure adequate cooling:
- GPU temperature should stay below 80°C under load
- CPU temperature should stay below 70°C

## Power Supply

### Requirements

- **Minimum:** 500W 80+ Bronze
- **Recommended:** 650W 80+ Gold

### Power Calculation

Example system:
- CPU: 125W (AMD Ryzen 7)
- GPU: 220W (RTX 3070)
- DeckLink: 25W
- Other: 50W
- **Total:** ~420W (650W PSU recommended)

## Tested Configurations

### Budget Configuration ($1200)

- **CPU:** AMD Ryzen 5 5600
- **GPU:** NVIDIA RTX 3060 12GB
- **RAM:** 16GB DDR4-3200
- **Motherboard:** ASUS TUF B550M
- **Capture:** DeckLink Mini Monitor 4K
- **PSU:** 650W 80+ Gold
- **Case:** Fractal Design Define 7 Compact

**Performance:** 4K60 HDR with sub-20ms latency

### Mid-Range Configuration ($2000)

- **CPU:** AMD Ryzen 7 5800X3D
- **GPU:** NVIDIA RTX 4070
- **RAM:** 32GB DDR4-3600
- **Motherboard:** MSI B550 Tomahawk
- **Capture:** DeckLink 4K Extreme 12G
- **PSU:** 750W 80+ Gold
- **Case:** Fractal Design Define 7

**Performance:** 4K60 HDR with sub-16ms latency, headroom for future features

### High-End Configuration ($3500)

- **CPU:** AMD Ryzen 9 7950X
- **GPU:** NVIDIA RTX 4080
- **RAM:** 64GB DDR5-5600
- **Motherboard:** ASUS ROG Strix X670E
- **Capture:** DeckLink 8K Pro
- **PSU:** 1000W 80+ Platinum
- **Case:** Fractal Design Define 7 XL

**Performance:** 4K120 HDR with sub-12ms latency, 8K30 capable

## Troubleshooting

### GPU Not Detected

```bash
# Check if NVIDIA driver is loaded
nvidia-smi

# Check Vulkan
vulkaninfo | grep deviceName

# Reload driver
sudo modprobe -r nvidia_drm nvidia_modeset nvidia
sudo modprobe nvidia nvidia_modeset nvidia_drm modeset=1
```

### DeckLink Not Detected

```bash
# Check if decklink module is loaded
lsmod | grep decklink

# Load module
sudo modprobe decklink

# Check device nodes
ls -l /dev/blackmagic*

# Check PCIe device
lspci | grep -i blackmagic
```

### FLIRC Not Detected

```bash
# List input devices
ls -l /dev/input/by-id/ | grep flirc

# Check permissions
groups ares | grep input

# If not in input group:
sudo usermod -aG input ares
```

## Future Hardware Support

Planned support for:
- AMD GPUs (via RADV Vulkan driver)
- Intel Arc GPUs (experimental)
- AJA Video Systems capture cards
- Magewell capture cards

## Recommendations

### Best Value
- **GPU:** RTX 3060 12GB (~$300)
- **Capture:** DeckLink Mini Monitor 4K (~$145)

### Best Performance
- **GPU:** RTX 4070 or better
- **Capture:** DeckLink 4K Extreme 12G (~$595)

### For Gaming (Low Latency)
- **CPU:** High single-thread performance (AMD X3D series or Intel Core i7/i9)
- **GPU:** RTX 4070 or better
- **RT Kernel:** Essential for sub-10ms latency

### For Cinema (Image Quality)
- **GPU:** RTX 4080 or better (for complex tone mapping)
- **Capture:** DeckLink 4K Extreme 12G or 8K Pro (for 12-bit color)
- **Projector:** High-end 4K HDR projector (Sony, JVC)

## Next Steps

- Read [BUILDING.md](BUILDING.md) for build instructions
- Read [CONFIGURATION.md](CONFIGURATION.md) for configuration options
- Check the [IMPLEMENTATION_PLAN.md](../IMPLEMENTATION_PLAN.md) for development roadmap
