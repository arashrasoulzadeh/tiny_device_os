cmake_minimum_required(VERSION 3.20)

# LittleFS library
add_library(littlefs_lib STATIC
    ${CMAKE_SOURCE_DIR}/third_party/littlefs/lfs.c
    ${CMAKE_SOURCE_DIR}/third_party/littlefs/lfs_util.c
    littlefs_vfs.c
)

target_include_directories(littlefs_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/fs
    ${CMAKE_SOURCE_DIR}/hal/include
    ${CMAKE_SOURCE_DIR}/third_party/littlefs
)

target_compile_definitions(littlefs_lib PRIVATE
    # Not LFS_THREADSAFE=0: littlefs checks this with #ifdef, not by value,
    # so defining it at all (even as 0) enables the locking code path,
    # which calls cfg->lock()/cfg->unlock() - function pointers nothing
    # here ever sets, since the scheduler is single-threaded and doesn't
    # need them. That null call is what crashed lfs_mount() immediately.
    LFS_READ_SIZE=16
    LFS_PROG_SIZE=16
    LFS_BLOCK_SIZE=4096
    LFS_LOOKAHEAD=16
    LFS_CACHE_SIZE=16
)