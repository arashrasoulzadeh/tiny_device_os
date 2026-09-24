# Kconfig integration for CMake
# Provides menuconfig and generates autoconf.h / auto.conf

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# Kconfiglib location (can be downloaded or installed via pip)
set(KCONFIGLIB_DIR "${ARDUBOT_ROOT_DIR}/tools/kconfiglib" CACHE PATH "Kconfiglib directory")

# Download Kconfiglib if not present
if(NOT EXISTS "${KCONFIGLIB_DIR}/kconfiglib.py")
    message(STATUS "Downloading Kconfiglib...")
    file(DOWNLOAD
        "https://github.com/ulfalizer/Kconfiglib/archive/refs/heads/master.zip"
        "${ARDUBOT_ROOT_DIR}/kconfiglib-master.zip"
        TIMEOUT 30
    )
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xzf "${ARDUBOT_ROOT_DIR}/kconfiglib-master.zip"
        WORKING_DIRECTORY ${ARDUBOT_ROOT_DIR}
    )
    file(RENAME "${ARDUBOT_ROOT_DIR}/Kconfiglib-master" "${KCONFIGLIB_DIR}")
    file(REMOVE "${ARDUBOT_ROOT_DIR}/kconfiglib-master.zip")
endif()

# Menuconfig target
add_custom_target(menuconfig
    COMMAND ${Python3_EXECUTABLE} ${KCONFIGLIB_DIR}/menuconfig.py Kconfig
    WORKING_DIRECTORY ${ARDUBOT_ROOT_DIR}
    COMMENT "Running menuconfig..."
)

# Guiconfig target (requires tkinter)
add_custom_target(guiconfig
    COMMAND ${Python3_EXECUTABLE} ${KCONFIGLIB_DIR}/guiconfig.py Kconfig
    WORKING_DIRECTORY ${ARDUBOT_ROOT_DIR}
    COMMENT "Running guiconfig..."
)

# Function to generate config headers from .config
function(ardubot_generate_config_header OUTPUT_HEADER CONFIG_FILE)
    add_custom_command(
        OUTPUT ${OUTPUT_HEADER}
        COMMAND ${Python3_EXECUTABLE} ${KCONFIGLIB_DIR}/genconfig.py
            --header-path ${OUTPUT_HEADER}
            --config-out ${CMAKE_CURRENT_BINARY_DIR}/auto.conf
            Kconfig
        DEPENDS ${CONFIG_FILE} ${KCONFIGLIB_DIR}/genconfig.py Kconfig
        COMMENT "Generating config header: ${OUTPUT_HEADER}"
        VERBATIM
    )
endfunction()

# Function to create autoconf.h target
function(ardubot_add_kconfig_target TARGET_NAME)
    set(AUTOCONF_H ${CMAKE_CURRENT_BINARY_DIR}/autoconf.h)
    set(AUTO_CONF ${CMAKE_CURRENT_BINARY_DIR}/auto.conf)
    
    add_custom_target(${TARGET_NAME}_kconfig ALL
        DEPENDS ${AUTOCONF_H} ${AUTO_CONF}
    )
    
    add_custom_command(
        OUTPUT ${AUTOCONF_H} ${AUTO_CONF}
        COMMAND ${Python3_EXECUTABLE} ${KCONFIGLIB_DIR}/genconfig.py
            --header-path ${AUTOCONF_H}
            --config-out ${AUTO_CONF}
            Kconfig
        DEPENDS ${ARDUBOT_ROOT_DIR}/Kconfig ${KCONFIGLIB_DIR}/genconfig.py
        COMMENT "Generating Kconfig headers for ${TARGET_NAME}"
        VERBATIM
    )
    
    target_include_directories(${TARGET_NAME} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
    add_dependencies(${TARGET_NAME} ${TARGET_NAME}_kconfig)
endfunction()

# Default config for simulator
if(ARDUBOT_BUILD_SIM)
    set(ARDUBOT_DEFCONFIG ${ARDUBOT_ROOT_DIR}/boards/sim/defconfig)
elseif(ARDUBOT_BUILD_ESP32)
    set(ARDUBOT_DEFCONFIG ${ARDUBOT_ROOT_DIR}/boards/esp32-devkitc/defconfig)
elseif(ARDUBOT_BUILD_ESP8266)
    set(ARDUBOT_DEFCONFIG ${ARDUBOT_ROOT_DIR}/boards/nodemcu/defconfig)
elseif(ARDUBOT_BUILD_AVR)
    set(ARDUBOT_DEFCONFIG ${ARDUBOT_ROOT_DIR}/boards/mega2560/defconfig)
endif()

if(ARDUBOT_DEFCONFIG AND EXISTS ${ARDUBOT_DEFCONFIG})
    add_custom_target(defconfig
        COMMAND ${CMAKE_COMMAND} -E copy ${ARDUBOT_DEFCONFIG} ${ARDUBOT_ROOT_DIR}/.config
        COMMAND ${Python3_EXECUTABLE} ${KCONFIGLIB_DIR}/genconfig.py
            --header-path ${ARDUBOT_ROOT_DIR}/autoconf.h
            --config-out ${ARDUBOT_ROOT_DIR}/auto.conf
            Kconfig
        COMMENT "Applying default configuration"
    )
endif()

# Savedefconfig target
add_custom_target(savedefconfig
    COMMAND ${Python3_EXECUTABLE} ${KCONFIGLIB_DIR}/genconfig.py
        --header-path ${ARDUBOT_ROOT_DIR}/autoconf.h
        --config-out ${ARDUBOT_ROOT_DIR}/auto.conf
        --defconfig-out ${ARDUBOT_ROOT_DIR}/defconfig
        Kconfig
    COMMENT "Saving minimal configuration"
    WORKING_DIRECTORY ${ARDUBOT_ROOT_DIR}
)