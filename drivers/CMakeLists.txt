add_library(drivers INTERFACE)

target_include_directories(drivers INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(display)
add_subdirectory(sensor)