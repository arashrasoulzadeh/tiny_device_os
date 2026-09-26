target_sources(hal PRIVATE
    hal_gpio_avr.c
    hal_i2c_avr.c
    hal_spi_avr.c
    hal_uart_avr.c
    hal_display_avr.c
    hal_audio_avr.c
    hal_net_avr.c
    hal_storage_avr.c
    hal_power_avr.c
)

target_include_directories(hal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ../../include
)

target_compile_definitions(hal PRIVATE
    ARDUINO_ARCH_AVR=1
    F_CPU=16000000UL
)