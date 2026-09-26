add_library(stdapps STATIC
    counter/counter_app.c
    counter/counter_icon.c
    launcher/launcher_app.c
    info/info_app.c
    info/info_icon.c
    stopwatch/stopwatch_app.c
    stopwatch/stopwatch_icon.c
)

target_include_directories(stdapps PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/counter
    ${CMAKE_CURRENT_SOURCE_DIR}/launcher
    ${CMAKE_CURRENT_SOURCE_DIR}/info
    ${CMAKE_CURRENT_SOURCE_DIR}/stopwatch
    ${CMAKE_SOURCE_DIR}/apps
    ${CMAKE_SOURCE_DIR}/kernel
    ${CMAKE_SOURCE_DIR}/drivers
    ${CMAKE_SOURCE_DIR}/fs
    ${CMAKE_SOURCE_DIR}/hal/include
    ${CMAKE_SOURCE_DIR}/sim/models
    ${CMAKE_SOURCE_DIR}/sim
)

target_link_libraries(stdapps PUBLIC apps)
