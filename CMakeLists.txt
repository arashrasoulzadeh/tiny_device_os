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

# Wi-Fi SSID/password from gitignored device_secrets.yaml. Missing file → radio off.
set(ARDUBOT_SECRETS_HEADER "${CMAKE_BINARY_DIR}/generated/device_secrets.h")
set(ARDUBOT_SECRETS_YAML "${CMAKE_SOURCE_DIR}/device_secrets.yaml")
file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")
if(EXISTS "${ARDUBOT_SECRETS_YAML}")
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(NOT Python3_EXECUTABLE)
        message(FATAL_ERROR "device_secrets.yaml is present but Python 3 was not found")
    endif()
    execute_process(
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/scripts/device_secrets.py" gen-header
                --secrets "${ARDUBOT_SECRETS_YAML}" -o "${ARDUBOT_SECRETS_HEADER}"
        RESULT_VARIABLE _ardubot_secrets_rc
        ERROR_VARIABLE _ardubot_secrets_err
    )
    if(NOT _ardubot_secrets_rc EQUAL 0)
        message(FATAL_ERROR "device_secrets.h generation failed:\n${_ardubot_secrets_err}")
    endif()
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${ARDUBOT_SECRETS_YAML}")
else()
    file(WRITE "${ARDUBOT_SECRETS_HEADER}"
         "/* Auto-generated — no device_secrets.yaml */\n"
         "#pragma once\n"
         "#define ARDUBOT_WIFI_HAS_CREDS 0\n"
         "#define ARDUBOT_WIFI_SSID \"\"\n"
         "#define ARDUBOT_WIFI_PASSWORD \"\"\n")
endif()

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
