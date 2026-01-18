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
    Threads::Threads
)

# Install test executables (optional)
install(TARGETS test_capture test_processing DESTINATION bin)
