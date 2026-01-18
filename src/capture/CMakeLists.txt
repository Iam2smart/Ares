add_library(ares_capture STATIC
    decklink_capture.cpp
    frame_buffer.cpp
)

target_include_directories(ares_capture PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${DECKLINK_SDK_PATH}
)

target_link_libraries(ares_capture
    ares_core
    ${CMAKE_DL_LIBS}
    Threads::Threads
)
