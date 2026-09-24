# Toolchain configurations for cross-compilation targets
# This file is included optionally by CMakeLists.txt

# Host/Simulator toolchain (native)
function(ardubot_setup_host_toolchain)
    if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures" FORCE)
    endif()
    
    # Find SDL2 using CMake's find_package (provides imported targets)
    find_package(SDL2 REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)
    
    # Find PortAudio library
    find_library(PORTAUDIO_LIBRARY portaudio)
    if(NOT PORTAUDIO_LIBRARY)
        find_library(PORTAUDIO_LIBRARY portaudio-2.0)
    endif()
    
    set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIRS} CACHE INTERNAL "SDL2 include dirs" FORCE)
    set(SDL2_LIBRARIES SDL2::SDL2 CACHE INTERNAL "SDL2 libraries" FORCE)
    set(PORTAUDIO_INCLUDE_DIRS ${PORTAUDIO_INCLUDE_DIRS} CACHE INTERNAL "PortAudio include dirs" FORCE)
    set(PORTAUDIO_LIBRARIES ${PORTAUDIO_LIBRARY} CACHE INTERNAL "PortAudio libraries" FORCE)
    
    if(ARDUBOT_SIM_BACKEND STREQUAL "sdl2")
        add_definitions(-DARDUBOT_SIM_SDL2=1)
    endif()
endfunction()

# ESP32 toolchain (using ESP-IDF or bare metal Xtensa GCC)
function(ardubot_setup_esp32_toolchain)
    set(CMAKE_SYSTEM_NAME Generic)
    set(CMAKE_SYSTEM_PROCESSOR xtensa)
    
    # Try to find ESP-IDF or use bare metal toolchain
    if(DEFINED ENV{IDF_PATH})
        message(STATUS "Using ESP-IDF from $ENV{IDF_PATH}")
        set(ESP_IDF_PATH $ENV{IDF_PATH})
        include(${ESP_IDF_PATH}/tools/cmake/idf.cmake)
    else()
        # Bare metal Xtensa GCC
        set(CMAKE_C_COMPILER xtensa-esp32-elf-gcc)
        set(CMAKE_CXX_COMPILER xtensa-esp32-elf-g++)
        set(CMAKE_ASM_COMPILER xtensa-esp32-elf-gcc)
        set(CMAKE_OBJCOPY xtensa-esp32-elf-objcopy)
        set(CMAKE_SIZE xtensa-esp32-elf-size)
        
        # ESP32 linker script and startup
        set(ESP32_LINKER_SCRIPT ${ARDUBOT_ROOT_DIR}/boards/esp32-devkitc/esp32.ld)
        set(ESP32_STARTUP ${ARDUBOT_ROOT_DIR}/boards/esp32-devkitc/startup_esp32.S)
        
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T ${ESP32_LINKER_SCRIPT}")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mlongcalls -ffunction-sections -fdata-sections")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mlongcalls -ffunction-sections -fdata-sections")
    endif()
    
    add_definitions(-DARDUBOT_TARGET_ESP32=1 -DFREERTOS=1)
endfunction()

# ESP8266 toolchain
function(ardubot_setup_esp8266_toolchain)
    set(CMAKE_SYSTEM_NAME Generic)
    set(CMAKE_SYSTEM_PROCESSOR xtensa)
    
    set(CMAKE_C_COMPILER xtensa-lx106-elf-gcc)
    set(CMAKE_CXX_COMPILER xtensa-lx106-elf-g++)
    set(CMAKE_ASM_COMPILER xtensa-lx106-elf-gcc)
    set(CMAKE_OBJCOPY xtensa-lx106-elf-objcopy)
    set(CMAKE_SIZE xtensa-lx106-elf-size)
    
    set(ESP8266_LINKER_SCRIPT ${ARDUBOT_ROOT_DIR}/boards/nodemcu/esp8266.ld)
    set(ESP8266_STARTUP ${ARDUBOT_ROOT_DIR}/boards/nodemcu/startup_esp8266.S)
    
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -T ${ESP8266_LINKER_SCRIPT}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mlongcalls -ffunction-sections -fdata-sections -DARDUBOT_TARGET_ESP8266=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mlongcalls -ffunction-sections -fdata-sections -DARDUBOT_TARGET_ESP8266=1")
endfunction()

# AVR toolchain (Mega2560)
function(ardubot_setup_avr_toolchain)
    set(CMAKE_SYSTEM_NAME Generic)
    set(CMAKE_SYSTEM_PROCESSOR avr)
    
    set(CMAKE_C_COMPILER avr-gcc)
    set(CMAKE_CXX_COMPILER avr-g++)
    set(CMAKE_ASM_COMPILER avr-gcc)
    set(CMAKE_OBJCOPY avr-objcopy)
    set(CMAKE_SIZE avr-size)
    
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -mmcu=atmega2560 -DF_CPU=16000000UL -DARDUBOT_TARGET_AVR=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mmcu=atmega2560 -DF_CPU=16000000UL -DARDUBOT_TARGET_AVR=1")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -mmcu=atmega2560 -Wl,--gc-sections")
    
    # AVR size check
    add_custom_target(check-size
        COMMAND avr-size -C -d --mcu=atmega2560 $<TARGET_FILE:ardubot-firmware>
        COMMENT "Checking AVR firmware size"
    )
endfunction()

# Select toolchain based on build options
if(ARDUBOT_BUILD_SIM)
    ardubot_setup_host_toolchain()
endif()

if(ARDUBOT_BUILD_ESP32)
    ardubot_setup_esp32_toolchain()
endif()

if(ARDUBOT_BUILD_ESP8266)
    ardubot_setup_esp8266_toolchain()
endif()

if(ARDUBOT_BUILD_AVR)
    ardubot_setup_avr_toolchain()
endif()

# Coverage flags for host builds
if(ARDUBOT_BUILD_SIM AND CMAKE_BUILD_TYPE STREQUAL "Debug")
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_C_COMPILER_ID STREQUAL "Clang")
        set(COVERAGE_FLAGS "-fprofile-arcs -ftest-coverage")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${COVERAGE_FLAGS}")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${COVERAGE_FLAGS}")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${COVERAGE_FLAGS}")
    endif()
endif()