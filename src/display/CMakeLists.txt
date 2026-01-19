add_library(ares_display STATIC
    drm_display.cpp
    vulkan_presenter.cpp
    edid_parser.cpp
    frame_rate_matcher.cpp
)

target_include_directories(ares_display PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${LIBDRM_INCLUDE_DIRS}
    ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(ares_display
    ares_core
    ares_capture
    ${LIBDRM_LIBRARIES}
    ${Vulkan_LIBRARIES}
)
