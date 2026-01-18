#pragma once

#define ARES_VERSION_MAJOR 1
#define ARES_VERSION_MINOR 0
#define ARES_VERSION_PATCH 0
#define ARES_VERSION_STRING "1.0.0"

namespace ares {

constexpr int VERSION_MAJOR = ARES_VERSION_MAJOR;
constexpr int VERSION_MINOR = ARES_VERSION_MINOR;
constexpr int VERSION_PATCH = ARES_VERSION_PATCH;
constexpr const char* VERSION_STRING = ARES_VERSION_STRING;

} // namespace ares
