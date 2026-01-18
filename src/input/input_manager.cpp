#include "input_manager.h"
#include "core/logger.h"

namespace ares {
namespace input {

InputManager::InputManager() {
    LOG_INFO("Input", "InputManager created (stub)");
}

InputManager::~InputManager() = default;

Result InputManager::initialize(const std::string& device_path) {
    LOG_INFO("Input", "Initializing input device %s (stub)", device_path.c_str());
    m_initialized = true;
    return Result::SUCCESS;
}

Result InputManager::pollInput() {
    // Stub implementation
    return Result::SUCCESS;
}

} // namespace input
} // namespace ares
