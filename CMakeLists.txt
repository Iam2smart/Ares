cmake_minimum_required(VERSION 3.20)
project(Ares VERSION 1.0.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build options
option(ARES_BUILD_TESTS "Build unit tests" ON)
option(ARES_ENABLE_PROFILING "Enable performance profiling" OFF)
option(ARES_ENABLE_VALIDATION "Enable Vulkan validation layers" OFF)

# Find dependencies
find_package(Vulkan REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBPLACEBO REQUIRED libplacebo>=6.338)
pkg_check_modules(LIBDRM REQUIRED libdrm)
pkg_check_modules(LIBUDEV REQUIRED libudev)
pkg_check_modules(LIBEVDEV REQUIRED libevdev)

# DeckLink SDK (manual path)
set(DECKLINK_SDK_PATH "/usr/local/include/DeckLinkAPI" CACHE PATH "Path to DeckLink SDK")
set(DECKLINK_LIB_PATH "/usr/local/lib" CACHE PATH "Path to DeckLink libraries")

# Threads
find_package(Threads REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${Vulkan_INCLUDE_DIRS}
    ${LIBPLACEBO_INCLUDE_DIRS}
    ${LIBDRM_INCLUDE_DIRS}
    ${LIBUDEV_INCLUDE_DIRS}
    ${LIBEVDEV_INCLUDE_DIRS}
    ${DECKLINK_SDK_PATH}
)

# Compiler flags
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Werror=return-type
    -march=native
    -O3
)

if(ARES_ENABLE_PROFILING)
    add_compile_definitions(ARES_ENABLE_PROFILING)
endif()

# Subdirectories
add_subdirectory(src/core)
add_subdirectory(src/capture)
add_subdirectory(src/processing)
add_subdirectory(src/display)
add_subdirectory(src/sync)
add_subdirectory(src/osd)
add_subdirectory(src/input)
add_subdirectory(src/config)

# Main executable
add_executable(ares src/main.cpp)
target_link_libraries(ares
    ares_core
    ares_capture
    ares_processing
    ares_display
    ares_sync
    ares_osd
    ares_input
    ares_config
    ${Vulkan_LIBRARIES}
    ${LIBPLACEBO_LIBRARIES}
    ${LIBDRM_LIBRARIES}
    ${LIBUDEV_LIBRARIES}
    ${LIBEVDEV_LIBRARIES}
    ${CMAKE_DL_LIBS}
    Threads::Threads
)

# Install targets
install(TARGETS ares DESTINATION bin)
install(DIRECTORY config/ DESTINATION /etc/ares)
install(FILES systemd/ares.service DESTINATION /etc/systemd/system)

# Tools (test utilities)
add_subdirectory(tools)

# Tests
if(ARES_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
