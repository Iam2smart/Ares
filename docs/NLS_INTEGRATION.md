# NLS (Non-Linear Stretch) Integration Status

## Overview

The NLS shader provides aspect ratio warping for transforming 16:9 content to cinemascope formats (2.35:1, 2.40:1, 2.55:1) with minimal distortion in the center and more stretch at the edges. It's based on NLS-Next by NotMithical.

## Current Status: Partially Complete

### ✅ Completed Components

1. **GLSL Compute Shader** (lines 10-151 in `nls_shader.cpp`)
   - Complete NLS-Next algorithm implementation
   - Bidirectional nonlinear stretch with power curves
   - Bicubic interpolation support
   - Center protection parameter
   - Crop and bars amount control
   - Ready to use once shader compilation is available

2. **libplacebo Integration** (GPU Operations)
   - Texture creation using `pl_tex_create` ✅
   - Frame upload using `pl_tex_upload` ✅
   - Frame download using `pl_tex_download` ✅
   - Automatic texture management ✅
   - Memory handling ✅

3. **Pipeline Integration**
   - NLSShader integrated into ProcessingPipeline ✅
   - Shares libplacebo GPU with PlaceboRenderer ✅
   - Configuration system in place ✅
   - Statistics tracking ✅

### ⚠️ Missing Component: Shader Compilation

The only missing piece is **GLSL to SPIR-V compilation** of the compute shader. This requires:

#### Option 1: Add shaderc Library (Recommended)

```cmake
# Add to CMakeLists.txt
find_package(shaderc REQUIRED)
target_link_libraries(ares PRIVATE shaderc)
```

Then implement in `nls_shader.cpp:createPipeline()`:

```cpp
#include <shaderc/shaderc.hpp>

// Compile GLSL to SPIR-V
shaderc::Compiler compiler;
shaderc::CompileOptions options;
options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);

auto result = compiler.CompileGlslToSpv(
    NLS_COMPUTE_SHADER, strlen(NLS_COMPUTE_SHADER),
    shaderc_compute_shader, "nls_shader.comp", options);

if (result.GetCompilationStatus() != shaderc_compilation_status_success) {
    LOG_ERROR("Processing", "Shader compilation failed: %s", result.GetErrorMessage().c_str());
    return Result::ERROR_GENERIC;
}

std::vector<uint32_t> spirv(result.begin(), result.end());

// Create pl_pass with compiled SPIR-V
struct pl_pass_params pass_params = {};
pass_params.type = PL_PASS_COMPUTE;
pass_params.compute_shader = spirv.data();
pass_params.compute_shader_size = spirv.size() * sizeof(uint32_t);
// ... configure descriptors and push constants

m_compute_pass = pl_pass_create(m_gpu, &pass_params);
```

Then in `runCompute()`:

```cpp
// Set push constants
PushConstants constants = {
    .horizontal_stretch = config.horizontal_stretch,
    .vertical_stretch = config.vertical_stretch,
    // ... other parameters
};

// Run compute pass
pl_pass_run(m_gpu, &(struct pl_pass_run_params) {
    .pass = m_compute_pass,
    .target = m_output_tex,
    .push_constants = &constants,
});
```

#### Option 2: Use Pre-compiled SPIR-V

Alternatively, compile the shader offline and embed the SPIR-V binary:

```bash
# Compile offline
glslangValidator -V nls_shader.comp -o nls_shader.spv

# Convert to C array
xxd -i nls_shader.spv > nls_shader_spirv.h
```

Then include and use directly in `createPipeline()`.

#### Option 3: Use libplacebo's MPV Shader Parser

The NLS shader could be converted to MPV user shader format and loaded via:

```cpp
#include <libplacebo/shaders/custom.h>

pl_mpv_user_shader* shader = pl_mpv_user_shader_parse(
    m_gpu, nls_shader_text, strlen(nls_shader_text));
```

## Current Behavior (Fallback)

Without shader compilation:
- NLS is initialized and textures are created
- Frame upload/download works correctly
- The compute shader execution logs a warning and bypasses the warping
- Frames pass through with original aspect ratio
- Statistics are still tracked

This allows the rest of the pipeline to work while NLS shader compilation is pending.

## Configuration

NLS is configured via `ProcessingConfig::nls`:

```cpp
struct NLSConfig {
    bool enabled = false;
    float horizontal_stretch = 0.5f;   // 0.0 - 1.0
    float vertical_stretch = 0.5f;     // 0.0 - 1.0
    float horizontal_power = 2.0f;     // Center protection power
    float vertical_power = 2.0f;

    enum class TargetAspect {
        SCOPE_235,  // 2.35:1
        SCOPE_240,  // 2.40:1
        SCOPE_255,  // 2.55:1
        CUSTOM
    } target_aspect = TargetAspect::SCOPE_235;

    float custom_aspect_ratio = 2.35f;
};
```

## Testing Once Complete

Once shader compilation is implemented, test with:

1. 16:9 SDR content → verify warping to 2.35:1
2. 16:9 HDR content → verify warping + tone mapping
3. Different aspect ratios (2.40:1, 2.55:1)
4. Adjust center protection parameter (0.0 - 4.0)
5. Performance: aim for < 2ms processing time per frame

## References

- NLS-Next by NotMithical: https://github.com/NotMithical/MPV-NLS-Next
- Original NLS concept discussion: https://www.avsforum.com/threads/the-one-thread-for-all-your-aspect-ratio-mad...
- libplacebo documentation: https://libplacebo.org/
- shaderc documentation: https://github.com/google/shaderc
