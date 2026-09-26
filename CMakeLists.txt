cmake_minimum_required(VERSION 3.20)
project(ArdubotOS VERSION 0.1.0 LANGUAGES C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# Emit compile_commands.json so clangd / IDE indexing can resolve includes & references.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic -Werror -fno-common -ffunction-sections -fdata-sections")
    set(CMAKE_C_FLAGS_DEBUG "-Og -g3 -ggdb")
    set(CMAKE_C_FLAGS_RELEASE "-Os -DNDEBUG")
endif()

option(ARDUBOT_BUILD_SIM "Build SDL2 simulator for host" ON)
option(ARDUBOT_BUILD_ESP32 "Build for ESP32" OFF)
option(ARDUBOT_BUILD_ESP8266 "Build for ESP8266" OFF)
option(ARDUBOT_BUILD_AVR "Build for AVR (Mega2560)" OFF)
option(ARDUBOT_SIM_BACKEND "Simulator backend" "sdl2")

set(ARDUBOT_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(ARDUBOT_BUILD_TYPE ${CMAKE_BUILD_TYPE})

include(${ARDUBOT_ROOT_DIR}/cmake/toolchain.cmake OPTIONAL)
include(${ARDUBOT_ROOT_DIR}/cmake/kconfig.cmake)

add_subdirectory(kernel)
add_subdirectory(hal)
add_subdirectory(fs)
add_subdirectory(drivers)
add_subdirectory(modules)
add_subdirectory(game)
add_subdirectory(apps)
add_subdirectory(sim)
add_subdirectory(tools)
add_subdirectory(tests)

enable_testing()
add_custom_target(test-all
    COMMAND ctest --output-on-failure
    COMMAND ${CMAKE_COMMAND} -P ${ARDUBOT_ROOT_DIR}/scripts/check_coverage.cmake
    DEPENDS ardubot-sim
    COMMENT "Running all tests with coverage gate"
)
