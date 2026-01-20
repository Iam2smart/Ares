#!/bin/bash
set -e

echo "==================================="
echo "Ares System Setup Script"
echo "==================================="
echo ""

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Please run as root (sudo)"
    exit 1
fi

# Detect distribution
if [ -f /etc/lsb-release ]; then
    . /etc/lsb-release
    DISTRO="$DISTRIB_ID"
elif [ -f /etc/arch-release ]; then
    DISTRO="Arch"
else
    DISTRO="Unknown"
fi

echo "Detected distribution: $DISTRO"
echo ""

# Create ares user if it doesn't exist
if ! id -u ares > /dev/null 2>&1; then
    echo "Creating ares user..."
    useradd -r -s /bin/false -G video,input,render ares
    echo "User 'ares' created."
else
    echo "User 'ares' already exists."
    # Ensure user is in correct groups
    echo "Adding ares user to required groups..."
    usermod -aG video,input,render ares
fi

# Create necessary directories
echo "Creating directories..."
mkdir -p /etc/ares/presets
mkdir -p /var/lib/ares
mkdir -p /var/log/ares

# Set ownership
chown -R ares:ares /var/lib/ares
chown -R ares:ares /var/log/ares
chmod 755 /etc/ares
chmod 755 /var/lib/ares
chmod 755 /var/log/ares

echo "Directories created."

# Copy example configuration if it doesn't exist
if [ ! -f /etc/ares/ares.json ]; then
    if [ -f ../config/ares.json.example ]; then
        echo "Installing default configuration..."
        cp ../config/ares.json.example /etc/ares/ares.json
        if [ -d ../config/presets ]; then
            cp -r ../config/presets/* /etc/ares/presets/ 2>/dev/null || true
        fi
        chmod 644 /etc/ares/ares.json
        echo "Configuration installed."
    else
        echo "Warning: Example configuration not found."
    fi
else
    echo "Configuration already exists at /etc/ares/ares.json"
fi

# Set CPU governor to performance
echo "Setting CPU governor to performance..."
if command -v cpupower &> /dev/null; then
    cpupower frequency-set -g performance 2>/dev/null || true
    echo "CPU governor set to performance."
elif [ -f /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor ]; then
    # Fallback method for Ubuntu
    for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
        echo "performance" > "$cpu" 2>/dev/null || true
    done
    echo "CPU governor set to performance (fallback method)."
else
    echo "Warning: Could not set CPU governor. Install 'linux-tools-generic' or 'cpufrequtils'."
fi

# Load kernel modules
echo "Loading kernel modules..."
modprobe nvidia-drm modeset=1 2>/dev/null || echo "Warning: nvidia-drm module not loaded (NVIDIA driver may not be installed)"

# Check for DeckLink devices
if command -v lspci &> /dev/null; then
    echo "Checking for DeckLink devices..."
    if lspci | grep -i blackmagic > /dev/null; then
        echo "✓ DeckLink capture card detected"

        # Try to load blackmagic module if available
        if modprobe blackmagic 2>/dev/null; then
            echo "✓ DeckLink module loaded"
        fi

        # Check for device nodes
        if ls /dev/blackmagic* 1> /dev/null 2>&1; then
            echo "✓ DeckLink device nodes found:"
            ls -l /dev/blackmagic*
        else
            echo "⚠ No DeckLink device nodes found."
            echo "  Install Desktop Video: sudo dpkg -i desktopvideo_*.deb"
        fi
    else
        echo "⚠ No DeckLink capture card detected"
        echo "  Make sure the DeckLink card is properly seated in PCIe slot"
    fi
fi

# Check NVIDIA GPU
echo ""
echo "Checking NVIDIA GPU..."
if command -v nvidia-smi &> /dev/null; then
    if nvidia-smi > /dev/null 2>&1; then
        echo "✓ NVIDIA driver loaded successfully"
        nvidia-smi --query-gpu=name,driver_version --format=csv,noheader

        # Check DRM KMS mode
        if [ -f /sys/module/nvidia_drm/parameters/modeset ]; then
            MODESET=$(cat /sys/module/nvidia_drm/parameters/modeset)
            if [ "$MODESET" = "Y" ]; then
                echo "✓ NVIDIA DRM KMS mode enabled"
            else
                echo "⚠ NVIDIA DRM KMS mode NOT enabled"
                echo "  Run: echo 'options nvidia-drm modeset=1' | sudo tee /etc/modprobe.d/nvidia.conf"
                echo "  Then: sudo update-initramfs -u && sudo reboot"
            fi
        fi
    else
        echo "⚠ NVIDIA driver installed but not loaded"
        echo "  Reboot may be required"
    fi
else
    echo "⚠ NVIDIA driver not found"
    echo "  Install: sudo apt install nvidia-driver-550"
fi

# Enable SSH for remote access
echo ""
echo "Enabling SSH service..."
if systemctl is-active --quiet ssh.service; then
    echo "✓ SSH service already running"
elif systemctl is-active --quiet sshd.service; then
    echo "✓ SSH service already running"
else
    # Try both ssh and sshd service names (Ubuntu vs Arch)
    systemctl enable ssh.service 2>/dev/null || systemctl enable sshd.service 2>/dev/null || true
    systemctl start ssh.service 2>/dev/null || systemctl start sshd.service 2>/dev/null || true
    echo "✓ SSH service enabled"
fi

# Get IP address for SSH instructions
IP_ADDR=$(hostname -I | awk '{print $1}')
if [ -n "$IP_ADDR" ]; then
    echo ""
    echo "You can connect from your laptop using:"
    echo "  ssh $(whoami)@$IP_ADDR"
    echo ""
    echo "For improved security, consider:"
    echo "  1. Setting up SSH key authentication"
    echo "  2. Disabling password authentication in /etc/ssh/sshd_config"
    echo "  3. Restricting SSH access to specific IPs"
fi

# Disable unnecessary services
echo ""
echo "Disabling unnecessary services..."
systemctl disable bluetooth.service 2>/dev/null || true
systemctl stop bluetooth.service 2>/dev/null || true
systemctl disable cups.service 2>/dev/null || true
systemctl stop cups.service 2>/dev/null || true
systemctl disable avahi-daemon.service 2>/dev/null || true
systemctl stop avahi-daemon.service 2>/dev/null || true
echo "✓ Unnecessary services disabled"

# Configure automatic time sync
echo ""
echo "Configuring time synchronization..."
if systemctl is-active --quiet chronyd.service; then
    echo "✓ Chrony time sync already running"
elif systemctl is-active --quiet chrony.service; then
    echo "✓ Chrony time sync already running"
else
    systemctl enable chrony.service 2>/dev/null || systemctl enable chronyd.service 2>/dev/null || true
    systemctl start chrony.service 2>/dev/null || systemctl start chronyd.service 2>/dev/null || true
    echo "✓ Time sync enabled"
fi

# Create systemd service file template
echo ""
echo "Creating systemd service template..."
cat > /tmp/ares.service.template << 'EOF'
[Unit]
Description=Ares HDR Video Processor
After=network.target graphical.target

[Service]
Type=simple
User=ares
Group=ares
ExecStart=/usr/local/bin/ares --config /etc/ares/ares.json
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal

# Performance settings
CPUSchedulingPolicy=fifo
CPUSchedulingPriority=80
Nice=-15
LimitRTPRIO=99
LimitMEMLOCK=infinity

# Security settings (optional, may need adjustment)
NoNewPrivileges=false
PrivateTmp=true

[Install]
WantedBy=multi-user.target
EOF

echo "✓ Service template created at /tmp/ares.service.template"
echo "  After building Ares, copy it to: /etc/systemd/system/ares.service"

echo ""
echo "==================================="
echo "System setup complete!"
echo ""
echo "✓ Configuration Summary:"
echo "  - User 'ares' created with video, input, render groups"
echo "  - Directories created in /etc/ares, /var/lib/ares, /var/log/ares"
echo "  - CPU governor set to performance"
echo "  - SSH service enabled"
echo "  - Unnecessary services disabled"
echo ""
echo "Next steps:"
echo ""
echo "1. Build Ares:"
echo "   $ cd /home/user/Ares"
echo "   $ mkdir build && cd build"
echo "   $ cmake .. -G Ninja"
echo "   $ ninja"
echo ""
echo "2. Install Ares:"
echo "   $ sudo ninja install"
echo ""
echo "3. Copy systemd service:"
echo "   $ sudo cp /tmp/ares.service.template /etc/systemd/system/ares.service"
echo "   $ sudo systemctl daemon-reload"
echo ""
echo "4. Start Ares service:"
echo "   $ sudo systemctl enable ares.service"
echo "   $ sudo systemctl start ares.service"
echo ""
echo "5. Check status:"
echo "   $ sudo systemctl status ares.service"
echo "   $ sudo journalctl -u ares.service -f"
echo ""
echo "==================================="
