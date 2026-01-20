#pragma once

#include <ares/types.h>
#include <ares/osd_config.h>
#include "osd_renderer.h"
#include "../input/ir_remote.h"
#include <memory>
#include <chrono>

namespace ares {
namespace osd {

// Menu system with madVR Envy-style navigation
class MenuSystem {
public:
    MenuSystem();
    ~MenuSystem();

    // Initialize with renderer and remote
    Result initialize(OSDRenderer* renderer, input::IRRemote* remote,
                     const OSDConfig& config);

    // Shutdown
    void shutdown();

    // Load menu structure
    Result loadMenu(const OSDMenuStructure& menu);

    // Show/hide menu
    void show();
    void hide();
    void toggle();
    bool isVisible() const { return m_visible; }

    // Update (handles timeouts, animations)
    void update(float delta_time_ms);

    // Render current menu state
    void render();

    // Navigation
    void navigateUp();
    void navigateDown();
    void navigateLeft();
    void navigateRight();
    void navigateTab(int tab_index);
    void selectCurrent();
    void goBack();

    // Value adjustment
    void adjustValue(float delta);

    // Get current menu/item
    Menu* getCurrentMenu();
    MenuItem* getCurrentItem();
    int getCurrentTabIndex() const { return m_current_tab; }
    int getCurrentItemIndex() const { return m_current_item; }

    // Configuration
    void setConfig(const OSDConfig& config) { m_config = config; }
    const OSDConfig& getConfig() const { return m_config; }

    // Access menu structure (for wiring values and callbacks)
    OSDMenuStructure& getMenuStructure() { return m_menu; }
    const OSDMenuStructure& getMenuStructure() const { return m_menu; }

    // Update dynamic info items (GPU stats, input info, etc.)
    void updateGPUPerformanceInfo(double frame_time_ms, double avg_frame_time_ms);
    void updateInfoItem(const std::string& item_id, const std::string& value);

    // Statistics
    struct Stats {
        uint64_t inputs_processed = 0;
        uint64_t menu_opens = 0;
        uint64_t items_selected = 0;
        double avg_render_time_ms = 0.0;
    };

    Stats getStats() const { return m_stats; }

private:
    // Navigation state
    struct NavState {
        int tab = 0;
        int item = 0;
        int scroll_offset = 0;
        std::vector<NavState> history;  // For going back
    };

    // Rendering helpers
    void renderTabs();
    void renderMenuItems();
    void renderTooltip();
    void renderBackground();
    void renderScrollbar();

    // Layout calculations
    int calculateMenuWidth();
    int calculateMenuHeight();
    int calculateVisibleItems();
    void updateScrollOffset();

    // Input handling
    void handleButton(const input::RemoteButton& button, bool pressed);

    // Auto-hide timer
    void resetTimeout();
    bool isTimedOut() const;

    // Menu structure
    OSDMenuStructure m_menu;
    NavState m_nav_state;

    // Components
    OSDRenderer* m_renderer = nullptr;
    input::IRRemote* m_remote = nullptr;

    // Configuration
    OSDConfig m_config;

    // State
    bool m_initialized = false;
    bool m_visible = false;
    bool m_adjusting_value = false;

    // Timeout tracking
    std::chrono::steady_clock::time_point m_last_activity;
    float m_timeout_accumulator = 0.0f;

    // Current navigation
    int m_current_tab = 0;
    int m_current_item = 0;
    int m_scroll_offset = 0;

    // Statistics
    Stats m_stats;

    // Animation state
    float m_animation_progress = 0.0f;
};

} // namespace osd
} // namespace ares
