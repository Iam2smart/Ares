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

# Install test executable (optional)
install(TARGETS test_capture DESTINATION bin)
