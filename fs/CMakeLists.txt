add_library(fs STATIC
    vfs.c
    config_store.c
    ota.c
)

target_include_directories(fs PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(littlefs)
add_subdirectory(fatfs)

target_link_libraries(fs PUBLIC littlefs_lib fatfs_lib)