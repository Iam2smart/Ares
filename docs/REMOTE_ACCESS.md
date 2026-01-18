# Remote Access Guide

This guide explains how to remotely access your Ares video processor from your laptop or other devices.

## SSH Access

Ares is configured with OpenSSH for secure remote access.

### Initial Setup

1. **Install OpenSSH** (done automatically by install script):
   ```bash
   sudo pacman -S openssh
   ```

2. **Enable SSH Service** (done by setup script):
   ```bash
   sudo systemctl enable sshd
   sudo systemctl start sshd
   ```

3. **Find Your Ares Machine's IP Address**:
   ```bash
   ip addr show
   ```
   Look for the IP address on your network interface (e.g., `192.168.1.100`)

### Connecting from Your Laptop

**From Linux/Mac**:
```bash
ssh username@<ares-ip-address>
```

**From Windows**:
- Use Windows Terminal, PowerShell, or PuTTY
- Example: `ssh username@192.168.1.100`

### SSH Key Authentication (Recommended)

For secure, password-less access:

1. **Generate SSH Key on Your Laptop** (if you don't have one):
   ```bash
   ssh-keygen -t ed25519 -C "your_email@example.com"
   ```
   Press Enter to accept defaults.

2. **Copy Your Public Key to Ares Machine**:
   ```bash
   ssh-copy-id username@<ares-ip-address>
   ```
   Enter your password when prompted.

3. **Test Key-Based Login**:
   ```bash
   ssh username@<ares-ip-address>
   ```
   You should log in without entering a password.

4. **Disable Password Authentication** (optional, for security):

   On Ares machine, edit `/etc/ssh/sshd_config`:
   ```bash
   sudo nano /etc/ssh/sshd_config
   ```

   Change:
   ```
   PasswordAuthentication yes
   ```
   To:
   ```
   PasswordAuthentication no
   ```

   Restart SSH:
   ```bash
   sudo systemctl restart sshd
   ```

## Remote Management Tasks

### Monitor Ares Service

Check service status:
```bash
ssh username@ares-ip "sudo systemctl status ares"
```

View live logs:
```bash
ssh username@ares-ip "sudo journalctl -u ares -f"
```

### Control Ares

Start/stop/restart:
```bash
ssh username@ares-ip "sudo systemctl start ares"
ssh username@ares-ip "sudo systemctl stop ares"
ssh username@ares-ip "sudo systemctl restart ares"
```

### Update Configuration

Edit configuration remotely:
```bash
ssh username@ares-ip "sudo nano /etc/ares/ares.json"
```

Or copy configuration from laptop:
```bash
scp my-ares-config.json username@ares-ip:/tmp/
ssh username@ares-ip "sudo mv /tmp/my-ares-config.json /etc/ares/ares.json"
ssh username@ares-ip "sudo systemctl restart ares"
```

### Run Test Programs

Test capture:
```bash
ssh username@ares-ip "/usr/local/bin/test_capture"
```

Test processing:
```bash
ssh username@ares-ip "/usr/local/bin/test_processing"
```

Test display:
```bash
ssh username@ares-ip "/usr/local/bin/test_display"
```

## File Transfer

### Copy Files to Ares

Upload configuration:
```bash
scp config.json username@ares-ip:/etc/ares/
```

Upload LUT files:
```bash
scp my-lut.cube username@ares-ip:/etc/ares/luts/
```

### Copy Files from Ares

Download logs:
```bash
scp username@ares-ip:/var/log/ares/ares.log ./
```

Download captured frames:
```bash
scp username@ares-ip:/var/lib/ares/frames/* ./frames/
```

## SSH Config for Easy Access

Create/edit `~/.ssh/config` on your laptop:

```
Host ares
    HostName 192.168.1.100
    User username
    Port 22
    IdentityFile ~/.ssh/id_ed25519
```

Then connect simply with:
```bash
ssh ares
```

## Security Best Practices

1. **Use SSH Keys**: More secure than passwords
2. **Disable Password Authentication**: After setting up SSH keys
3. **Change Default SSH Port**: Edit `/etc/ssh/sshd_config` and change `Port 22`
4. **Use Firewall**: Restrict SSH to your laptop's IP
   ```bash
   sudo ufw allow from 192.168.1.0/24 to any port 22
   sudo ufw enable
   ```
5. **Keep System Updated**:
   ```bash
   ssh ares "sudo pacman -Syu"
   ```

## Troubleshooting

### Cannot Connect

1. **Check SSH service is running**:
   ```bash
   sudo systemctl status sshd
   ```

2. **Check firewall**:
   ```bash
   sudo ufw status
   ```

3. **Test network connectivity**:
   ```bash
   ping <ares-ip-address>
   ```

### Connection Refused

- Ensure SSH service is running: `sudo systemctl start sshd`
- Check if port 22 is open: `sudo netstat -tlnp | grep :22`

### Permission Denied

- Verify username and password are correct
- Check SSH key permissions: `chmod 600 ~/.ssh/id_ed25519`
- Verify public key is in `~/.ssh/authorized_keys` on Ares machine

## Network Configuration

### Static IP (Recommended)

For consistent access, set a static IP on Ares machine.

Edit network configuration (systemd-networkd):
```bash
sudo nano /etc/systemd/network/20-wired.network
```

```ini
[Match]
Name=eno1

[Network]
Address=192.168.1.100/24
Gateway=192.168.1.1
DNS=8.8.8.8
DNS=8.8.4.4
```

Restart networking:
```bash
sudo systemctl restart systemd-networkd
```

### DHCP with Reserved IP

Alternatively, configure your router to assign a fixed IP to Ares machine's MAC address.

## Remote Desktop (Optional)

For graphical access (though Ares runs headless), you can install VNC:

```bash
sudo pacman -S tigervnc
```

Start VNC server:
```bash
vncserver :1 -geometry 1920x1080 -depth 24
```

Connect from laptop using VNC viewer:
```
<ares-ip-address>:5901
```

Note: VNC is not recommended for production use. Use SSH for management.
