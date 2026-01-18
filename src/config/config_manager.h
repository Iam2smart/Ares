#pragma once

#include <ares/types.h>
#include <string>

namespace ares {
namespace config {

class ConfigManager {
public:
    ConfigManager();
    Result loadConfig(const std::string& config_path);
    Result saveConfig(const std::string& config_path);

private:
    bool m_loaded = false;
};

} // namespace config
} // namespace ares
