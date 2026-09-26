add_library(apps STATIC
    syscall.c
    app.c
    input.c
    ui.c
    app_utils.c
    app_kit.c
    device_info.c
    stdlog.c
    ui/components/canvas.c
    ui/components/menu.c
    ui/components/catalog.c
    ui/components/screen.c
)

# Panel size for the active device (override with -DARDUBOT_DISPLAY_WIDTH/HEIGHT).
if(NOT DEFINED ARDUBOT_DISPLAY_WIDTH)
    set(ARDUBOT_DISPLAY_WIDTH 128)
endif()
if(NOT DEFINED ARDUBOT_DISPLAY_HEIGHT)
    set(ARDUBOT_DISPLAY_HEIGHT 64)
endif()

target_include_directories(apps PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/ui/components
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
    APP_DISPLAY_WIDTH=${ARDUBOT_DISPLAY_WIDTH}
    APP_DISPLAY_HEIGHT=${ARDUBOT_DISPLAY_HEIGHT}
)

if(ARDUBOT_BUILD_SIM)
    target_compile_definitions(apps PUBLIC ARDUBOT_SIM_SDL2=1)
endif()

add_subdirectory(stdapps)