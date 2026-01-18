add_library(ares_config STATIC
    config_manager.cpp
)

target_include_directories(ares_config PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(ares_config
    ares_core
)
