#include "menu_system.h"
#include "core/logger.h"
#include <algorithm>

namespace ares {
namespace osd {

MenuSystem::MenuSystem() = default;

MenuSystem::~MenuSystem() {
    shutdown();
}

Result MenuSystem::initialize(OSDRenderer* renderer, input::IRRemote* remote,
                              const OSDConfig& config) {
    if (m_initialized) {
        shutdown();
    }

    if (!renderer || !remote) {
        return Result::ERROR_INVALID_PARAMETER;
    }

    m_renderer = renderer;
    m_remote = remote;
    m_config = config;

    // Set up IR remote callback
    m_remote->setButtonCallback([this](const input::ButtonEvent& event) {
        handleButton(event.button, event.pressed);
    });

    m_last_activity = std::chrono::steady_clock::now();
    m_initialized = true;

    LOG_INFO("OSD", "Menu system initialized");
    return Result::SUCCESS;
}

void MenuSystem::shutdown() {
    if (!m_initialized) return;

    m_renderer = nullptr;
    m_remote = nullptr;
    m_initialized = false;

    LOG_INFO("OSD", "Menu system shutdown");
}

Result MenuSystem::loadMenu(const OSDMenuStructure& menu) {
    m_menu = menu;
    m_current_tab = 0;
    m_current_item = 0;
    m_scroll_offset = 0;

    LOG_INFO("OSD", "Loaded menu with %zu tabs", m_menu.tabs.size());
    return Result::SUCCESS;
}

void MenuSystem::show() {
    if (!m_visible) {
        m_visible = true;
        m_animation_progress = 0.0f;
        resetTimeout();
        m_stats.menu_opens++;
        LOG_DEBUG("OSD", "Menu shown");
    }
}

void MenuSystem::hide() {
    if (m_visible) {
        m_visible = false;
        LOG_DEBUG("OSD", "Menu hidden");
    }
}

void MenuSystem::toggle() {
    if (m_visible) {
        hide();
    } else {
        show();
    }
}

void MenuSystem::update(float delta_time_ms) {
    if (!m_initialized || !m_visible) return;

    // Update animation
    if (m_config.animate_transitions && m_animation_progress < 1.0f) {
        m_animation_progress += delta_time_ms / 200.0f;  // 200ms animation
        if (m_animation_progress > 1.0f) {
            m_animation_progress = 1.0f;
        }
    }

    // Check timeout
    if (m_config.timeout_ms > 0) {
        m_timeout_accumulator += delta_time_ms;
        if (m_timeout_accumulator >= m_config.timeout_ms) {
            hide();
            m_timeout_accumulator = 0.0f;
        }
    }
}

void MenuSystem::render() {
    if (!m_initialized || !m_visible || !m_renderer) return;

    auto start = std::chrono::high_resolution_clock::now();

    m_renderer->beginFrame();

    renderBackground();
    renderTabs();
    renderMenuItems();

    if (m_config.show_tooltips) {
        renderTooltip();
    }

    renderScrollbar();

    m_renderer->endFrame();

    auto end = std::chrono::high_resolution_clock::now();
    double render_time = std::chrono::duration<double, std::milli>(end - start).count();
    m_stats.avg_render_time_ms = (m_stats.avg_render_time_ms * 0.9) + (render_time * 0.1);
}

void MenuSystem::renderBackground() {
    // Full-screen semi-transparent background
    m_renderer->drawRectangle(0, 0, m_renderer->getWidth(), m_renderer->getHeight(),
                             m_config.background_color & 0xFFFFFF80, true);

    // Menu panel background
    int menu_width = calculateMenuWidth();
    int menu_height = calculateMenuHeight();
    int menu_x = (m_renderer->getWidth() - menu_width) / 2;
    int menu_y = (m_renderer->getHeight() - menu_height) / 2;

    m_renderer->drawRectangle(menu_x, menu_y, menu_width, menu_height,
                             m_config.background_color, true);

    // Border
    m_renderer->drawRectangle(menu_x, menu_y, menu_width, menu_height,
                             m_config.border_color, false);
}

void MenuSystem::renderTabs() {
    if (m_menu.tabs.empty()) return;

    int menu_width = calculateMenuWidth();
    int menu_x = (m_renderer->getWidth() - menu_width) / 2;
    int menu_y = (m_renderer->getHeight() - calculateMenuHeight()) / 2;

    int tab_width = menu_width / static_cast<int>(m_menu.tabs.size());
    int tab_y = menu_y;

    for (size_t i = 0; i < m_menu.tabs.size(); i++) {
        int tab_x = menu_x + (i * tab_width);
        bool active = (i == m_current_tab);

        m_renderer->drawTab(m_menu.tabs[i].title, tab_x, tab_y, tab_width,
                           m_config.tab_height, active, m_config);
    }
}

void MenuSystem::renderMenuItems() {
    Menu* menu = getCurrentMenu();
    if (!menu) return;

    int menu_width = calculateMenuWidth();
    int menu_x = (m_renderer->getWidth() - menu_width) / 2;
    int menu_y = (m_renderer->getHeight() - calculateMenuHeight()) / 2;

    int items_y = menu_y + m_config.tab_height + m_config.margin;
    int visible_items = calculateVisibleItems();

    updateScrollOffset();

    // Render visible items
    int item_index = 0;
    for (size_t i = m_scroll_offset;
         i < menu->items.size() && item_index < visible_items;
         i++, item_index++) {

        MenuItem& item = menu->items[i];
        if (!item.visible) continue;

        // Check visibility condition
        if (item.visibility_condition && !item.visibility_condition()) {
            continue;
        }

        bool selected = (i == m_current_item);
        int item_y = items_y + (item_index * (m_config.item_height + m_config.item_spacing));

        // Update item value display
        if (item.type == MenuItemType::TOGGLE && item.toggle_value) {
            item.value = *item.toggle_value ? "On" : "Off";
        } else if (item.type == MenuItemType::SLIDER && item.float_value) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.2f%s", *item.float_value, item.unit.c_str());
            item.value = buf;
        } else if (item.type == MenuItemType::INTEGER && item.int_value) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%d%s", *item.int_value, item.unit.c_str());
            item.value = buf;
        } else if (item.type == MenuItemType::ENUM && item.enum_value) {
            int idx = *item.enum_value;
            if (idx >= 0 && idx < static_cast<int>(item.enum_options.size())) {
                item.value = item.enum_options[idx];
            }
        }

        // Set has_submenu flag
        item.has_submenu = (item.type == MenuItemType::SUBMENU);

        // Render based on type
        if (item.type == MenuItemType::SLIDER && item.float_value && selected && m_adjusting_value) {
            m_renderer->drawSlider(item.label, *item.float_value, item.min_value,
                                  item.max_value, menu_x + m_config.margin, item_y,
                                  menu_width - 2 * m_config.margin, true, m_config);
        } else {
            m_renderer->drawMenuItem(item, menu_x + m_config.margin, item_y,
                                    menu_width - 2 * m_config.margin,
                                    m_config.item_height, selected, m_config);
        }
    }
}

void MenuSystem::renderTooltip() {
    MenuItem* item = getCurrentItem();
    if (!item || item->tooltip.empty()) return;

    int menu_width = calculateMenuWidth();
    int menu_height = calculateMenuHeight();
    int menu_x = (m_renderer->getWidth() - menu_width) / 2;
    int menu_y = (m_renderer->getHeight() - menu_height) / 2;

    int tooltip_y = menu_y + menu_height + 10;
    int tooltip_x = menu_x;

    // Draw tooltip background
    m_renderer->drawRectangle(tooltip_x, tooltip_y, menu_width, 40,
                             m_config.background_color, true);

    // Draw tooltip text
    m_renderer->drawText(item->tooltip, tooltip_x + 10, tooltip_y + 10, m_config);
}

void MenuSystem::renderScrollbar() {
    Menu* menu = getCurrentMenu();
    if (!menu) return;

    int visible_count = calculateVisibleItems();
    if (static_cast<int>(menu->items.size()) <= visible_count) return;

    int menu_width = calculateMenuWidth();
    int menu_height = calculateMenuHeight();
    int menu_x = (m_renderer->getWidth() - menu_width) / 2;
    int menu_y = (m_renderer->getHeight() - menu_height) / 2;

    int scrollbar_x = menu_x + menu_width - 10;
    int scrollbar_y = menu_y + m_config.tab_height + m_config.margin;
    int scrollbar_height = menu_height - m_config.tab_height - 2 * m_config.margin;

    // Track
    m_renderer->drawRectangle(scrollbar_x, scrollbar_y, 5, scrollbar_height,
                             0x40404080, true);

    // Thumb
    float ratio = (float)visible_count / menu->items.size();
    int thumb_height = std::max(20, (int)(scrollbar_height * ratio));
    float scroll_ratio = (float)m_scroll_offset / (menu->items.size() - visible_count);
    int thumb_y = scrollbar_y + (int)((scrollbar_height - thumb_height) * scroll_ratio);

    m_renderer->drawRectangle(scrollbar_x, thumb_y, 5, thumb_height,
                             m_config.highlight_color, true);
}

int MenuSystem::calculateMenuWidth() {
    return m_renderer->getWidth() * 0.6f;  // 60% of screen width
}

int MenuSystem::calculateMenuHeight() {
    return m_renderer->getHeight() * 0.7f;  // 70% of screen height
}

int MenuSystem::calculateVisibleItems() {
    int available_height = calculateMenuHeight() - m_config.tab_height - 2 * m_config.margin;
    return available_height / (m_config.item_height + m_config.item_spacing);
}

void MenuSystem::updateScrollOffset() {
    int visible_items = calculateVisibleItems();

    // Scroll to keep current item visible
    if (m_current_item < m_scroll_offset) {
        m_scroll_offset = m_current_item;
    } else if (m_current_item >= m_scroll_offset + visible_items) {
        m_scroll_offset = m_current_item - visible_items + 1;
    }

    // Clamp scroll offset
    Menu* menu = getCurrentMenu();
    if (menu) {
        int max_scroll = std::max(0, (int)menu->items.size() - visible_items);
        m_scroll_offset = std::clamp(m_scroll_offset, 0, max_scroll);
    }
}

void MenuSystem::navigateUp() {
    Menu* menu = getCurrentMenu();
    if (!menu || menu->items.empty()) return;

    do {
        m_current_item--;
        if (m_current_item < 0) {
            m_current_item = static_cast<int>(menu->items.size()) - 1;
        }
    } while (!menu->items[m_current_item].visible || !menu->items[m_current_item].enabled);

    resetTimeout();
    LOG_DEBUG("OSD", "Navigate up to item %d", m_current_item);
}

void MenuSystem::navigateDown() {
    Menu* menu = getCurrentMenu();
    if (!menu || menu->items.empty()) return;

    do {
        m_current_item++;
        if (m_current_item >= static_cast<int>(menu->items.size())) {
            m_current_item = 0;
        }
    } while (!menu->items[m_current_item].visible || !menu->items[m_current_item].enabled);

    resetTimeout();
    LOG_DEBUG("OSD", "Navigate down to item %d", m_current_item);
}

void MenuSystem::navigateLeft() {
    if (m_adjusting_value) {
        adjustValue(-0.1f);
    } else {
        // Switch to previous tab
        m_current_tab--;
        if (m_current_tab < 0) {
            m_current_tab = static_cast<int>(m_menu.tabs.size()) - 1;
        }
        m_current_item = 0;
        m_scroll_offset = 0;
        resetTimeout();
        LOG_DEBUG("OSD", "Navigate left to tab %d", m_current_tab);
    }
}

void MenuSystem::navigateRight() {
    if (m_adjusting_value) {
        adjustValue(0.1f);
    } else {
        // Switch to next tab
        m_current_tab++;
        if (m_current_tab >= static_cast<int>(m_menu.tabs.size())) {
            m_current_tab = 0;
        }
        m_current_item = 0;
        m_scroll_offset = 0;
        resetTimeout();
        LOG_DEBUG("OSD", "Navigate right to tab %d", m_current_tab);
    }
}

void MenuSystem::navigateTab(int tab_index) {
    if (tab_index >= 0 && tab_index < static_cast<int>(m_menu.tabs.size())) {
        m_current_tab = tab_index;
        m_current_item = 0;
        m_scroll_offset = 0;
        resetTimeout();
    }
}

void MenuSystem::selectCurrent() {
    MenuItem* item = getCurrentItem();
    if (!item || !item->enabled) return;

    m_stats.items_selected++;

    switch (item->type) {
        case MenuItemType::TOGGLE:
            if (item->toggle_value) {
                *item->toggle_value = !*item->toggle_value;
                LOG_DEBUG("OSD", "Toggled %s to %d", item->label.c_str(), *item->toggle_value);
                // Trigger value change callback
                if (item->on_change) {
                    item->on_change();
                }
            }
            break;

        case MenuItemType::SLIDER:
            m_adjusting_value = !m_adjusting_value;
            LOG_DEBUG("OSD", "Adjusting value for %s: %d", item->label.c_str(), m_adjusting_value);
            break;

        case MenuItemType::ENUM:
            if (item->enum_value && !item->enum_options.empty()) {
                *item->enum_value = (*item->enum_value + 1) % static_cast<int>(item->enum_options.size());
                LOG_DEBUG("OSD", "Changed %s to option %d", item->label.c_str(), *item->enum_value);
                // Trigger value change callback
                if (item->on_change) {
                    item->on_change();
                }
            }
            break;

        case MenuItemType::ACTION:
            if (item->action) {
                item->action();
                LOG_DEBUG("OSD", "Executed action: %s", item->label.c_str());
            }
            break;

        case MenuItemType::SUBMENU:
            // TODO: Navigate to submenu
            LOG_DEBUG("OSD", "Enter submenu: %s", item->label.c_str());
            break;

        default:
            break;
    }

    resetTimeout();
}

void MenuSystem::goBack() {
    if (m_adjusting_value) {
        m_adjusting_value = false;
    } else {
        hide();
    }
    resetTimeout();
}

void MenuSystem::adjustValue(float delta) {
    MenuItem* item = getCurrentItem();
    if (!item) return;

    bool value_changed = false;

    if (item->type == MenuItemType::SLIDER && item->float_value) {
        float step = item->step * delta * 10.0f;  // 10x step for faster adjustment
        float old_value = *item->float_value;
        *item->float_value = std::clamp(*item->float_value + step,
                                       item->min_value, item->max_value);
        value_changed = (*item->float_value != old_value);
        LOG_DEBUG("OSD", "Adjusted %s to %.2f", item->label.c_str(), *item->float_value);
    } else if (item->type == MenuItemType::INTEGER && item->int_value) {
        int step = std::max(1, (int)(item->step * delta * 10.0f));
        int old_value = *item->int_value;
        *item->int_value = std::clamp(*item->int_value + step,
                                     (int)item->min_value, (int)item->max_value);
        value_changed = (*item->int_value != old_value);
        LOG_DEBUG("OSD", "Adjusted %s to %d", item->label.c_str(), *item->int_value);
    }

    // Trigger value change callback
    if (value_changed && item->on_change) {
        item->on_change();
    }

    resetTimeout();
}

void MenuSystem::handleButton(const input::RemoteButton& button, bool pressed) {
    if (!pressed) return;  // Only handle button press, not release

    m_stats.inputs_processed++;

    using RB = input::RemoteButton;

    switch (button) {
        case RB::MENU:
            toggle();
            break;

        case RB::UP:
            if (m_visible) navigateUp();
            break;

        case RB::DOWN:
            if (m_visible) navigateDown();
            break;

        case RB::LEFT:
            if (m_visible) navigateLeft();
            break;

        case RB::RIGHT:
            if (m_visible) navigateRight();
            break;

        case RB::OK:
            if (m_visible) selectCurrent();
            break;

        case RB::BACK:
            if (m_visible) goBack();
            break;

        case RB::NUM_1:
        case RB::NUM_2:
        case RB::NUM_3:
        case RB::NUM_4:
        case RB::NUM_5:
        case RB::NUM_6:
        case RB::NUM_7:
        case RB::NUM_8:
            // Direct tab navigation
            if (m_visible) {
                int tab_idx = static_cast<int>(button) - static_cast<int>(RB::NUM_1);
                navigateTab(tab_idx);
            }
            break;

        default:
            break;
    }
}

void MenuSystem::resetTimeout() {
    m_last_activity = std::chrono::steady_clock::now();
    m_timeout_accumulator = 0.0f;
}

bool MenuSystem::isTimedOut() const {
    if (m_config.timeout_ms <= 0) return false;
    return m_timeout_accumulator >= m_config.timeout_ms;
}

Menu* MenuSystem::getCurrentMenu() {
    if (m_current_tab < 0 || m_current_tab >= static_cast<int>(m_menu.tabs.size())) {
        return nullptr;
    }
    return &m_menu.tabs[m_current_tab];
}

MenuItem* MenuSystem::getCurrentItem() {
    Menu* menu = getCurrentMenu();
    if (!menu || m_current_item < 0 || m_current_item >= static_cast<int>(menu->items.size())) {
        return nullptr;
    }
    return &menu->items[m_current_item];
}

void MenuSystem::updateGPUPerformanceInfo(double frame_time_ms, double avg_frame_time_ms) {
    // Update GPU frame time
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%.2f ms", frame_time_ms);
    updateInfoItem("gpu_frame_time", buffer);

    // Update GPU frame rate (max FPS based on frame time)
    double max_fps = (frame_time_ms > 0.0) ? (1000.0 / frame_time_ms) : 0.0;
    snprintf(buffer, sizeof(buffer), "%.1f FPS", max_fps);
    updateInfoItem("gpu_frame_rate", buffer);

    // Update average frame time
    snprintf(buffer, sizeof(buffer), "%.2f ms", avg_frame_time_ms);
    updateInfoItem("gpu_avg_frame_time", buffer);

    // Update performance status
    const char* status;
    const double target_60fps = 16.67;  // 60 FPS = 16.67ms per frame
    const double target_30fps = 33.33;  // 30 FPS = 33.33ms per frame

    if (frame_time_ms <= 16.0) {
        status = "Excellent (60+ FPS)";
    } else if (frame_time_ms <= target_60fps) {
        status = "Good (60 FPS capable)";
    } else if (frame_time_ms <= 20.0) {
        status = "Acceptable (50+ FPS)";
    } else if (frame_time_ms <= target_30fps) {
        status = "Fair (30+ FPS)";
    } else {
        status = "Poor (< 30 FPS)";
    }

    updateInfoItem("performance_status", status);
}

void MenuSystem::updateInfoItem(const std::string& item_id, const std::string& value) {
    // Find and update the info item in the menu structure
    for (auto& tab : m_menu.tabs) {
        for (auto& item : tab.items) {
            if (item.id == item_id && item.type == MenuItemType::INFO) {
                item.info_text = value;
                return;
            }
        }
    }
}

} // namespace osd
} // namespace ares
