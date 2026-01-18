add_library(ares_osd STATIC
    vulkan_osd.cpp
    menu_system.cpp
)

target_include_directories(ares_osd PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${Vulkan_INCLUDE_DIRS}
)

target_link_libraries(ares_osd
    ares_core
    ${Vulkan_LIBRARIES}
)
