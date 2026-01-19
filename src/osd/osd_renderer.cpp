#include "osd_renderer.h"
#include <cmath>
#include <cstring>

namespace ares {
namespace osd {

OSDRenderer::OSDRenderer() = default;

OSDRenderer::~OSDRenderer() {
    shutdown();
}

Result OSDRenderer::initialize(uint32_t width, uint32_t height, const OSDConfig& config) {
    if (m_initialized) {
        shutdown();
    }

    m_width = width;
    m_height = height;
    m_config = config;

    // Create Cairo image surface (ARGB32 format)
    m_surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    if (cairo_surface_status(m_surface) != CAIRO_STATUS_SUCCESS) {
        return Result::ERROR_INITIALIZATION_FAILED;
    }

    // Create Cairo context
    m_cr = cairo_create(m_surface);
    if (cairo_status(m_cr) != CAIRO_STATUS_SUCCESS) {
        cairo_surface_destroy(m_surface);
        m_surface = nullptr;
        return Result::ERROR_INITIALIZATION_FAILED;
    }

    // Create Pango layout for text rendering
    m_layout = pango_cairo_create_layout(m_cr);
    if (!m_layout) {
        cairo_destroy(m_cr);
        cairo_surface_destroy(m_surface);
        m_cr = nullptr;
        m_surface = nullptr;
        return Result::ERROR_INITIALIZATION_FAILED;
    }

    // Create default font description
    m_font_desc = pango_font_description_from_string(config.font_family.c_str());
    pango_font_description_set_size(m_font_desc, config.font_size * PANGO_SCALE);
    pango_layout_set_font_description(m_layout, m_font_desc);

    m_initialized = true;
    return Result::SUCCESS;
}

void OSDRenderer::shutdown() {
    if (m_font_desc) {
        pango_font_description_free(m_font_desc);
        m_font_desc = nullptr;
    }

    if (m_layout) {
        g_object_unref(m_layout);
        m_layout = nullptr;
    }

    if (m_cr) {
        cairo_destroy(m_cr);
        m_cr = nullptr;
    }

    if (m_surface) {
        cairo_surface_destroy(m_surface);
        m_surface = nullptr;
    }

    m_initialized = false;
}

void OSDRenderer::beginFrame() {
    if (!m_initialized) return;

    // Clear surface with transparent background
    cairo_save(m_cr);
    cairo_set_operator(m_cr, CAIRO_OPERATOR_CLEAR);
    cairo_paint(m_cr);
    cairo_restore(m_cr);

    // Set default blend mode
    cairo_set_operator(m_cr, CAIRO_OPERATOR_OVER);
}

void OSDRenderer::endFrame() {
    if (!m_initialized) return;

    // Flush Cairo surface to ensure all drawing is complete
    cairo_surface_flush(m_surface);
}

void OSDRenderer::setColor(uint32_t rgba) {
    double r = ((rgba >> 24) & 0xFF) / 255.0;
    double g = ((rgba >> 16) & 0xFF) / 255.0;
    double b = ((rgba >> 8) & 0xFF) / 255.0;
    double a = (rgba & 0xFF) / 255.0;
    cairo_set_source_rgba(m_cr, r, g, b, a);
}

void OSDRenderer::setFont(const std::string& font, int size) {
    if (m_font_desc) {
        pango_font_description_free(m_font_desc);
    }
    m_font_desc = pango_font_description_from_string(font.c_str());
    pango_font_description_set_size(m_font_desc, size * PANGO_SCALE);
    pango_layout_set_font_description(m_layout, m_font_desc);
}

void OSDRenderer::drawText(const std::string& text, int x, int y, const OSDConfig& config) {
    if (!m_initialized) return;

    // Set font
    setFont(config.font_family, config.font_size);

    // Set text color
    setColor(config.text_color);

    // Set text
    pango_layout_set_text(m_layout, text.c_str(), -1);

    // Move to position
    cairo_move_to(m_cr, x, y);

    // Draw text with shadow if enabled
    if (config.text_shadow) {
        cairo_save(m_cr);
        cairo_move_to(m_cr, x + 2, y + 2);
        setColor(config.shadow_color);
        pango_cairo_show_layout(m_cr, m_layout);
        cairo_restore(m_cr);

        cairo_move_to(m_cr, x, y);
        setColor(config.text_color);
    }

    // Draw text
    pango_cairo_show_layout(m_cr, m_layout);
}

void OSDRenderer::drawRectangle(int x, int y, int width, int height, uint32_t color, bool filled) {
    if (!m_initialized) return;

    setColor(color);
    cairo_rectangle(m_cr, x, y, width, height);

    if (filled) {
        cairo_fill(m_cr);
    } else {
        cairo_stroke(m_cr);
    }
}

void OSDRenderer::drawTab(const std::string& title, int x, int y, int width, int height,
                          bool active, const OSDConfig& config) {
    if (!m_initialized) return;

    // Draw tab background
    uint32_t bg_color = active ? config.tab_active_bg : config.tab_inactive_bg;
    drawRectangle(x, y, width, height, bg_color, true);

    // Draw tab border
    if (active) {
        cairo_set_line_width(m_cr, 2.0);
        setColor(config.highlight_color);
        cairo_rectangle(m_cr, x, y, width, height);
        cairo_stroke(m_cr);
    }

    // Draw tab text (centered)
    setFont(config.font_family, config.font_size);
    pango_layout_set_text(m_layout, title.c_str(), -1);

    int text_width, text_height;
    pango_layout_get_pixel_size(m_layout, &text_width, &text_height);

    int text_x = x + (width - text_width) / 2;
    int text_y = y + (height - text_height) / 2;

    uint32_t text_color = active ? config.tab_active_text : config.tab_inactive_text;
    setColor(text_color);
    cairo_move_to(m_cr, text_x, text_y);
    pango_cairo_show_layout(m_cr, m_layout);
}

void OSDRenderer::drawMenuItem(const MenuItem& item, int x, int y, int width, int height,
                               bool selected, const OSDConfig& config) {
    if (!m_initialized) return;

    // Draw selection highlight
    if (selected) {
        drawRectangle(x, y, width, height, config.selection_color, true);
    }

    // Draw item icon if present
    int icon_x = x + 10;
    int text_x = icon_x + 30;

    if (!item.icon.empty()) {
        // TODO: Draw icon (would need icon system)
        // For now, just draw a colored square
        uint32_t icon_color = item.enabled ? 0xFFFFFFFF : 0x808080FF;
        drawRectangle(icon_x, y + (height - 20) / 2, 20, 20, icon_color, true);
    }

    // Draw item label
    setFont(config.font_family, config.font_size);
    uint32_t text_color = item.enabled ? config.text_color : config.disabled_text_color;
    if (selected) {
        text_color = config.selected_text_color;
    }
    setColor(text_color);

    pango_layout_set_text(m_layout, item.label.c_str(), -1);
    cairo_move_to(m_cr, text_x, y + (height - config.font_size) / 2);
    pango_cairo_show_layout(m_cr, m_layout);

    // Draw value if present
    if (!item.value.empty()) {
        setColor(selected ? config.selected_text_color : config.value_color);
        pango_layout_set_text(m_layout, item.value.c_str(), -1);

        int value_width;
        pango_layout_get_pixel_size(m_layout, &value_width, nullptr);

        cairo_move_to(m_cr, x + width - value_width - 10, y + (height - config.font_size) / 2);
        pango_cairo_show_layout(m_cr, m_layout);
    }

    // Draw submenu indicator
    if (item.has_submenu) {
        int arrow_x = x + width - 20;
        int arrow_y = y + height / 2;

        setColor(text_color);
        cairo_move_to(m_cr, arrow_x, arrow_y - 5);
        cairo_line_to(m_cr, arrow_x + 8, arrow_y);
        cairo_line_to(m_cr, arrow_x, arrow_y + 5);
        cairo_stroke(m_cr);
    }
}

void OSDRenderer::drawSlider(const std::string& label, float value, float min, float max,
                             int x, int y, int width, bool selected, const OSDConfig& config) {
    if (!m_initialized) return;

    int slider_height = 30;
    int bar_height = 8;
    int handle_size = 16;

    // Draw label
    setFont(config.font_family, config.font_size);
    uint32_t text_color = selected ? config.selected_text_color : config.text_color;
    setColor(text_color);

    pango_layout_set_text(m_layout, label.c_str(), -1);
    cairo_move_to(m_cr, x, y);
    pango_cairo_show_layout(m_cr, m_layout);

    int label_height;
    pango_layout_get_pixel_size(m_layout, nullptr, &label_height);

    // Calculate slider bar position
    int bar_y = y + label_height + 5;
    int bar_x = x + 10;
    int bar_width = width - 20;

    // Draw slider track
    drawRectangle(bar_x, bar_y + (slider_height - bar_height) / 2,
                  bar_width, bar_height, config.slider_bg_color, true);

    // Calculate handle position
    float normalized = (value - min) / (max - min);
    int handle_x = bar_x + (int)(normalized * bar_width);

    // Draw filled portion
    drawRectangle(bar_x, bar_y + (slider_height - bar_height) / 2,
                  handle_x - bar_x, bar_height, config.slider_fill_color, true);

    // Draw handle
    uint32_t handle_color = selected ? config.highlight_color : config.slider_handle_color;
    cairo_set_line_width(m_cr, 2.0);
    setColor(handle_color);
    cairo_arc(m_cr, handle_x, bar_y + slider_height / 2, handle_size / 2, 0, 2 * M_PI);
    cairo_fill(m_cr);

    // Draw value text
    char value_text[32];
    snprintf(value_text, sizeof(value_text), "%.2f", value);
    pango_layout_set_text(m_layout, value_text, -1);

    int value_width;
    pango_layout_get_pixel_size(m_layout, &value_width, nullptr);

    setColor(text_color);
    cairo_move_to(m_cr, x + width - value_width, y);
    pango_cairo_show_layout(m_cr, m_layout);
}

void OSDRenderer::drawProgressBar(float progress, int x, int y, int width, int height, uint32_t color) {
    if (!m_initialized) return;

    // Draw background
    drawRectangle(x, y, width, height, 0x404040FF, true);

    // Draw progress
    int fill_width = (int)(progress * width);
    drawRectangle(x, y, fill_width, height, color, true);

    // Draw border
    cairo_set_line_width(m_cr, 1.0);
    setColor(0xFFFFFFFF);
    cairo_rectangle(m_cr, x, y, width, height);
    cairo_stroke(m_cr);
}

void OSDRenderer::drawVolumeOverlay(int level, bool muted, float opacity) {
    if (!m_initialized) return;

    // Volume overlay dimensions and position (bottom right corner)
    const int overlay_width = 300;
    const int overlay_height = 120;
    const int margin = 40;
    const int x = m_width - overlay_width - margin;
    const int y = m_height - overlay_height - margin;

    // Background with rounded corners and opacity
    cairo_save(m_cr);

    // Set global opacity
    cairo_push_group(m_cr);

    // Draw rounded rectangle background
    const int radius = 12;
    const double degrees = M_PI / 180.0;

    cairo_new_sub_path(m_cr);
    cairo_arc(m_cr, x + overlay_width - radius, y + radius, radius, -90 * degrees, 0 * degrees);
    cairo_arc(m_cr, x + overlay_width - radius, y + overlay_height - radius, radius, 0 * degrees, 90 * degrees);
    cairo_arc(m_cr, x + radius, y + overlay_height - radius, radius, 90 * degrees, 180 * degrees);
    cairo_arc(m_cr, x + radius, y + radius, radius, 180 * degrees, 270 * degrees);
    cairo_close_path(m_cr);

    // Background color with transparency
    setColor(0x202020E6);  // Dark background, 90% opacity
    cairo_fill_preserve(m_cr);

    // Border
    cairo_set_line_width(m_cr, 2.0);
    setColor(0x4080FFFF);  // Blue border
    cairo_stroke(m_cr);

    // Draw volume icon (speaker symbol)
    const int icon_x = x + 20;
    const int icon_y = y + 20;
    const int icon_size = 40;

    cairo_save(m_cr);

    if (muted) {
        // Muted icon (speaker with X)
        setColor(0xFF4040FF);  // Red for muted

        // Speaker body
        cairo_move_to(m_cr, icon_x, icon_y + 10);
        cairo_line_to(m_cr, icon_x + 10, icon_y + 10);
        cairo_line_to(m_cr, icon_x + 20, icon_y);
        cairo_line_to(m_cr, icon_x + 20, icon_y + icon_size);
        cairo_line_to(m_cr, icon_x + 10, icon_y + 30);
        cairo_line_to(m_cr, icon_x, icon_y + 30);
        cairo_close_path(m_cr);
        cairo_fill(m_cr);

        // X mark
        cairo_set_line_width(m_cr, 3.0);
        cairo_move_to(m_cr, icon_x + 25, icon_y + 8);
        cairo_line_to(m_cr, icon_x + 37, icon_y + 32);
        cairo_stroke(m_cr);
        cairo_move_to(m_cr, icon_x + 37, icon_y + 8);
        cairo_line_to(m_cr, icon_x + 25, icon_y + 32);
        cairo_stroke(m_cr);
    } else {
        // Normal volume icon (speaker with sound waves)
        setColor(0x4080FFFF);  // Blue

        // Speaker body
        cairo_move_to(m_cr, icon_x, icon_y + 10);
        cairo_line_to(m_cr, icon_x + 10, icon_y + 10);
        cairo_line_to(m_cr, icon_x + 20, icon_y);
        cairo_line_to(m_cr, icon_x + 20, icon_y + icon_size);
        cairo_line_to(m_cr, icon_x + 10, icon_y + 30);
        cairo_line_to(m_cr, icon_x, icon_y + 30);
        cairo_close_path(m_cr);
        cairo_fill(m_cr);

        // Sound waves (based on volume level)
        cairo_set_line_width(m_cr, 2.5);
        cairo_set_line_cap(m_cr, CAIRO_LINE_CAP_ROUND);

        if (level > 0) {
            // First wave (always visible if not muted)
            cairo_arc(m_cr, icon_x + 20, icon_y + 20, 8, -30 * degrees, 30 * degrees);
            cairo_stroke(m_cr);
        }

        if (level > 33) {
            // Second wave
            cairo_arc(m_cr, icon_x + 20, icon_y + 20, 14, -30 * degrees, 30 * degrees);
            cairo_stroke(m_cr);
        }

        if (level > 66) {
            // Third wave (high volume)
            cairo_arc(m_cr, icon_x + 20, icon_y + 20, 20, -30 * degrees, 30 * degrees);
            cairo_stroke(m_cr);
        }
    }

    cairo_restore(m_cr);

    // Volume level text
    const int text_x = x + 20;
    const int text_y = y + 70;

    cairo_save(m_cr);
    setFont(m_config.font_family, 36);

    char volume_text[32];
    if (muted) {
        snprintf(volume_text, sizeof(volume_text), "MUTED");
        setColor(0xFF4040FF);  // Red
    } else {
        snprintf(volume_text, sizeof(volume_text), "%d", level);
        setColor(0xFFFFFFFF);  // White
    }

    pango_layout_set_text(m_layout, volume_text, -1);
    cairo_move_to(m_cr, text_x, text_y);
    pango_cairo_show_layout(m_cr, m_layout);
    cairo_restore(m_cr);

    // Volume bar
    if (!muted) {
        const int bar_x = x + 100;
        const int bar_y = y + 80;
        const int bar_width = 170;
        const int bar_height = 20;

        // Background
        drawRectangle(bar_x, bar_y, bar_width, bar_height, 0x404040FF, true);

        // Fill based on volume level
        int fill_width = (level * bar_width) / 100;

        // Color gradient based on level
        uint32_t bar_color;
        if (level < 33) {
            bar_color = 0x40FF40FF;  // Green (low)
        } else if (level < 66) {
            bar_color = 0xFFFF40FF;  // Yellow (medium)
        } else {
            bar_color = 0xFF8040FF;  // Orange (high)
        }

        drawRectangle(bar_x, bar_y, fill_width, bar_height, bar_color, true);

        // Border
        cairo_set_line_width(m_cr, 1.5);
        setColor(0xFFFFFFFF);
        cairo_rectangle(m_cr, bar_x, bar_y, bar_width, bar_height);
        cairo_stroke(m_cr);
    }

    // Pop group and apply opacity
    cairo_pop_group_to_source(m_cr);
    cairo_paint_with_alpha(m_cr, opacity);

    cairo_restore(m_cr);
}

const uint8_t* OSDRenderer::getSurfaceData() const {
    if (!m_initialized || !m_surface) {
        return nullptr;
    }

    return cairo_image_surface_get_data(m_surface);
}

size_t OSDRenderer::getSurfaceDataSize() const {
    if (!m_initialized || !m_surface) {
        return 0;
    }

    int stride = cairo_image_surface_get_stride(m_surface);
    return stride * m_height;
}

// OSDCompositor implementation

OSDCompositor::OSDCompositor() = default;

OSDCompositor::~OSDCompositor() {
    shutdown();
}

Result OSDCompositor::initialize(VkDevice device, VkPhysicalDevice physical_device) {
    if (m_initialized) {
        shutdown();
    }

    m_device = device;
    m_physical_device = physical_device;

    // TODO: Create Vulkan resources for compositing
    // - Descriptor set layout
    // - Pipeline for alpha blending
    // - Sampler for OSD texture

    m_initialized = true;
    return Result::SUCCESS;
}

void OSDCompositor::shutdown() {
    if (!m_initialized) return;

    // Cleanup Vulkan resources
    if (m_osd_view != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device, m_osd_view, nullptr);
        m_osd_view = VK_NULL_HANDLE;
    }

    if (m_osd_image != VK_NULL_HANDLE) {
        vkDestroyImage(m_device, m_osd_image, nullptr);
        m_osd_image = VK_NULL_HANDLE;
    }

    if (m_osd_memory != VK_NULL_HANDLE) {
        vkFreeMemory(m_device, m_osd_memory, nullptr);
        m_osd_memory = VK_NULL_HANDLE;
    }

    m_initialized = false;
}

Result OSDCompositor::composite(const VideoFrame& video, const uint8_t* osd_data,
                                uint32_t osd_width, uint32_t osd_height,
                                VideoFrame& output, const OSDConfig& config) {
    if (!m_initialized) {
        return Result::ERROR_NOT_INITIALIZED;
    }

    // TODO: Implement Vulkan-based compositing
    // 1. Upload OSD data to GPU texture
    // 2. Create compute shader or graphics pipeline for alpha blending
    // 3. Composite OSD over video with proper positioning and scaling
    // 4. Handle OSD position (config.position)
    // 5. Apply opacity (config.opacity)

    // For now, just copy video to output (no OSD)
    output = video;

    return Result::SUCCESS;
}

} // namespace osd
} // namespace ares
