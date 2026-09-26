cmake_minimum_required(VERSION 3.20)

# Simple FatFS wrapper using hal_storage
# For production, replace with actual FatFS library
add_library(fatfs_lib STATIC
    fatfs_vfs.c
)

target_include_directories(fatfs_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/fs
    ${CMAKE_SOURCE_DIR}/hal/include
)

target_compile_definitions(fatfs_lib PRIVATE
    _POSIX_C_SOURCE=200809L
)