# Ares Configuration Guide

This guide explains all configuration options available in Ares.

## Configuration File Location

- **System-wide default:** `/etc/ares/ares.json`
- **Runtime state:** `/var/lib/ares/runtime.json`
- **Presets:** `/etc/ares/presets/*.json`

## Configuration Schema

### Top-Level Structure

```json
{
  "version": "1.0",
  "capture": { /* ... */ },
  "processing": { /* ... */ },
  "display": { /* ... */ },
  "sync": { /* ... */ },
  "osd": { /* ... */ },
  "input": { /* ... */ },
  "logging": { /* ... */ }
}
```

## Capture Settings

Controls video input from the DeckLink capture card.

```json
"capture": {
  "device_index": 0,
  "input_connection": "HDMI",
  "pixel_format": "10BitYUV",
  "buffer_size": 3
}
```

### Options

- **`device_index`** (integer, default: 0)
  - DeckLink device to use (0 for first device)

- **`input_connection`** (string, default: "HDMI")
  - Input connector type
  - Options: `"HDMI"`, `"SDI"`, `"Optical SDI"`, `"Component"`, `"Composite"`, `"S-Video"`

- **`pixel_format`** (string, default: "10BitYUV")
  - Capture pixel format
  - Options: `"8BitYUV"`, `"10BitYUV"`, `"8BitRGB"`, `"10BitRGB"`

- **`buffer_size`** (integer, default: 3)
  - Number of frames to buffer (2-5)
  - Lower = less latency, higher = more stability

## Processing Settings

Controls HDR tone mapping and color processing.

### Tone Mapping

```json
"processing": {
  "tone_mapping": {
    "algorithm": "bt2390",
    "target_nits": 100,
    "source_nits": 1000,
    "contrast": 1.0,
    "saturation": 1.0
  }
}
```

#### Options

- **`algorithm`** (string, default: "bt2390")
  - Tone mapping algorithm
  - Options:
    - `"bt2390"` - ITU-R BT.2390 EETF (recommended for accuracy)
    - `"reinhard"` - Simple Reinhard tone mapping
    - `"hable"` - Hable (Uncharted 2) filmic tone mapping
    - `"custom"` - User-defined LUT (requires `lut_path`)

- **`target_nits`** (float, default: 100)
  - Target display brightness in nits
  - Typical values: 48-150 nits for projectors

- **`source_nits`** (float, default: 1000)
  - Source content max brightness in nits
  - Usually 1000 for HDR10, 4000 for some content

- **`contrast`** (float, default: 1.0)
  - Contrast adjustment (0.5-2.0)
  - >1.0 = increased contrast

- **`saturation`** (float, default: 1.0)
  - Color saturation adjustment (0.5-2.0)
  - >1.0 = more saturated colors

### Color Space

```json
"color": {
  "gamut": "bt709",
  "transfer": "gamma22"
}
```

#### Options

- **`gamut`** (string, default: "bt709")
  - Target color gamut
  - Options: `"bt709"` (Rec.709), `"bt2020"` (Rec.2020), `"dci-p3"` (DCI-P3)

- **`transfer`** (string, default: "gamma22")
  - Target transfer function
  - Options: `"gamma22"`, `"gamma28"`, `"srgb"`, `"linear"`

### NLS (Natural Light Simulation)

```json
"nls": {
  "enabled": true,
  "strength": 0.5,
  "adapt_speed": 0.3
}
```

#### Options

- **`enabled`** (boolean, default: true)
  - Enable adaptive brightness adjustment

- **`strength`** (float, default: 0.5)
  - NLS effect strength (0.0-1.0)
  - 0.0 = disabled, 1.0 = maximum effect

- **`adapt_speed`** (float, default: 0.3)
  - Adaptation speed (0.0-1.0)
  - 0.0 = instant, 1.0 = very slow

### Black Bar Detection

```json
"black_bars": {
  "enabled": true,
  "threshold": 16,
  "min_content_height": 0.5
}
```

#### Options

- **`enabled`** (boolean, default: true)
  - Enable black bar detection and cropping

- **`threshold`** (integer, default: 16)
  - Pixel brightness threshold (0-255)

- **`min_content_height`** (float, default: 0.5)
  - Minimum content height ratio (0.0-1.0)

## Display Settings

Controls output to the projector/display.

```json
"display": {
  "connector": "HDMI-A-1",
  "edid_override": null,
  "preferred_modes": [
    {"width": 3840, "height": 2160, "refresh": 24},
    {"width": 3840, "height": 2160, "refresh": 60}
  ],
  "refresh_rate_matching": true
}
```

### Options

- **`connector`** (string, default: "HDMI-A-1")
  - DRM connector name
  - Find with: `ls /sys/class/drm/`

- **`edid_override`** (string or null, default: null)
  - Path to custom EDID file
  - Use for scalers/splitters that mangle EDID

- **`preferred_modes`** (array of objects)
  - List of preferred display modes
  - Ares will choose the closest match to input

- **`refresh_rate_matching`** (boolean, default: true)
  - Automatically match output refresh rate to input

## Sync Settings

Controls frame timing and synchronization.

```json
"sync": {
  "target_latency_ms": 16,
  "jitter_compensation": true,
  "vsync_mode": "adaptive"
}
```

### Options

- **`target_latency_ms`** (float, default: 16)
  - Target capture-to-display latency in milliseconds
  - Lower = faster response, higher = more stability

- **`jitter_compensation`** (boolean, default: true)
  - Enable adaptive jitter reduction

- **`vsync_mode`** (string, default: "adaptive")
  - Vertical sync mode
  - Options: `"adaptive"`, `"on"`, `"off"`

## OSD Settings

Controls on-screen display overlay.

```json
"osd": {
  "enabled": true,
  "position": "bottom_right",
  "opacity": 0.8,
  "show_stats": false
}
```

### Options

- **`enabled`** (boolean, default: true)
  - Enable OSD overlay

- **`position`** (string, default: "bottom_right")
  - OSD position on screen
  - Options: `"top_left"`, `"top_right"`, `"bottom_left"`, `"bottom_right"`, `"center"`

- **`opacity`** (float, default: 0.8)
  - OSD transparency (0.0-1.0)
  - 0.0 = fully transparent, 1.0 = fully opaque

- **`show_stats`** (boolean, default: false)
  - Show real-time statistics (FPS, latency, etc.)

## Input Settings

Controls IR remote and keyboard input.

```json
"input": {
  "device": "/dev/input/by-id/usb-flirc.tv_flirc-event-kbd",
  "mappings": {
    "KEY_UP": "menu_up",
    "KEY_DOWN": "menu_down",
    "KEY_ENTER": "menu_select",
    "KEY_ESC": "menu_back",
    "KEY_INFO": "toggle_stats",
    "KEY_MENU": "toggle_menu"
  }
}
```

### Options

- **`device`** (string)
  - Path to input device
  - Find with: `ls /dev/input/by-id/`

- **`mappings`** (object)
  - Key mappings from input to actions
  - See [Key Mappings](#key-mappings) for available actions

### Key Mappings

Available actions:

- **Menu Navigation:**
  - `menu_up`, `menu_down`, `menu_left`, `menu_right`
  - `menu_select`, `menu_back`

- **Toggles:**
  - `toggle_menu` - Show/hide menu
  - `toggle_stats` - Show/hide statistics
  - `toggle_nls` - Enable/disable NLS
  - `toggle_black_bars` - Enable/disable black bar detection

- **Presets:**
  - `preset_cinema` - Load cinema preset
  - `preset_bright_room` - Load bright room preset
  - `preset_custom` - Load custom preset

## Logging Settings

Controls logging output.

```json
"logging": {
  "level": "INFO",
  "output": "syslog",
  "file_path": "/var/log/ares/ares.log"
}
```

### Options

- **`level`** (string, default: "INFO")
  - Logging verbosity
  - Options: `"DEBUG"`, `"INFO"`, `"WARN"`, `"ERROR"`

- **`output`** (string, default: "syslog")
  - Logging output destination
  - Options: `"syslog"`, `"file"`, `"console"`, `"both"`

- **`file_path`** (string)
  - Log file path (when output is "file" or "both")

## Presets

Presets are stored in `/etc/ares/presets/` and override settings from the main config.

### Example Preset

```json
{
  "name": "Cinema",
  "description": "Dark room optimized settings",
  "processing": {
    "tone_mapping": {
      "algorithm": "bt2390",
      "target_nits": 48,
      "contrast": 1.1
    },
    "nls": {
      "enabled": true,
      "strength": 0.7
    }
  }
}
```

### Loading Presets

Presets can be loaded via:
- IR remote (mapped to `preset_*` actions)
- OSD menu (Presets submenu)
- Command line: `ares --preset cinema`

## Hot-Reload

Some settings can be changed without restarting Ares:

- All `processing.*` settings
- `osd.*` settings
- `input.mappings`

Settings that require restart:
- `capture.*` settings
- `display.connector`
- `logging.level`

## Validation

Validate your configuration before applying:

```bash
ares --validate-config --config /etc/ares/ares.json
```

## Examples

### Low Latency Gaming

```json
{
  "sync": {
    "target_latency_ms": 8,
    "vsync_mode": "off"
  },
  "capture": {
    "buffer_size": 2
  },
  "processing": {
    "tone_mapping": {
      "algorithm": "reinhard",
      "target_nits": 150
    },
    "nls": {
      "enabled": false
    }
  }
}
```

### Cinematic Viewing

```json
{
  "processing": {
    "tone_mapping": {
      "algorithm": "bt2390",
      "target_nits": 48,
      "contrast": 1.1,
      "saturation": 0.95
    },
    "nls": {
      "enabled": true,
      "strength": 0.8,
      "adapt_speed": 0.2
    },
    "black_bars": {
      "enabled": true
    }
  }
}
```

## Next Steps

- Read [BUILDING.md](BUILDING.md) for build instructions
- Read [HARDWARE.md](HARDWARE.md) for hardware compatibility
- Check example configs in `/etc/ares/`
