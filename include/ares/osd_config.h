#pragma once

#include <string>
#include <vector>
#include <functional>

namespace ares {

// OSD Menu system (madVR Envy style)
// Multi-tab interface with IR remote control

// Menu item types
enum class MenuItemType {
    SUBMENU,        // Opens another menu/tab
    TOGGLE,         // Boolean on/off
    SLIDER,         // Float value with slider
    INTEGER,        // Integer value
    ENUM,           // Selection from list
    ACTION,         // Execute action (e.g., save preset)
    INFO            // Display-only information
};

// Menu item definition
struct MenuItem {
    std::string id;                         // Unique identifier
    std::string label;                      // Display label
    MenuItemType type;                      // Item type

    // For submenus
    std::string submenu_id;                 // ID of submenu to open

    // For toggles
    bool* toggle_value = nullptr;

    // For sliders/integers
    float* float_value = nullptr;
    int* int_value = nullptr;
    float min_value = 0.0f;
    float max_value = 1.0f;
    float step = 0.1f;
    std::string unit;                       // Display unit (%, dB, ms, etc.)

    // For enums
    int* enum_value = nullptr;
    std::vector<std::string> enum_options;

    // For actions
    std::function<void()> action;

    // For info
    std::string info_text;
    std::function<std::string()> info_callback; // Dynamic info

    // Value change callback (called when menu value changes)
    std::function<void()> on_change;

    // Display settings
    bool enabled = true;                    // Item is selectable
    bool visible = true;                    // Item is visible
    std::string tooltip;                    // Help text
    std::string icon;                       // Item icon
    std::string value;                      // Current value display
    bool has_submenu = false;               // Has submenu indicator

    // Conditional visibility
    std::function<bool()> visibility_condition;
};

// Menu/Tab definition
struct Menu {
    std::string id;                         // Unique identifier
    std::string title;                      // Tab/menu title
    std::vector<MenuItem> items;            // Menu items
    std::string icon;                       // Optional icon
};

// OSD Configuration (madVR Envy style)
struct OSDConfig {
    // Display settings
    bool enabled = true;
    float opacity = 0.9f;                   // OSD opacity (0.0-1.0)
    int position_x = 100;                   // Screen position X
    int position_y = 100;                   // Screen position Y

    // Appearance
    std::string font_family = "Sans";
    int font_size = 24;
    uint32_t text_color = 0xFFFFFFFF;           // RGBA
    uint32_t background_color = 0xE0000000;     // RGBA
    uint32_t highlight_color = 0xFF00AAFF;      // RGBA
    uint32_t border_color = 0xFF666666;         // RGBA

    // Text appearance
    bool text_shadow = true;
    uint32_t shadow_color = 0x000000FF;         // RGBA
    uint32_t selected_text_color = 0xFFFFFFFF;  // RGBA
    uint32_t disabled_text_color = 0x808080FF;  // RGBA
    uint32_t value_color = 0xAAAAAAFF;          // RGBA

    // Tab appearance
    uint32_t tab_active_bg = 0xFF1A1A1AFF;
    uint32_t tab_inactive_bg = 0xFF0A0A0AFF;
    uint32_t tab_active_text = 0xFFFFFFFF;
    uint32_t tab_inactive_text = 0xAAAAAAFF;

    // Selection and UI elements
    uint32_t selection_color = 0x8000AAFF;      // RGBA
    uint32_t slider_bg_color = 0xFF404040FF;
    uint32_t slider_fill_color = 0xFF00AAFF;
    uint32_t slider_handle_color = 0xFFFFFFFF;

    // Layout
    int item_height = 40;
    int item_spacing = 5;
    int margin = 20;
    int tab_height = 60;
    int max_visible_items = 12;

    // Behavior
    int timeout_ms = 5000;                  // Auto-hide timeout (0 = never)
    bool show_values = true;                // Show current values
    bool show_tooltips = true;
    bool animate_transitions = true;

    // Statistics overlay
    bool show_stats = false;
    int stats_position_x = 50;
    int stats_position_y = 50;
    int stats_update_ms = 500;
};

// Main OSD menu structure (madVR Envy style)
// Organized in tabs like: Processing | Display | Color | Audio | Misc
struct OSDMenuStructure {
    std::vector<Menu> tabs;

    // Tab indices for quick access
    enum TabIndex {
        TAB_PROCESSING = 0,
        TAB_SCALING,
        TAB_TONE_MAPPING,
        TAB_COLOR,
        TAB_ENHANCEMENTS,
        TAB_DISPLAY,
        TAB_MISC,
        TAB_INFO
    };

    // Build the complete menu structure
    void build();

    // Navigation
    Menu* getTab(int index);
    Menu* getTab(const std::string& id);
    MenuItem* getItem(const std::string& tab_id, const std::string& item_id);
};

// Create default OSD menu structure
OSDMenuStructure createDefaultOSDMenu();

} // namespace ares
