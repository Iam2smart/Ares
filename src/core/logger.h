#pragma once

#include <string>
#include <cstdio>

namespace ares {
namespace core {

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR
};

class Logger {
public:
    static Logger& getInstance();

    void setLevel(LogLevel level);
    void log(LogLevel level, const char* module, const char* format, ...);

private:
    Logger() = default;
    LogLevel m_level = LogLevel::INFO;
};

// Helper macros
#define LOG_DEBUG(module, ...) ares::core::Logger::getInstance().log(ares::core::LogLevel::DEBUG, module, __VA_ARGS__)
#define LOG_INFO(module, ...) ares::core::Logger::getInstance().log(ares::core::LogLevel::INFO, module, __VA_ARGS__)
#define LOG_WARN(module, ...) ares::core::Logger::getInstance().log(ares::core::LogLevel::WARN, module, __VA_ARGS__)
#define LOG_ERROR(module, ...) ares::core::Logger::getInstance().log(ares::core::LogLevel::ERROR, module, __VA_ARGS__)

} // namespace core
} // namespace ares
