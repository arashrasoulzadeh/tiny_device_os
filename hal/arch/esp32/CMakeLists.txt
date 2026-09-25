target_sources(hal PRIVATE
    hal_gpio_esp32.c
    hal_i2c_esp32.c
    hal_spi_esp32.c
    hal_uart_esp32.c
    hal_display_esp32.c
    hal_audio_esp32.c
    hal_net_esp32.c
    hal_storage_esp32.c
    hal_power_esp32.c
)

target_include_directories(hal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ../../include
)

# ESP32 specific compile definitions
target_compile_definitions(hal PRIVATE
    ARDUINO_ARCH_ESP32=1
    ESP_PLATFORM=1
)

# Link ESP-IDF components if available
if(DEFINED ENV{IDF_PATH})
    target_link_libraries(hal PRIVATE
        esp_driver_gpio
        esp_driver_i2c
        esp_driver_spi
        esp_driver_uart
        esp_driver_ledc
        esp_driver_adc
        esp_wifi
        esp_event
        esp_timer
        esp_sleep
        esp_pm
        freertos
        driver
        hal
        soc
        esp_system
        esp_common
        esp_rom
        esp_hw_support
        esp_efuse
        esp_phy
    )
endif()