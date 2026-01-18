#include "config_manager.h"
#include "core/logger.h"

namespace ares {
namespace config {

ConfigManager::ConfigManager() {
    LOG_INFO("Config", "ConfigManager created (stub)");
}

Result ConfigManager::loadConfig(const std::string& config_path) {
    LOG_INFO("Config", "Loading config from %s (stub)", config_path.c_str());
    m_loaded = true;
    return Result::SUCCESS;
}

Result ConfigManager::saveConfig(const std::string& config_path) {
    LOG_INFO("Config", "Saving config to %s (stub)", config_path.c_str());
    return Result::SUCCESS;
}

} // namespace config
} // namespace ares
