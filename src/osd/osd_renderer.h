#pragma once

#include <ares/types.h>
#include <ares/osd_config.h>
#include <cairo/cairo.h>
#include <pango/pangocairo.h>
#include <vulkan/vulkan.h>
#include <memory>
#include <vector>

namespace ares {
namespace osd {

// OSD rendering with Cairo/Pango for text and graphics
class OSDRenderer {
public:
    OSDRenderer();
    ~OSDRenderer();

    // Initialize OSD renderer
    Result initialize(uint32_t width, uint32_t height, const OSDConfig& config);

    // Shutdown
    void shutdown();

    // Check if initialized
    bool isInitialized() const { return m_initialized; }

    // Begin frame (clear surface)
    void beginFrame();

    // End frame (prepare for display)
    void endFrame();

    // Drawing operations
    void drawText(const std::string& text, int x, int y, const OSDConfig& config);
    void drawRectangle(int x, int y, int width, int height, uint32_t color, bool filled = true);
    void drawTab(const std::string& title, int x, int y, int width, int height,
                bool active, const OSDConfig& config);
    void drawMenuItem(const MenuItem& item, int x, int y, int width, int height,
                     bool selected, const OSDConfig& config);
    void drawSlider(const std::string& label, float value, float min, float max,
                   int x, int y, int width, bool selected, const OSDConfig& config);
    void drawProgressBar(float progress, int x, int y, int width, int height, uint32_t color);

    // Get rendered surface data (RGBA8)
    const uint8_t* getSurfaceData() const;
    size_t getSurfaceDataSize() const;

    // Get surface dimensions
    uint32_t getWidth() const { return m_width; }
    uint32_t getHeight() const { return m_height; }

    // Get configuration
    const OSDConfig& getConfig() const { return m_config; }

private:
    // Cairo context
    cairo_surface_t* m_surface = nullptr;
    cairo_t* m_cr = nullptr;

    // Pango layout for text
    PangoLayout* m_layout = nullptr;
    PangoFontDescription* m_font_desc = nullptr;

    // Dimensions
    uint32_t m_width = 0;
    uint32_t m_height = 0;

    // Configuration
    OSDConfig m_config;

    // State
    bool m_initialized = false;

    // Helper functions
    void setColor(uint32_t rgba);
    void setFont(const std::string& font, int size);
};

// OSD compositor - composites OSD over video
class OSDCompositor {
public:
    OSDCompositor();
    ~OSDCompositor();

    // Initialize compositor with Vulkan
    Result initialize(VkDevice device, VkPhysicalDevice physical_device);

    // Shutdown
    void shutdown();

    // Composite OSD over video frame
    Result composite(const VideoFrame& video, const uint8_t* osd_data,
                    uint32_t osd_width, uint32_t osd_height,
                    VideoFrame& output, const OSDConfig& config);

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;

    // Vulkan resources for compositing
    VkImage m_osd_image = VK_NULL_HANDLE;
    VkDeviceMemory m_osd_memory = VK_NULL_HANDLE;
    VkImageView m_osd_view = VK_NULL_HANDLE;

    bool m_initialized = false;
};

} // namespace osd
} // namespace ares
