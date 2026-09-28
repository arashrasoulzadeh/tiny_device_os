add_library(kernel STATIC
    scheduler.c
    host_stack.c
    os_time.c
    alloc.c
    event.c
    power.c
)

target_include_directories(kernel PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/hal/include
)

target_compile_options(kernel PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang,AppleClang>:-Wall;-Wextra;-Wpedantic;-Werror;-fno-common;-ffunction-sections;-fdata-sections>
    $<$<C_COMPILER_ID:MSVC>:/W4;/WX>
)

target_link_libraries(kernel PUBLIC hal)