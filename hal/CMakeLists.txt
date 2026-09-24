add_library(hal STATIC)

target_include_directories(hal PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(ARDUBOT_BUILD_SIM)
    add_subdirectory(arch/sim)
endif()