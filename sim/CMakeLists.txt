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
    ${CMAKE_SOURCE_DIR}/apps
    ${CMAKE_BINARY_DIR}/generated
)

# Add SDL2 and PortAudio include directories if available
if(SDL2_INCLUDE_DIRS)
    target_include_directories(sim SYSTEM PUBLIC ${SDL2_INCLUDE_DIRS})
endif()

if(PORTAUDIO_INCLUDE_DIRS)
    target_include_directories(sim SYSTEM PUBLIC ${PORTAUDIO_INCLUDE_DIRS})
endif()

target_link_libraries(sim PUBLIC kernel hal apps stdapps)

# sim_video.c/sim_audio.c call SDL2/PortAudio directly, so `sim` (the
# library) needs to link them itself as a PUBLIC dependency - not just
# get their include dirs (above) while leaving every consumer (the
# ardubot-sim executable, every test that links `sim`) to separately
# bolt on the actual libraries by hand. That per-consumer pattern is
# fragile: once apps/CMakeLists.txt declared apps<->sim's circular
# dependency (apps calls sim_gpio_*/ssd1306_model_*; sim needs apps for
# the built-in app manifests), a consumer that only pulls `sim` in
# transitively (like test_app_ui, via apps) could end up with SDL2
# positioned wrong relative to sim_video.c.o on GNU ld's single-pass
# link line, even though its own CMakeLists.txt separately lists
# SDL2_LIBRARIES - confirmed via an actual CI link failure. Declaring
# the dependency on `sim` itself lets CMake's normal transitive
# ordering handle every consumer uniformly.
if(SDL2_LIBRARIES)
    target_link_libraries(sim PUBLIC ${SDL2_LIBRARIES})
endif()
if(SDL2_LIBRARY_DIRS)
    target_link_directories(sim PUBLIC ${SDL2_LIBRARY_DIRS})
endif()
if(SDL2_LDFLAGS_OTHER)
    target_link_options(sim PUBLIC ${SDL2_LDFLAGS_OTHER})
endif()

if(PORTAUDIO_LIBRARIES)
    target_link_libraries(sim PUBLIC ${PORTAUDIO_LIBRARIES})
endif()
if(PORTAUDIO_LIBRARY_DIRS)
    target_link_directories(sim PUBLIC ${PORTAUDIO_LIBRARY_DIRS})
endif()

if(ARDUBOT_BUILD_SIM)
    add_subdirectory(models)
endif()

# Main simulator executable
add_executable(ardubot-sim sim_main.c)
target_link_libraries(ardubot-sim sim sim_args)