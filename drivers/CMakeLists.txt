add_library(drivers STATIC
    driver.c
    module.c
    device_registry.c
    gpio_driver.c
    i2c_driver.c
    spi_driver.c
    uart_driver.c
    wifi_driver.c
    display_driver.c
)

target_include_directories(drivers PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/hal/include
    ${CMAKE_SOURCE_DIR}/fs
)

target_link_libraries(drivers PUBLIC kernel hal fs)

add_subdirectory(display)
add_subdirectory(sensor)