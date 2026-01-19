add_library(ares_input STATIC
    input_manager.cpp
    ir_remote.cpp
    receiver_control.cpp
)

target_include_directories(ares_input PUBLIC
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${LIBEVDEV_INCLUDE_DIRS}
)

target_link_libraries(ares_input
    ares_core
    ${LIBEVDEV_LIBRARIES}
)
