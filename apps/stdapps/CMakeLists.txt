add_library(stdapps STATIC
    counter_app.c
    launcher_app.c
)

target_include_directories(stdapps PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/apps
    ${CMAKE_SOURCE_DIR}/kernel
    ${CMAKE_SOURCE_DIR}/drivers
    ${CMAKE_SOURCE_DIR}/fs
    ${CMAKE_SOURCE_DIR}/hal/include
    ${CMAKE_SOURCE_DIR}/sim/models
    ${CMAKE_SOURCE_DIR}/sim
)

target_link_libraries(stdapps PUBLIC apps)