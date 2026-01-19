# Testing Ares HDR Video Processor

This guide explains how to test Ares components without full hardware installation.

## Testing Strategy

Ares is designed for specific hardware (DeckLink capture, NVIDIA GPU, projector), but many components can be tested independently.

### Hardware Requirements for Full Testing

**Required:**
- Blackmagic DeckLink capture card (4K or 8K Pro)
- NVIDIA GPU (GTX 1060 or newer)
- Display/projector with HDR support
- IR remote (FLIRC USB or MCE receiver)

**Minimum for Development:**
- Linux system with Vulkan support
- Any GPU (NVIDIA, AMD, Intel)
- No capture card needed for unit tests

---

## Unit Testing (No Hardware Required)

### 1. Build System Tests

Test that project builds correctly:

```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

**Tests:**
- CMake configuration
- All libraries link correctly
- No compilation errors

### 2. Frame Rate Detection Tests

Test frame rate detection algorithm:

```cpp
// tests/frame_rate_detection_test.cpp
TEST(FrameRateDetection, DetectsCommonRates) {
    DeckLinkCapture capture;

    // Simulate 24fps PTS stream
    for (int i = 0; i < 30; i++) {
        int64_t pts = i * (90000 / 24);  // 90kHz clock
        capture.detectFrameRate(pts);
    }

    EXPECT_NEAR(capture.getDetectedFrameRate(), 24.0, 0.1);
    EXPECT_TRUE(capture.isFrameRateStable());
}

TEST(FrameRateDetection, Handles23976) {
    // Test 23.976 fps detection
    // Should round to 24.000
}
```

### 3. Color Space Detection Tests

Test HDR/SDR auto-detection:

```cpp
TEST(ColorSpace, DetectsHDR10) {
    VideoFrame frame;
    frame.hdr_metadata.type = HDRType::HDR10;

    // Should select BT.2020
    EXPECT_EQ(detectColorSpace(frame), ColorGamut::BT2020);
}

TEST(ColorSpace, DetectsSDR) {
    VideoFrame frame;
    frame.hdr_metadata.type = HDRType::NONE;

    // Should select BT.709
    EXPECT_EQ(detectColorSpace(frame), ColorGamut::BT709);
}
```

### 4. Refresh Rate Matcher Tests

Test mode selection algorithm:

```cpp
TEST(RefreshRateMatcher, Matches24fps) {
    std::vector<DisplayMode> modes = {
        {3840, 2160, 24.0f, false},
        {3840, 2160, 60.0f, false}
    };

    FrameRateMatcher matcher;
    DisplayMode best = matcher.findBestMatch(23.976, modes);

    EXPECT_EQ(best.refresh_rate, 24.0f);
}
```

### 5. NLS Algorithm Tests

Test non-linear stretch calculations:

```cpp
TEST(NLS, StretchesCorrectly) {
    NLSShader nls;
    NLSConfig config;
    config.horizontal_stretch = 0.5f;
    config.vertical_stretch = 0.5f;
    config.center_protect = 1.5f;

    // Test stretch function with known inputs
    vec2 input = {0.5f, 0.5f};
    vec2 output = nls.stretch(input, config);

    // Center point should remain at center
    EXPECT_NEAR(output.x, 0.5f, 0.01f);
    EXPECT_NEAR(output.y, 0.5f, 0.01f);
}
```

---

## Integration Testing (Partial Hardware)

### 1. Vulkan/libplacebo Tests (GPU Only)

Test rendering pipeline without capture:

```bash
# Create test pattern
./tests/generate_test_pattern --hdr --resolution 3840x2160 --output test.yuv

# Process through Ares pipeline
./build/ares --input test.yuv --output output.yuv --validate

# Verify output
./tests/verify_output output.yuv
```

**Tests:**
- Tone mapping (HDR → SDR)
- Dithering (check for banding reduction)
- Debanding (check for artifact removal)
- Chroma upsampling (4:2:0 → 4:4:4)

### 2. OSD Rendering Tests (GPU + Cairo)

Test OSD without capture or display:

```bash
# Render OSD to PNG
./tests/osd_test --render-to-png osd_output.png

# Verify OSD elements
./tests/verify_osd osd_output.png
```

**Tests:**
- Menu rendering
- Text rendering (Pango)
- Tab navigation
- Slider rendering

### 3. Display Mode Tests (GPU + Display)

Test display mode enumeration and switching:

```bash
# List available modes
./tests/display_test --list-modes

# Test mode switching
./tests/display_test --test-switch 24Hz
./tests/display_test --test-switch 60Hz
```

**Tests:**
- DRM/KMS initialization
- Mode enumeration
- EDID parsing
- Mode switching

---

## Mock Testing (No Hardware)

### Mock DeckLink Capture

Create mock capture that feeds test patterns:

```cpp
class MockDeckLinkCapture : public DeckLinkCapture {
public:
    Result getFrame(VideoFrame& frame, int timeout_ms) override {
        // Return pre-recorded or generated frames
        return generateTestFrame(frame, m_frame_count++);
    }

private:
    int m_frame_count = 0;
};
```

### Mock Display

Create mock display for testing without projector:

```cpp
class MockDRMDisplay : public DRMDisplay {
public:
    Result setMode(const DisplayMode& mode) override {
        m_current_mode = mode;
        LOG_INFO("Mock", "Switched to %dx%d @ %.2f Hz",
                 mode.width, mode.height, mode.refresh_rate);
        return Result::SUCCESS;
    }
};
```

---

## Docker Testing

Run tests in isolated container:

```dockerfile
# Dockerfile.test
FROM archlinux:latest

RUN pacman -Syu --noconfirm && \
    pacman -S --noconfirm \
        base-devel cmake ninja git \
        vulkan-headers vulkan-validation-layers \
        libplacebo cairo pango

COPY . /ares
WORKDIR /ares/build

RUN cmake .. && make -j$(nproc)
RUN ctest --output-on-failure
```

```bash
docker build -f Dockerfile.test -t ares-test .
docker run ares-test
```

---

## CI/CD Testing

### GitHub Actions Workflow

```yaml
name: CI

on: [push, pull_request]

jobs:
  build:
    runs-on: ubuntu-latest
    container: archlinux:latest

    steps:
    - uses: actions/checkout@v2

    - name: Install dependencies
      run: |
        pacman -Syu --noconfirm
        pacman -S --noconfirm base-devel cmake ninja vulkan-headers libplacebo

    - name: Build
      run: |
        mkdir build && cd build
        cmake -G Ninja ..
        ninja

    - name: Run tests
      run: |
        cd build
        ctest --output-on-failure
```

---

## Performance Testing

### Benchmark Without Hardware

```cpp
TEST(Performance, ToneMappingSpeed) {
    PlaceboRenderer renderer;
    VideoFrame input = generateTestFrame(3840, 2160);
    VideoFrame output;

    auto start = high_resolution_clock::now();

    for (int i = 0; i < 100; i++) {
        renderer.processFrame(input, output, config);
    }

    auto end = high_resolution_clock::now();
    double avg_ms = duration_cast<milliseconds>(end - start).count() / 100.0;

    // Should be < 16ms @ 60fps
    EXPECT_LT(avg_ms, 16.0);
}
```

---

## Validation Tests

### 1. HDR Metadata Validation

```bash
# Generate HDR10 test pattern with metadata
ffmpeg -f lavfi -i testsrc2=size=3840x2160:rate=24 \
    -vf "format=yuv420p10le" \
    -color_primaries bt2020 \
    -color_trc smpte2084 \
    -colorspace bt2020nc \
    -t 10 test_hdr.yuv

# Process through Ares
./ares --input test_hdr.yuv --output output.yuv

# Verify metadata preserved
ffprobe output.yuv
```

### 2. Frame Rate Accuracy

```python
# Measure actual frame rate
import time

frame_times = []
for frame in capture_frames(duration=60):
    frame_times.append(time.time())

# Calculate average FPS
intervals = [frame_times[i+1] - frame_times[i] for i in range(len(frame_times)-1)]
avg_fps = 1.0 / (sum(intervals) / len(intervals))

assert abs(avg_fps - 23.976) < 0.01, f"FPS mismatch: {avg_fps}"
```

---

## Test Data

### Generate Test Patterns

```bash
# HDR10 test pattern
./tests/generate_pattern --type hdr10 --fps 23.976 --output hdr10_pattern.yuv

# SDR test pattern
./tests/generate_pattern --type sdr --fps 60 --output sdr_pattern.yuv

# Black bar test
./tests/generate_pattern --type black-bars --aspect 2.35 --output letterbox.yuv

# Banding test (for debanding)
./tests/generate_pattern --type gradient --bits 8 --output banding_test.yuv
```

---

## Debugging Without Hardware

### 1. Enable Verbose Logging

```bash
export ARES_LOG_LEVEL=DEBUG
./ares
```

### 2. Dump Intermediate Frames

```bash
./ares --dump-frames --output-dir /tmp/ares_frames/
```

Outputs:
- `frame_001_input.yuv`
- `frame_001_after_crop.yuv`
- `frame_001_after_nls.yuv`
- `frame_001_after_tone_mapping.yuv`
- `frame_001_final.yuv`

### 3. Statistics Mode

```bash
./ares --stats-only
```

Outputs real-time statistics without display:
```
Frame Rate: 23.976 fps (stable)
HDR: HDR10 detected, BT.2020
Tone Mapping: BT.2390, 1000 nits → 100 nits
Processing: 12.3 ms/frame (81 fps capable)
```

---

## Summary

| Test Type | Hardware Required | What It Tests |
|-----------|------------------|---------------|
| Unit Tests | None | Algorithm correctness |
| Mock Tests | None | Integration logic |
| Docker Tests | None (CPU only) | Build system, dependencies |
| Vulkan Tests | GPU | Rendering pipeline |
| Display Tests | GPU + Display | Mode switching, EDID |
| OSD Tests | GPU | Menu rendering |
| Full Integration | All hardware | End-to-end functionality |

**Recommendation:** Start with unit tests and Docker tests, then progress to hardware testing as components are available.
