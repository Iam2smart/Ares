add_library(ares_core STATIC
    logger.cpp
    thread_pool.cpp
)

target_include_directories(ares_core PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ares_core
    Threads::Threads
)
