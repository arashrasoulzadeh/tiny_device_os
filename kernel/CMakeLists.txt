add_library(kernel STATIC
    scheduler.c
    os_time.c
    alloc.c
)

target_include_directories(kernel PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/hal/include
)

target_compile_options(kernel PRIVATE
    -Wall -Wextra -Wpedantic -Werror
    -fno-common -ffunction-sections -fdata-sections
)

target_link_libraries(kernel PUBLIC hal)