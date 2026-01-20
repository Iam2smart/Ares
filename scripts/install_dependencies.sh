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
elif [ -f /etc/lsb-release ]; then
    . /etc/lsb-release
    if [ "$DISTRIB_ID" = "Ubuntu" ]; then
        DISTRO="ubuntu"
        UBUNTU_VERSION="$DISTRIB_RELEASE"
    else
        DISTRO="debian"
    fi
elif [ -f /etc/debian_version ]; then
    DISTRO="debian"
else
    echo "Unsupported distribution. This script supports Ubuntu 22.04/24.04 LTS and Arch Linux."
    exit 1
fi

echo "Detected distribution: $DISTRO"
if [ "$DISTRO" = "ubuntu" ]; then
    echo "Ubuntu version: $UBUNTU_VERSION"
fi
echo ""

if [ "$DISTRO" = "ubuntu" ] || [ "$DISTRO" = "debian" ]; then
    echo "Installing Ubuntu/Debian packages..."

    # Update package database
    apt update

    # Base development tools
    echo "Installing build tools..."
    apt install -y \
        build-essential \
        cmake \
        ninja-build \
        git \
        pkg-config \
        software-properties-common \
        wget \
        curl

    # Add graphics PPA for latest drivers (Ubuntu only)
    if [ "$DISTRO" = "ubuntu" ]; then
        echo "Adding graphics drivers PPA..."
        add-apt-repository -y ppa:graphics-drivers/ppa
        apt update
    fi

    # GPU and Vulkan
    echo "Installing GPU and Vulkan packages..."
    apt install -y \
        nvidia-driver-550 \
        nvidia-utils-550 \
        nvidia-dkms-550 \
        vulkan-tools \
        vulkan-validationlayers \
        libvulkan-dev \
        mesa-vulkan-drivers

    # Video processing libraries
    echo "Installing video processing libraries..."
    apt install -y \
        libplacebo-dev \
        libplacebo208 \
        ffmpeg \
        libavcodec-dev \
        libavformat-dev \
        libavutil-dev \
        libswscale-dev \
        libswresample-dev \
        libdrm-dev \
        libgbm-dev

    # Input/output libraries
    echo "Installing input/output libraries..."
    apt install -y \
        libevdev-dev \
        libudev-dev \
        libinput-dev \
        libegl1-mesa-dev \
        libgles2-mesa-dev

    # OSD rendering libraries
    echo "Installing OSD rendering libraries..."
    apt install -y \
        libcairo2-dev \
        libpango1.0-dev \
        libpangocairo-1.0-0

    # System utilities
    echo "Installing system utilities..."
    apt install -y \
        chrony \
        openssh-server \
        cpufrequtils \
        linux-tools-generic \
        linux-cloud-tools-generic

    # Optional: Install low-latency kernel (not RT, but better than generic)
    echo ""
    read -p "Install low-latency kernel? (Recommended for real-time performance) [y/N]: " -n 1 -r
    echo ""
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        echo "Installing low-latency kernel..."
        apt install -y linux-lowlatency linux-headers-lowlatency
        echo "Low-latency kernel installed. Select it in GRUB on next boot."
    fi

    echo ""
    echo "Package installation complete!"

elif [ "$DISTRO" = "arch" ]; then
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

    # OSD rendering libraries
    pacman -S --needed --noconfirm \
        cairo \
        pango

    # System utilities
    pacman -S --needed --noconfirm \
        chrony \
        linux-rt \
        linux-rt-headers \
        openssh

    echo ""
    echo "Package installation complete!"
fi

# Enable NVIDIA DRM KMS mode
echo ""
echo "Configuring NVIDIA DRM KMS..."
mkdir -p /etc/modprobe.d
echo "options nvidia-drm modeset=1" > /etc/modprobe.d/nvidia.conf
echo "NVIDIA KMS enabled."

# Update initramfs
if [ "$DISTRO" = "ubuntu" ] || [ "$DISTRO" = "debian" ]; then
    echo "Updating initramfs..."
    update-initramfs -u
elif [ "$DISTRO" = "arch" ]; then
    echo "Updating mkinitcpio..."
    mkinitcpio -P
fi

# Configure real-time priority limits
echo ""
echo "Configuring real-time priority limits..."
if ! grep -q "@ares" /etc/security/limits.conf; then
    echo "@ares hard rtprio 99" >> /etc/security/limits.conf
    echo "@ares hard nice -20" >> /etc/security/limits.conf
    echo "@ares soft memlock unlimited" >> /etc/security/limits.conf
    echo "@ares hard memlock unlimited" >> /etc/security/limits.conf
    echo "Real-time limits configured."
else
    echo "Real-time limits already configured."
fi

# Enable SSH service
echo ""
echo "Enabling SSH service..."
if [ "$DISTRO" = "ubuntu" ] || [ "$DISTRO" = "debian" ]; then
    systemctl enable ssh.service
    systemctl start ssh.service || true
else
    systemctl enable sshd.service
    systemctl start sshd.service || true
fi

echo ""
echo "==================================="
echo "Dependencies installed successfully!"
echo ""
echo "Next steps:"
echo "1. Reboot to load the new kernel and NVIDIA modules"
echo "   $ sudo reboot"
echo ""
echo "2. After reboot, verify NVIDIA driver:"
echo "   $ nvidia-smi"
echo ""
echo "3. Install Blackmagic DeckLink Desktop Video:"
echo "   - Download from: https://www.blackmagicdesign.com/support/download/"
echo "   - Install: sudo dpkg -i desktopvideo_*.deb"
echo "   - Install GUI: sudo dpkg -i desktopvideo-gui_*.deb"
echo ""
echo "4. Run system setup script:"
echo "   $ sudo ./scripts/setup_system.sh"
echo ""
echo "5. Build Ares:"
echo "   $ mkdir build && cd build"
echo "   $ cmake .. -G Ninja"
echo "   $ ninja"
echo "==================================="
