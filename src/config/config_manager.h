#pragma once

#include <ares/types.h>
#include <ares/ares_config.h>
#include <string>
#include <map>

namespace ares {
namespace config {

class ConfigManager {
public:
    ConfigManager();
    Result loadConfig(const std::string& config_path, AresConfig& config);
    Result saveConfig(const std::string& config_path, const AresConfig& config);

    // Load from INI-style format (simple key=value pairs with sections)
    Result loadFromIni(const std::string& config_path, AresConfig& config);
    Result saveToIni(const std::string& config_path, const AresConfig& config);

    // Get default configuration
    static AresConfig getDefaultConfig();

private:
    bool m_loaded = false;

    // INI parsing helpers
    std::map<std::string, std::map<std::string, std::string>> parseIni(const std::string& content);
    std::string getValue(const std::map<std::string, std::map<std::string, std::string>>& ini,
                        const std::string& section, const std::string& key,
                        const std::string& default_value = "");
    int getIntValue(const std::map<std::string, std::map<std::string, std::string>>& ini,
                   const std::string& section, const std::string& key, int default_value = 0);
    float getFloatValue(const std::map<std::string, std::map<std::string, std::string>>& ini,
                       const std::string& section, const std::string& key, float default_value = 0.0f);
    bool getBoolValue(const std::map<std::string, std::map<std::string, std::string>>& ini,
                     const std::string& section, const std::string& key, bool default_value = false);
};

} // namespace config
} // namespace ares
