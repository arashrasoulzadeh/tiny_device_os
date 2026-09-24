add_library(sim_args STATIC
    sim_args.c
)

target_include_directories(sim_args PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/kernel
    ${CMAKE_SOURCE_DIR}/hal/include
)

if(SDL2_INCLUDE_DIRS)
    target_include_directories(sim_args SYSTEM PUBLIC ${SDL2_INCLUDE_DIRS})
endif()

if(PORTAUDIO_INCLUDE_DIRS)
    target_include_directories(sim_args SYSTEM PUBLIC ${PORTAUDIO_INCLUDE_DIRS})
endif()

target_link_libraries(sim_args PUBLIC kernel hal)

add_library(sim STATIC
    sim_main.c
    sim_video.c
    sim_audio.c
    sim_storage.c
    sim_time.c
    sim_gpio.c
    sim_i2c.c
    sim_spi.c
    sim_net.c
    sim_wifi.c
    sim_ble.c
    sim_adc.c
    sim_pwm.c
    sim_rtc.c
)

target_include_directories(sim PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/hal/include
    ${CMAKE_SOURCE_DIR}/kernel
)

# Add SDL2 and PortAudio include directories if available
if(SDL2_INCLUDE_DIRS)
    target_include_directories(sim SYSTEM PUBLIC ${SDL2_INCLUDE_DIRS})
endif()

if(PORTAUDIO_INCLUDE_DIRS)
    target_include_directories(sim SYSTEM PUBLIC ${PORTAUDIO_INCLUDE_DIRS})
endif()

target_link_libraries(sim PUBLIC kernel hal)

if(ARDUBOT_BUILD_SIM)
    add_subdirectory(models)
endif()

# Main simulator executable
add_executable(ardubot-sim sim_main.c)
target_link_libraries(ardubot-sim sim sim_args)
if(SDL2_LIBRARIES)
    target_link_libraries(ardubot-sim ${SDL2_LIBRARIES})
endif()
if(PORTAUDIO_LIBRARIES)
    target_link_libraries(ardubot-sim ${PORTAUDIO_LIBRARIES})
endif()