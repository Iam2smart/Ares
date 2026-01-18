add_library(ares_processing STATIC
    vulkan_context.cpp
    placebo_renderer.cpp
    black_bar_detector.cpp
    nls_shader.cpp
    processing_pipeline.cpp
)

target_include_directories(ares_processing PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${Vulkan_INCLUDE_DIRS}
    ${LIBPLACEBO_INCLUDE_DIRS}
)

target_link_libraries(ares_processing
    ares_core
    ${Vulkan_LIBRARIES}
    ${LIBPLACEBO_LIBRARIES}
)
