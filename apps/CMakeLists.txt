add_library(apps STATIC
    syscall.c
    app.c
    input.c
    ui.c
    app_utils.c
    app_kit.c
    device_info.c
    stdlog.c
)

target_include_directories(apps PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/kernel
    ${CMAKE_SOURCE_DIR}/drivers
    ${CMAKE_SOURCE_DIR}/fs
    ${CMAKE_SOURCE_DIR}/hal/include
    ${CMAKE_SOURCE_DIR}/sim
    ${CMAKE_SOURCE_DIR}/sim/models
)

target_link_libraries(apps PUBLIC kernel drivers fs)

target_compile_definitions(apps PUBLIC
    ARDUBOT_VERSION_STR="${PROJECT_VERSION}"
)

if(ARDUBOT_BUILD_SIM)
    target_compile_definitions(apps PUBLIC ARDUBOT_SIM_SDL2=1)
endif()

add_subdirectory(stdapps)