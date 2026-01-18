# OSD (On-Screen Display) library
# Cairo-based 2D rendering and menu system

find_package(PkgConfig REQUIRED)
pkg_check_modules(CAIRO REQUIRED cairo)
pkg_check_modules(PANGO REQUIRED pangocairo)

add_library(ares_osd STATIC
    osd_renderer.cpp
    osd_config.cpp
    vulkan_osd.cpp
    menu_system.cpp
)

target_include_directories(ares_osd PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${Vulkan_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
    ${PANGO_INCLUDE_DIRS}
)

target_link_libraries(ares_osd
    ares_core
    ${Vulkan_LIBRARIES}
    ${CAIRO_LIBRARIES}
    ${PANGO_LIBRARIES}
)

target_compile_options(ares_osd PRIVATE ${CAIRO_CFLAGS_OTHER} ${PANGO_CFLAGS_OTHER})
