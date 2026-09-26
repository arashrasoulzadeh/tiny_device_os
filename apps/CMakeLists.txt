add_library(apps STATIC
    syscall.c
    app.c
    input.c
    ui.c
    app_utils.c
    stdlog.c
)

target_include_directories(apps PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/kernel
    ${CMAKE_SOURCE_DIR}/drivers
    ${CMAKE_SOURCE_DIR}/fs
    ${CMAKE_SOURCE_DIR}/hal/include
    ${CMAKE_SOURCE_DIR}/sim
)

target_link_libraries(apps PUBLIC kernel drivers fs)

add_subdirectory(stdapps)