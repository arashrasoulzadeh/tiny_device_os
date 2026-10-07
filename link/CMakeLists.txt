add_library(link STATIC
    link_frame.c
    link_crypto.c
    link_session.c
    link_dispatch.c
)

target_include_directories(link PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
