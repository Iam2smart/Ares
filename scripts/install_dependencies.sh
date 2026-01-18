#!/bin/bash
set -e

echo "==================================="
echo "Ares Dependency Installation Script"
echo "==================================="
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Please run as root (sudo)"
    exit 1
fi

# Detect distribution
if [ -f /etc/arch-release ]; then
    DISTRO="arch"
elif [ -f /etc/debian_version ]; then
    DISTRO="debian"
else
    echo "Unsupported distribution. This script is designed for Arch Linux."
    exit 1
fi

echo "Detected distribution: $DISTRO"
echo ""

if [ "$DISTRO" = "arch" ]; then
    echo "Installing Arch Linux packages..."

    # Update package database
    pacman -Syu --noconfirm

    # Base development tools
    pacman -S --needed --noconfirm \
        base-devel \
        cmake \
        ninja \
        git \
        pkg-config

    # GPU and Vulkan
    pacman -S --needed --noconfirm \
        nvidia \
        nvidia-utils \
        nvidia-dkms \
        vulkan-icd-loader \
        lib32-vulkan-icd-loader \
        vulkan-tools \
        vulkan-validation-layers \
        vulkan-headers \
        glslang \
        shaderc

    # Video processing libraries
    pacman -S --needed --noconfirm \
        libplacebo \
        ffmpeg \
        libdrm

    # Input/output libraries
    pacman -S --needed --noconfirm \
        libevdev \
        libudev.so

    # System utilities
    pacman -S --needed --noconfirm \
        chrony \
        linux-rt \
        linux-rt-headers

    echo ""
    echo "Package installation complete!"

elif [ "$DISTRO" = "debian" ]; then
    echo "Debian/Ubuntu installation not yet implemented."
    echo "Please install packages manually."
    exit 1
fi

# Enable NVIDIA DRM KMS mode
echo "Configuring NVIDIA DRM KMS..."
echo "options nvidia-drm modeset=1" > /etc/modprobe.d/nvidia.conf

# Configure real-time priority limits
echo "Configuring real-time priority limits..."
if ! grep -q "@ares" /etc/security/limits.conf; then
    echo "@ares hard rtprio 99" >> /etc/security/limits.conf
    echo "@ares hard nice -20" >> /etc/security/limits.conf
fi

echo ""
echo "==================================="
echo "Dependencies installed successfully!"
echo ""
echo "Next steps:"
echo "1. Reboot to load the RT kernel and NVIDIA modules"
echo "2. Install Blackmagic DeckLink SDK manually"
echo "3. Run ./scripts/setup_system.sh to configure the system"
echo "==================================="
