# Build capture library with optional DeckLink support
set(CAPTURE_SOURCES
    frame_buffer.cpp
)

if(DECKLINK_FOUND)
    list(APPEND CAPTURE_SOURCES decklink_capture.cpp)
endif()

add_library(ares_capture STATIC ${CAPTURE_SOURCES})

target_include_directories(ares_capture PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

if(DECKLINK_FOUND)
    target_include_directories(ares_capture PUBLIC ${DECKLINK_SDK_PATH})
    target_compile_definitions(ares_capture PUBLIC ARES_HAS_DECKLINK)
endif()

target_link_libraries(ares_capture
    ares_core
    ${CMAKE_DL_LIBS}
    Threads::Threads
)
