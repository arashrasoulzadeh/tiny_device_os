# Toolchain configurations for cross-compilation targets
# This file is included optionally by CMakeLists.txt

# Host/Simulator toolchain (native)
function(ardubot_setup_host_toolchain)
    message(STATUS "ardubot_setup_host_toolchain() called")
    
    if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
        # Prefer vcpkg CONFIG packages (CI uses vcpkg + CMAKE_TOOLCHAIN_FILE).
        # choco's sdl2/portaudio packages ship runtime DLLs only, with no
        # headers or import libs, so they cannot satisfy an MSVC build.
        find_package(SDL2 CONFIG QUIET)
        if(SDL2_FOUND OR TARGET SDL2::SDL2)
            set(SDL2_LIBRARIES SDL2::SDL2 SDL2::SDL2main CACHE INTERNAL "SDL2 libraries" FORCE)
        else()
            # Fallback: manual search (e.g. a self-hosted runner with SDL2 dev files staged)
            find_path(SDL2_INCLUDE_DIR SDL.h
                PATHS "C:/Program Files/SDL2/include" "C:/tools/SDL2/include" "C:/msys64/mingw64/include" "C:/mingw64/include"
                PATH_SUFFIXES SDL2
            )
            find_library(SDL2_LIBRARY NAMES SDL2 SDL2main
                PATHS "C:/Program Files/SDL2/lib" "C:/tools/SDL2/lib" "C:/msys64/mingw64/lib" "C:/mingw64/lib"
            )
            if(SDL2_INCLUDE_DIR AND SDL2_LIBRARY)
                set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIR} CACHE INTERNAL "SDL2 include dirs" FORCE)
                set(SDL2_LIBRARIES ${SDL2_LIBRARY} CACHE INTERNAL "SDL2 libraries" FORCE)
            else()
                message(FATAL_ERROR "SDL2 not found. Install with: vcpkg install sdl2 (and pass -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake)")
            endif()
        endif()

        find_package(portaudio CONFIG QUIET)
        if(TARGET portaudio)
            set(PORTAUDIO_LIBRARIES portaudio CACHE INTERNAL "PortAudio libraries" FORCE)
        elseif(TARGET portaudio_static)
            set(PORTAUDIO_LIBRARIES portaudio_static CACHE INTERNAL "PortAudio libraries" FORCE)
        else()
            find_library(PORTAUDIO_LIBRARY NAMES portaudio
                PATHS "C:/Program Files/PortAudio/lib" "C:/tools/portaudio/lib" "C:/msys64/mingw64/lib" "C:/mingw64/lib"
            )
            find_path(PORTAUDIO_INCLUDE_DIR portaudio.h
                PATHS "C:/Program Files/PortAudio/include" "C:/tools/portaudio/include" "C:/msys64/mingw64/include" "C:/mingw64/include"
            )
            if(PORTAUDIO_INCLUDE_DIR AND PORTAUDIO_LIBRARY)
                set(PORTAUDIO_INCLUDE_DIRS ${PORTAUDIO_INCLUDE_DIR} CACHE INTERNAL "PortAudio include dirs" FORCE)
                set(PORTAUDIO_LIBRARIES ${PORTAUDIO_LIBRARY} CACHE INTERNAL "PortAudio libraries" FORCE)
            else()
                message(FATAL_ERROR "PortAudio not found. Install with: vcpkg install portaudio (and pass -DCMAKE_TOOLCHAIN_FILE=<vcpkg>/scripts/buildsystems/vcpkg.cmake)")
            endif()
        endif()
    else()
        # Linux/macOS: use pkg-config
        if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
            # Build native arch only (arm64 on Apple Silicon) - Homebrew libs are single-arch
            set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "macOS architectures" FORCE)
        endif()
        
        message(STATUS "Finding SDL2 and PortAudio via pkg-config on Linux/macOS")
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(SDL2 REQUIRED sdl2)
        pkg_check_modules(PORTAUDIO REQUIRED portaudio-2.0)
        
        set(SDL2_INCLUDE_DIRS ${SDL2_INCLUDE_DIRS} CACHE INTERNAL "SDL2 include dirs" FORCE)
        set(SDL2_LIBRARIES ${SDL2_LIBRARIES} CACHE INTERNAL "SDL2 libraries" FORCE)
        set(PORTAUDIO_INCLUDE_DIRS ${PORTAUDIO_INCLUDE_DIRS} CACHE INTERNAL "PortAudio include dirs" FORCE)
        set(PORTAUDIO_LIBRARIES ${PORTAUDIO_LIBRARIES} CACHE INTERNAL "PortAudio libraries" FORCE)
        
        # Add _POSIX_C_SOURCE for usleep, ftruncate etc.
        add_compile_definitions(_POSIX_C_SOURCE=200809L)
    endif()
    
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