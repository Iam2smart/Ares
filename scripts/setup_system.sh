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

# Create ares user if it doesn't exist
if ! id -u ares > /dev/null 2>&1; then
    echo "Creating ares user..."
    useradd -r -s /bin/false -G video,input ares
    echo "User 'ares' created."
else
    echo "User 'ares' already exists."
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
        cp -r ../config/presets/* /etc/ares/presets/
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
    cpupower frequency-set -g performance
    echo "CPU governor set to performance."
else
    echo "Warning: cpupower not found. Install 'cpupower' package."
fi

# Load kernel modules
echo "Loading kernel modules..."
modprobe nvidia-drm modeset=1 || echo "Warning: nvidia-drm module not loaded"
modprobe decklink || echo "Warning: decklink module not loaded (DeckLink SDK may not be installed)"

# Check for DeckLink devices
if ls /dev/blackmagic* 1> /dev/null 2>&1; then
    echo "DeckLink devices detected:"
    ls -l /dev/blackmagic*
else
    echo "Warning: No DeckLink devices found."
fi

# Enable SSH for remote access
echo "Enabling SSH service..."
systemctl enable sshd.service
systemctl start sshd.service
echo "SSH service enabled. You can connect from your laptop using:"
echo "  ssh user@<this-machine-ip>"
echo ""
echo "For improved security, consider:"
echo "  1. Setting up SSH key authentication"
echo "  2. Disabling password authentication in /etc/ssh/sshd_config"
echo "  3. Restricting SSH access to specific IPs"
echo ""

# Disable unnecessary services
echo "Disabling unnecessary services..."
systemctl disable bluetooth.service 2>/dev/null || true
systemctl stop bluetooth.service 2>/dev/null || true
systemctl disable cups.service 2>/dev/null || true
systemctl stop cups.service 2>/dev/null || true

echo ""
echo "==================================="
echo "System setup complete!"
echo ""
echo "Next steps:"
echo "1. Build Ares: cd build && cmake .. && make"
echo "2. Install Ares: sudo make install"
echo "3. Enable service: sudo systemctl enable ares.service"
echo "4. Start service: sudo systemctl start ares.service"
echo "==================================="
