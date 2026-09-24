add_library(fs INTERFACE)

target_include_directories(fs INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(littlefs)
add_subdirectory(fatfs)