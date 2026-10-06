add_library(apps STATIC
    syscall.c
    app.c
    input.c
    ui.c
    app_utils.c
    app_kit.c
    ardubot_keys.c
    device_info.c
    stdlog.c
    app_ui.c
    app_helper.c
    app_runtime.c
    clock_service.c
    sensor_service.c
    stdapps_register.c
    ui/components/canvas.c
    ui/components/menu.c
    ui/components/catalog.c
    ui/components/screen.c
    ui/components/icons.c
    ui/components/status.c
)

# Panel size for the active device (override with -DARDUBOT_DISPLAY_WIDTH/HEIGHT).
# Defaults match repo-root device_config.yaml (NodeMCU SSD1306 128x32).
if(NOT DEFINED ARDUBOT_DISPLAY_WIDTH)
    set(ARDUBOT_DISPLAY_WIDTH 128)
endif()
if(NOT DEFINED ARDUBOT_DISPLAY_HEIGHT)
    set(ARDUBOT_DISPLAY_HEIGHT 32)
endif()

# Simulator uses larger logical resolution for better UI development
if(ARDUBOT_BUILD_SIM)
    set(ARDUBOT_DISPLAY_WIDTH 320)
    set(ARDUBOT_DISPLAY_HEIGHT 172)
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
    ${CMAKE_BINARY_DIR}/generated
)

target_link_libraries(apps PUBLIC kernel drivers fs)

if(ARDUBOT_BUILD_SIM)
    # apps/app_framework.h's inline helpers and apps/app_ui.c call
    # sim_gpio_*/ssd1306_model_* directly (apps/app_framework.h already
    # includes sim/'s headers above, just never declared the matching
    # library dependency) - sim/CMakeLists.txt links `apps` PUBLIC the
    # other way round (for the built-in app manifests), so this is a
    # genuine circular static-library dependency, not a mistake to
    # avoid. CMake handles a declared cycle between static libraries
    # correctly (repeating/grouping them on the final link line); only
    # declaring it on sim's side left Linux's linker unable to resolve
    # apps's sim_gpio_*/ssd1306_model_* calls whenever something links
    # `sim` before `apps` gets pulled in transitively (confirmed via a
    # real CI link failure in test_app_ui - clang/macOS's linker is more
    # forgiving about library order than GNU ld, which is why this
    # never surfaced locally).
    target_link_libraries(apps PUBLIC sim)
endif()

target_compile_definitions(apps PUBLIC
    ARDUBOT_VERSION_STR="${PROJECT_VERSION}"
    APP_DISPLAY_WIDTH=${ARDUBOT_DISPLAY_WIDTH}
    APP_DISPLAY_HEIGHT=${ARDUBOT_DISPLAY_HEIGHT}
)

if(ARDUBOT_BUILD_SIM)
    target_compile_definitions(apps PUBLIC ARDUBOT_SIM_SDL2=1)
endif()

add_subdirectory(stdapps)