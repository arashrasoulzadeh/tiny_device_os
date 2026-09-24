target_sources(hal PRIVATE
    hal_gpio_sim.c
    hal_i2c_sim.c
    hal_spi_sim.c
    hal_uart_sim.c
    hal_display_sim.c
    hal_audio_sim.c
    hal_net_sim.c
    hal_storage_sim.c
)

target_include_directories(hal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
)