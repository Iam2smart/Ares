# Receiver Volume Integration

Ares supports displaying receiver volume via network control for Onkyo/Integra receivers using the EISCP protocol.

## Overview

Since your receiver won't be seeing the video signal (it's in passthrough mode), you can't see the receiver's volume OSD. Ares solves this by connecting to your receiver over the network and displaying volume changes directly on screen.

## Supported Receivers

- **Integra** - All network-enabled models (DHC-40.2, DHC-80.3, DRX series, etc.)
- **Onkyo** - All network-enabled models (TX-NR series, RZ series, etc.)
- **Pioneer** - Some models with EISCP support

## Configuration

Add receiver settings to `/etc/ares/ares.json`:

```json
{
  "receiver": {
    "enabled": true,
    "ip_address": "192.168.1.100",
    "port": 60128,
    "max_volume": 80,
    "monitoring_enabled": true,
    "volume_display": {
      "show_on_change": true,
      "display_duration_ms": 3000,
      "fade_duration_ms": 500,
      "position": "bottom_right"
    }
  }
}
```

### Configuration Options

#### Receiver Connection

- **`enabled`** (boolean, default: false)
  - Enable receiver integration

- **`ip_address`** (string, required)
  - IP address of your receiver
  - Find this in your receiver's network settings
  - Example: "192.168.1.100"

- **`port`** (integer, default: 60128)
  - EISCP port (standard is 60128)
  - Usually don't need to change this

- **`max_volume`** (integer, default: 80)
  - Maximum volume in receiver units
  - Integra: typically 80 or 82
  - Onkyo: typically 80
  - Check your receiver's manual

- **`monitoring_enabled`** (boolean, default: true)
  - Continuously monitor receiver for volume changes
  - Recommended: keep enabled for real-time updates

#### Volume Display

- **`show_on_change`** (boolean, default: true)
  - Show volume overlay when volume changes

- **`display_duration_ms`** (integer, default: 3000)
  - How long to show volume overlay (milliseconds)
  - 3000 = 3 seconds

- **`fade_duration_ms`** (integer, default: 500)
  - Fade-out animation duration
  - 500 = 0.5 seconds

- **`position`** (string, default: "bottom_right")
  - Where to display volume overlay
  - Options: "bottom_right", "bottom_left", "top_right", "top_left"

## Finding Your Receiver's IP Address

### Method 1: Receiver's OSD/Menu

1. Press "Setup" or "Menu" on your receiver remote
2. Navigate to "Network" → "Network Information"
3. Look for "IP Address"
4. Write down the address (e.g., 192.168.1.100)

### Method 2: Router Admin Page

1. Log into your router's admin page
2. Look for "Connected Devices" or "DHCP Clients"
3. Find your Integra/Onkyo receiver in the list
4. Note the IP address

### Method 3: Static IP (Recommended)

Set a static IP on your receiver to prevent it from changing:

1. Go to receiver menu: "Setup" → "Network"
2. Change "DHCP" to "Static" or "Manual"
3. Set IP address: 192.168.1.100 (or your preference)
4. Set Subnet: 255.255.255.0
5. Set Gateway: 192.168.1.1 (your router)
6. Set DNS: 192.168.1.1 (your router)

## Testing the Connection

### Test with Command Line

You can test the EISCP connection before configuring Ares:

```bash
# Install netcat if not already installed
sudo pacman -S gnu-netcat

# Test connection (should see response)
echo -ne "ISCP\x00\x00\x00\x10\x00\x00\x00\x0a\x01\x00\x00\x00!1MVLQSTN\r\n" | nc 192.168.1.100 60128 | xxd
```

If you see a response with "ISCP" header and "MVL" data, your receiver is responding correctly.

### Test with Java Tool

The [jEISCP](https://github.com/cljk/jEISCP) tool can also be used for testing:

```bash
# Clone the repo
git clone https://github.com/cljk/jEISCP.git
cd jEISCP

# Build and run
./gradlew build
java -jar build/libs/jEISCP.jar --host 192.168.1.100 --command MVL --param QSTN
```

## Volume Overlay Display

When volume changes, Ares displays a sleek overlay in the bottom right corner showing:

- **Speaker Icon**: Visual indicator
  - Sound waves animate based on volume level
  - Red X when muted

- **Volume Level**: Large numeric display
  - Shows as percentage (0-100)
  - Shows "MUTED" when muted

- **Volume Bar**: Color-coded bar graph
  - Green: Low volume (0-33%)
  - Yellow: Medium volume (34-66%)
  - Orange: High volume (67-100%)

- **Auto-Hide**: Fades out after 3 seconds (configurable)

## Volume Control

You can control receiver volume through:

1. **Your Receiver Remote** - Changes detected and displayed automatically
2. **IR Remote** - Program volume up/down/mute buttons (if supported)
3. **Web Interface** - Coming in v1.1

## Troubleshooting

### Receiver Not Connecting

**Check network connectivity:**
```bash
ping 192.168.1.100
```

**Check if receiver is listening on port 60128:**
```bash
nmap -p 60128 192.168.1.100
```

**Check Ares logs:**
```bash
journalctl -u ares -f | grep Receiver
```

Common issues:
- Receiver has network standby disabled (enable in receiver settings)
- Firewall blocking port 60128
- Receiver is on different subnet
- Wrong IP address

### Volume Not Updating

- Check `monitoring_enabled: true` in config
- Verify receiver is sending updates (some models require enabling network control)
- Check Ares logs for EISCP errors

### Wrong Volume Scale

If volume shows as 200% or wrong values:

- Adjust `max_volume` in config
- Integra receivers typically use 0-80 scale
- Some Onkyo receivers use 0-100 scale
- Check your receiver manual for volume range

## Integration with Other Systems

### Home Automation

You can query receiver volume programmatically:

```bash
# Get current volume (if Ares adds HTTP API in v1.1)
curl http://localhost:8080/api/receiver/volume
```

### Custom Scripts

The EISCP protocol is well-documented. You can create custom integrations:

**Volume Up:**
```bash
echo -ne "ISCP\x00\x00\x00\x10\x00\x00\x00\x09\x01\x00\x00\x00!1MVLUP\r\n" | nc 192.168.1.100 60128
```

**Volume Down:**
```bash
echo -ne "ISCP\x00\x00\x00\x10\x00\x00\x00\x0b\x01\x00\x00\x00!1MVLDOWN\r\n" | nc 192.168.1.100 60128
```

**Set Volume to 50 (hex 32):**
```bash
echo -ne "ISCP\x00\x00\x00\x10\x00\x00\x00\x09\x01\x00\x00\x00!1MVL32\r\n" | nc 192.168.1.100 60128
```

**Mute:**
```bash
echo -ne "ISCP\x00\x00\x00\x10\x00\x00\x00\x09\x01\x00\x00\x00!1AMT01\r\n" | nc 192.168.1.100 60128
```

## EISCP Protocol Reference

### Common Commands

| Command | Parameter | Description |
|---------|-----------|-------------|
| `MVL` | `00`-`50` (hex) | Set master volume |
| `MVL` | `UP` | Volume up |
| `MVL` | `DOWN` | Volume down |
| `MVL` | `QSTN` | Query current volume |
| `AMT` | `00` | Unmute |
| `AMT` | `01` | Mute |
| `AMT` | `TG` | Toggle mute |
| `PWR` | `01` | Power on |
| `PWR` | `00` | Power off |
| `SLI` | `XX` | Select input |

### Response Format

Responses follow the same ISCP format:
- Header: "ISCP" (4 bytes)
- Header Size: 16 (big-endian)
- Data Size: variable (big-endian)
- Version: 0x01
- Data: "!1" + command + parameter + "\r\n"

Example response for volume query:
```
ISCP + size + "!1MVL32\r\n"
```
This means volume is at 0x32 (50 decimal).

## Performance Impact

- **CPU Usage**: Negligible (<0.1%)
- **Network Traffic**: ~100 bytes every 5 seconds (monitoring queries)
- **Latency**: <50ms from receiver volume change to OSD update

## Future Enhancements (v1.1+)

- [ ] Support for other receiver protocols (RS-232, IP control)
- [ ] Receiver power state display
- [ ] Input source display
- [ ] Audio format display (Dolby Atmos, DTS:X, etc.)
- [ ] Multi-zone volume control
- [ ] Web interface for receiver control

## Credits

- EISCP protocol reverse engineering: [eiscp-rs](https://github.com/clux/eiscp-rs)
- Java reference implementation: [jEISCP](https://github.com/cljk/jEISCP)
- Protocol documentation: [eiscp.readthedocs.io](https://eiscp.readthedocs.io/)
