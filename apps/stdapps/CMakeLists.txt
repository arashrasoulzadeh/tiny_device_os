add_library(stdapps STATIC
    counter/counter_app.c
    counter/counter_icon.c
    launcher/launcher_app.c
    launcher/launcher_icon.c
    info/info_app.c
    info/info_icon.c
    stopwatch/stopwatch_app.c
    stopwatch/stopwatch.c
    stopwatch/stopwatch_icon.c
    pong/pong_app.c
    pong/pong.c
    pong/pong_icon.c
    settings/settings_app.c
    settings/settings_icon.c
    fileman/fileman_app.c
    fileman/fileman_icon.c
    shell/shell_app.c
    shell/shell_icon.c
    demo/demo_app.c
    demo/demo_icon.c
)

target_include_directories(stdapps PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/counter
    ${CMAKE_CURRENT_SOURCE_DIR}/launcher
    ${CMAKE_CURRENT_SOURCE_DIR}/info
    ${CMAKE_CURRENT_SOURCE_DIR}/stopwatch
    ${CMAKE_CURRENT_SOURCE_DIR}/pong
    ${CMAKE_CURRENT_SOURCE_DIR}/settings
    ${CMAKE_CURRENT_SOURCE_DIR}/fileman
    ${CMAKE_CURRENT_SOURCE_DIR}/shell
    ${CMAKE_CURRENT_SOURCE_DIR}/demo
    ${CMAKE_SOURCE_DIR}/apps
    ${CMAKE_SOURCE_DIR}/kernel
    ${CMAKE_SOURCE_DIR}/drivers
    ${CMAKE_SOURCE_DIR}/fs
    ${CMAKE_SOURCE_DIR}/hal/include
    ${CMAKE_SOURCE_DIR}/sim/models
    ${CMAKE_SOURCE_DIR}/sim
)

target_link_libraries(stdapps PUBLIC apps)
