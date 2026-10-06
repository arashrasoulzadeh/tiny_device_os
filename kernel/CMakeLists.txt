add_library(kernel STATIC
    scheduler.c
    host_stack.c
    os_time.c
    os_clock.c
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
    # /wd4324: jmp_buf on x64 Windows needs 16-byte alignment (SSE register
    # save area), which pads any struct that embeds one (scheduler_t,
    # task_tcb_t) - an unavoidable, harmless consequence of using setjmp/
    # longjmp for cooperative task switching here, not a real code issue.
    $<$<C_COMPILER_ID:MSVC>:/W4;/WX;/wd4324>
)

target_compile_definitions(kernel PRIVATE
    # MSVC deprecates the plain CRT string functions (strncpy, etc.) in
    # favor of its own non-portable _s variants; this codebase targets
    # standard C across gcc/clang/MSVC and uses strncpy deliberately
    # everywhere else, so silence the deprecation rather than fork every
    # call site onto an MSVC-only API.
    $<$<C_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
)

target_link_libraries(kernel PUBLIC hal)