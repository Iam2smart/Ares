#include "menu_system.h"
#include "core/logger.h"

namespace ares {
namespace osd {

MenuSystem::MenuSystem() {
    LOG_INFO("OSD", "MenuSystem created (stub)");
}

Result MenuSystem::initialize() {
    LOG_INFO("OSD", "Initializing menu system (stub)");
    m_initialized = true;
    return Result::SUCCESS;
}

void MenuSystem::handleInput(int key) {
    // Stub implementation
}

} // namespace osd
} // namespace ares
