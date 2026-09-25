target_sources(hal PRIVATE
    hal_gpio_esp8266.c
    hal_i2c_esp8266.c
    hal_spi_esp8266.c
    hal_uart_esp8266.c
    hal_display_esp8266.c
    hal_audio_esp8266.c
    hal_net_esp8266.c
    hal_storage_esp8266.c
    hal_power_esp8266.c
    hal_wifi_esp8266.c
    hal_ble_esp8266.c
    hal_adc_esp8266.c
    hal_pwm_esp8266.c
    hal_rtc_esp8266.c
)

target_include_directories(hal PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ../../include
)

target_compile_definitions(hal PRIVATE
    ARDUINO_ARCH_ESP8266=1
)

if(DEFINED ENV{ESP8266_SDK_PATH})
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