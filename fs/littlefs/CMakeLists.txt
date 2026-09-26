cmake_minimum_required(VERSION 3.20)

# LittleFS library
add_library(littlefs_lib STATIC
    ${CMAKE_SOURCE_DIR}/third_party/littlefs/lfs.c
    littlefs_vfs.c
)

target_include_directories(littlefs_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/fs
    ${CMAKE_SOURCE_DIR}/hal/include
    ${CMAKE_SOURCE_DIR}/third_party/littlefs
)

target_compile_definitions(littlefs_lib PRIVATE
    LFS_THREADSAFE=0
    LFS_READ_SIZE=16
    LFS_PROG_SIZE=16
    LFS_BLOCK_SIZE=4096
    LFS_LOOKAHEAD=16
    LFS_CACHE_SIZE=16
)