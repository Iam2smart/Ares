# Test tools for Ares development

# OSD test tool - test OSD without DeckLink capture
add_executable(test_osd
    test_osd.cpp
)

target_link_libraries(test_osd
    ares_core
    ares_osd
    ares_input
    ares_display
    ${Vulkan_LIBRARIES}
    ${LIBPLACEBO_LIBRARIES}
    ${LIBDRM_LIBRARIES}
    ${LIBUDEV_LIBRARIES}
    ${LIBEVDEV_LIBRARIES}
    Threads::Threads
)

# Install to build directory for testing
install(TARGETS test_osd
    RUNTIME DESTINATION ${CMAKE_BINARY_DIR}/tools
)
