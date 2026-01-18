add_library(ares_sync STATIC
    master_clock.cpp
    frame_scheduler.cpp
)

target_include_directories(ares_sync PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ares_sync
    ares_core
    Threads::Threads
)
