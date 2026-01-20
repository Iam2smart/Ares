#include "config_manager.h"
#include "core/logger.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

namespace ares {
namespace config {

ConfigManager::ConfigManager() {
    LOG_INFO("Config", "ConfigManager created");
}

AresConfig ConfigManager::getDefaultConfig() {
    AresConfig config;

    // Capture defaults
    config.capture.device_index = 0;
    config.capture.input_connection = "HDMI";
    config.capture.buffer_size = 3;

    // Processing defaults
    config.processing.tone_mapping.algorithm = ToneMappingAlgorithm::BT2390;
    config.processing.tone_mapping.target_nits = 100.0f;
    config.processing.tone_mapping.source_nits = 1000.0f;
    config.processing.tone_mapping.contrast = 1.0f;
    config.processing.tone_mapping.saturation = 1.0f;

    config.processing.nls.enabled = false;
    config.processing.nls.horizontal_stretch = 0.5f;
    config.processing.nls.vertical_stretch = 0.5f;
    config.processing.nls.horizontal_power = 2.0f;
    config.processing.nls.vertical_power = 2.0f;

    config.processing.black_bars.enabled = true;
    config.processing.black_bars.auto_crop = true;

    config.processing.dithering.enabled = true;
    config.processing.dithering.method = DitheringConfig::Method::BLUE_NOISE;

    config.processing.debanding.enabled = false;
    config.processing.debanding.iterations = 2;
    config.processing.debanding.threshold = 16.0f;

    config.processing.chroma_upscaling.enabled = true;
    config.processing.chroma_upscaling.algorithm = ScalingAlgorithm::EWA_LANCZOS;

    // Display defaults
    config.display.connector = "auto";
    config.display.card = "/dev/dri/card0";
    config.display.mode.width = 3840;
    config.display.mode.height = 2160;
    config.display.mode.refresh_rate = 60.0f;

    // OSD defaults
    config.osd.enabled = true;
    config.osd.opacity = 0.9f;
    config.osd.font_family = "Sans";
    config.osd.font_size = 24;
    config.osd.timeout_ms = 10000;

    // Receiver defaults
    config.receiver.enabled = false;
    config.receiver.ip_address = "192.168.1.100";
    config.receiver.port = 60128;
    config.receiver.max_volume = 80;

    // System defaults
    config.log_level = "INFO";
    config.log_to_file = true;
    config.log_file = "/var/log/ares/ares.log";
    config.thread_count = 4;

    return config;
}

Result ConfigManager::loadConfig(const std::string& config_path, AresConfig& config) {
    // Start with defaults
    config = getDefaultConfig();

    // Try to load from file
    return loadFromIni(config_path, config);
}

Result ConfigManager::loadFromIni(const std::string& config_path, AresConfig& config) {
    LOG_INFO("Config", "Loading configuration from %s", config_path.c_str());

    // Read file
    std::ifstream file(config_path);
    if (!file.is_open()) {
        LOG_WARN("Config", "Configuration file not found, using defaults");
        return Result::SUCCESS;  // Not an error, just use defaults
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    // Parse INI
    auto ini = parseIni(content);

    // Load capture config
    config.capture.device_index = getIntValue(ini, "capture", "device_index", 0);
    config.capture.input_connection = getValue(ini, "capture", "input_connection", "HDMI");
    config.capture.buffer_size = getIntValue(ini, "capture", "buffer_size", 3);

    // Load tone mapping config
    std::string tm_algo = getValue(ini, "tone_mapping", "algorithm", "bt2390");
    if (tm_algo == "bt2390") config.processing.tone_mapping.algorithm = ToneMappingAlgorithm::BT2390;
    else if (tm_algo == "reinhard") config.processing.tone_mapping.algorithm = ToneMappingAlgorithm::REINHARD;
    else if (tm_algo == "hable") config.processing.tone_mapping.algorithm = ToneMappingAlgorithm::HABLE;
    else if (tm_algo == "mobius") config.processing.tone_mapping.algorithm = ToneMappingAlgorithm::MOBIUS;

    config.processing.tone_mapping.target_nits = getFloatValue(ini, "tone_mapping", "target_nits", 100.0f);
    config.processing.tone_mapping.source_nits = getFloatValue(ini, "tone_mapping", "source_nits", 1000.0f);
    config.processing.tone_mapping.contrast = getFloatValue(ini, "tone_mapping", "contrast", 1.0f);
    config.processing.tone_mapping.saturation = getFloatValue(ini, "tone_mapping", "saturation", 1.0f);

    // Load NLS config
    config.processing.nls.enabled = getBoolValue(ini, "nls", "enabled", false);
    config.processing.nls.horizontal_stretch = getFloatValue(ini, "nls", "horizontal_stretch", 0.5f);
    config.processing.nls.vertical_stretch = getFloatValue(ini, "nls", "vertical_stretch", 0.5f);
    config.processing.nls.horizontal_power = getFloatValue(ini, "nls", "horizontal_power", 2.0f);
    config.processing.nls.vertical_power = getFloatValue(ini, "nls", "vertical_power", 2.0f);

    // Load black bars config
    config.processing.black_bars.enabled = getBoolValue(ini, "black_bars", "enabled", true);
    config.processing.black_bars.auto_crop = getBoolValue(ini, "black_bars", "auto_crop", true);
    config.processing.black_bars.threshold = getIntValue(ini, "black_bars", "threshold", 16);

    // Load dithering config
    config.processing.dithering.enabled = getBoolValue(ini, "dithering", "enabled", true);
    std::string dither_method = getValue(ini, "dithering", "method", "blue_noise");
    if (dither_method == "blue_noise") config.processing.dithering.method = DitheringConfig::Method::BLUE_NOISE;
    else if (dither_method == "white_noise") config.processing.dithering.method = DitheringConfig::Method::WHITE_NOISE;
    else if (dither_method == "ordered") config.processing.dithering.method = DitheringConfig::Method::ORDERED;
    else if (dither_method == "error_diffusion") config.processing.dithering.method = DitheringConfig::Method::ERROR_DIFFUSION;

    // Load debanding config
    config.processing.debanding.enabled = getBoolValue(ini, "debanding", "enabled", false);
    config.processing.debanding.iterations = getIntValue(ini, "debanding", "iterations", 2);
    config.processing.debanding.threshold = getFloatValue(ini, "debanding", "threshold", 16.0f);
    config.processing.debanding.grain = getFloatValue(ini, "debanding", "grain", 0.3f);

    // Load chroma upscaling config
    config.processing.chroma_upscaling.enabled = getBoolValue(ini, "chroma", "enabled", true);

    // Load display config
    config.display.connector = getValue(ini, "display", "connector", "auto");
    config.display.card = getValue(ini, "display", "card", "/dev/dri/card0");
    config.display.mode.width = getIntValue(ini, "display", "width", 3840);
    config.display.mode.height = getIntValue(ini, "display", "height", 2160);
    config.display.mode.refresh_rate = getFloatValue(ini, "display", "refresh_rate", 60.0f);

    // Load OSD config
    config.osd.enabled = getBoolValue(ini, "osd", "enabled", true);
    config.osd.opacity = getFloatValue(ini, "osd", "opacity", 0.9f);
    config.osd.font_family = getValue(ini, "osd", "font_family", "Sans");
    config.osd.font_size = getIntValue(ini, "osd", "font_size", 24);
    config.osd.timeout_ms = static_cast<int>(getFloatValue(ini, "osd", "timeout", 10.0f) * 1000);

    // Load receiver config
    config.receiver.enabled = getBoolValue(ini, "receiver", "enabled", false);
    config.receiver.ip_address = getValue(ini, "receiver", "ip_address", "192.168.1.100");
    config.receiver.port = getIntValue(ini, "receiver", "port", 60128);
    config.receiver.max_volume = getIntValue(ini, "receiver", "max_volume", 80);

    // Load system config
    config.log_level = getValue(ini, "system", "log_level", "INFO");
    config.log_to_file = getBoolValue(ini, "system", "log_to_file", true);
    config.log_file = getValue(ini, "system", "log_file", "/var/log/ares/ares.log");
    config.thread_count = getIntValue(ini, "system", "thread_count", 4);

    m_loaded = true;
    LOG_INFO("Config", "Configuration loaded successfully");
    return Result::SUCCESS;
}

Result ConfigManager::saveConfig(const std::string& config_path, const AresConfig& config) {
    return saveToIni(config_path, config);
}

Result ConfigManager::saveToIni(const std::string& config_path, const AresConfig& config) {
    LOG_INFO("Config", "Saving configuration to %s", config_path.c_str());

    std::ofstream file(config_path);
    if (!file.is_open()) {
        LOG_ERROR("Config", "Failed to open configuration file for writing: %s", config_path.c_str());
        return Result::ERROR_WRITE_FAILED;
    }

    file << "# Ares HDR Video Processor Configuration\n";
    file << "# Generated by ConfigManager\n\n";

    // Capture section
    file << "[capture]\n";
    file << "device_index = " << config.capture.device_index << "\n";
    file << "input_connection = " << config.capture.input_connection << "\n";
    file << "buffer_size = " << config.capture.buffer_size << "\n\n";

    // Tone mapping section
    file << "[tone_mapping]\n";
    file << "algorithm = ";
    switch (config.processing.tone_mapping.algorithm) {
        case ToneMappingAlgorithm::BT2390: file << "bt2390"; break;
        case ToneMappingAlgorithm::REINHARD: file << "reinhard"; break;
        case ToneMappingAlgorithm::HABLE: file << "hable"; break;
        case ToneMappingAlgorithm::MOBIUS: file << "mobius"; break;
        default: file << "bt2390"; break;
    }
    file << "\n";
    file << "target_nits = " << config.processing.tone_mapping.target_nits << "\n";
    file << "source_nits = " << config.processing.tone_mapping.source_nits << "\n";
    file << "contrast = " << config.processing.tone_mapping.contrast << "\n";
    file << "saturation = " << config.processing.tone_mapping.saturation << "\n\n";

    // NLS section
    file << "[nls]\n";
    file << "enabled = " << (config.processing.nls.enabled ? "true" : "false") << "\n";
    file << "horizontal_stretch = " << config.processing.nls.horizontal_stretch << "\n";
    file << "vertical_stretch = " << config.processing.nls.vertical_stretch << "\n";
    file << "horizontal_power = " << config.processing.nls.horizontal_power << "\n";
    file << "vertical_power = " << config.processing.nls.vertical_power << "\n\n";

    // Black bars section
    file << "[black_bars]\n";
    file << "enabled = " << (config.processing.black_bars.enabled ? "true" : "false") << "\n";
    file << "auto_crop = " << (config.processing.black_bars.auto_crop ? "true" : "false") << "\n";
    file << "threshold = " << config.processing.black_bars.threshold << "\n\n";

    // Dithering section
    file << "[dithering]\n";
    file << "enabled = " << (config.processing.dithering.enabled ? "true" : "false") << "\n";
    file << "method = ";
    switch (config.processing.dithering.method) {
        case DitheringConfig::Method::BLUE_NOISE: file << "blue_noise"; break;
        case DitheringConfig::Method::WHITE_NOISE: file << "white_noise"; break;
        case DitheringConfig::Method::ORDERED: file << "ordered"; break;
        case DitheringConfig::Method::ERROR_DIFFUSION: file << "error_diffusion"; break;
        default: file << "blue_noise"; break;
    }
    file << "\n\n";

    // Debanding section
    file << "[debanding]\n";
    file << "enabled = " << (config.processing.debanding.enabled ? "true" : "false") << "\n";
    file << "iterations = " << config.processing.debanding.iterations << "\n";
    file << "threshold = " << config.processing.debanding.threshold << "\n";
    file << "grain = " << config.processing.debanding.grain << "\n\n";

    // Chroma section
    file << "[chroma]\n";
    file << "enabled = " << (config.processing.chroma_upscaling.enabled ? "true" : "false") << "\n\n";

    // Display section
    file << "[display]\n";
    file << "connector = " << config.display.connector << "\n";
    file << "card = " << config.display.card << "\n";
    file << "width = " << config.display.mode.width << "\n";
    file << "height = " << config.display.mode.height << "\n";
    file << "refresh_rate = " << config.display.mode.refresh_rate << "\n\n";

    // OSD section
    file << "[osd]\n";
    file << "enabled = " << (config.osd.enabled ? "true" : "false") << "\n";
    file << "opacity = " << config.osd.opacity << "\n";
    file << "font_family = " << config.osd.font_family << "\n";
    file << "font_size = " << config.osd.font_size << "\n";
    file << "timeout = " << (config.osd.timeout_ms / 1000.0f) << "\n\n";

    // Receiver section
    file << "[receiver]\n";
    file << "enabled = " << (config.receiver.enabled ? "true" : "false") << "\n";
    file << "ip_address = " << config.receiver.ip_address << "\n";
    file << "port = " << config.receiver.port << "\n";
    file << "max_volume = " << config.receiver.max_volume << "\n\n";

    // System section
    file << "[system]\n";
    file << "log_level = " << config.log_level << "\n";
    file << "log_to_file = " << (config.log_to_file ? "true" : "false") << "\n";
    file << "log_file = " << config.log_file << "\n";
    file << "thread_count = " << config.thread_count << "\n";

    file.close();
    LOG_INFO("Config", "Configuration saved successfully");
    return Result::SUCCESS;
}

std::map<std::string, std::map<std::string, std::string>>
ConfigManager::parseIni(const std::string& content) {
    std::map<std::string, std::map<std::string, std::string>> result;
    std::istringstream stream(content);
    std::string line;
    std::string current_section;

    while (std::getline(stream, line)) {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        // Skip empty lines and comments
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        // Section header
        if (line[0] == '[' && line[line.length() - 1] == ']') {
            current_section = line.substr(1, line.length() - 2);
            continue;
        }

        // Key = value
        size_t eq_pos = line.find('=');
        if (eq_pos != std::string::npos) {
            std::string key = line.substr(0, eq_pos);
            std::string value = line.substr(eq_pos + 1);

            // Trim key and value
            key.erase(0, key.find_first_not_of(" \t"));
            key.erase(key.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);

            result[current_section][key] = value;
        }
    }

    return result;
}

std::string ConfigManager::getValue(
    const std::map<std::string, std::map<std::string, std::string>>& ini,
    const std::string& section, const std::string& key, const std::string& default_value) {

    auto section_it = ini.find(section);
    if (section_it == ini.end()) {
        return default_value;
    }

    auto key_it = section_it->second.find(key);
    if (key_it == section_it->second.end()) {
        return default_value;
    }

    return key_it->second;
}

int ConfigManager::getIntValue(
    const std::map<std::string, std::map<std::string, std::string>>& ini,
    const std::string& section, const std::string& key, int default_value) {

    std::string value = getValue(ini, section, key, "");
    if (value.empty()) {
        return default_value;
    }

    try {
        return std::stoi(value);
    } catch (...) {
        return default_value;
    }
}

float ConfigManager::getFloatValue(
    const std::map<std::string, std::map<std::string, std::string>>& ini,
    const std::string& section, const std::string& key, float default_value) {

    std::string value = getValue(ini, section, key, "");
    if (value.empty()) {
        return default_value;
    }

    try {
        return std::stof(value);
    } catch (...) {
        return default_value;
    }
}

bool ConfigManager::getBoolValue(
    const std::map<std::string, std::map<std::string, std::string>>& ini,
    const std::string& section, const std::string& key, bool default_value) {

    std::string value = getValue(ini, section, key, "");
    if (value.empty()) {
        return default_value;
    }

    // Convert to lowercase
    std::transform(value.begin(), value.end(), value.begin(), ::tolower);

    if (value == "true" || value == "yes" || value == "1" || value == "on") {
        return true;
    } else if (value == "false" || value == "no" || value == "0" || value == "off") {
        return false;
    }

    return default_value;
}

} // namespace config
} // namespace ares
