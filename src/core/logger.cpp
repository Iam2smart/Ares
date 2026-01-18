#include "logger.h"
#include <cstdarg>
#include <ctime>

namespace ares {
namespace core {

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

void Logger::setLevel(LogLevel level) {
    m_level = level;
}

void Logger::log(LogLevel level, const char* module, const char* format, ...) {
    if (level < m_level) {
        return;
    }

    // Get timestamp
    time_t now = time(nullptr);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    // Level string
    const char* level_str;
    switch (level) {
        case LogLevel::DEBUG: level_str = "DEBUG"; break;
        case LogLevel::INFO:  level_str = "INFO "; break;
        case LogLevel::WARN:  level_str = "WARN "; break;
        case LogLevel::ERROR: level_str = "ERROR"; break;
        default: level_str = "?????"; break;
    }

    // Print prefix
    fprintf(stderr, "[%s] [%s] [%s] ", timestamp, level_str, module);

    // Print message
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n");
    fflush(stderr);
}

} // namespace core
} // namespace ares
