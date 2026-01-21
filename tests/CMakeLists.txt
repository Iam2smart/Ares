# Test executables
add_executable(test_capture test_capture.cpp)

target_include_directories(test_capture PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${DECKLINK_SDK_PATH}
)

target_link_libraries(test_capture
    ares_core
    ares_capture
    ares_sync
    ${CMAKE_DL_LIBS}
    Threads::Threads
)

# Processing pipeline test
add_executable(test_processing test_processing.cpp)

target_include_directories(test_processing PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_processing
    ares_core
    ares_processing
    ares_osd
    ${Vulkan_LIBRARIES}
    ${LIBPLACEBO_LIBRARIES}
    Threads::Threads
)

# Display output test
add_executable(test_display test_display.cpp)

target_include_directories(test_display PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(test_display
    ares_core
    ares_processing
    ares_display
    ${Vulkan_LIBRARIES}
    ${LIBPLACEBO_LIBRARIES}
    ${LIBDRM_LIBRARIES}
    Threads::Threads
)

# Install test executables (optional)
install(TARGETS test_capture test_processing test_display DESTINATION bin)
