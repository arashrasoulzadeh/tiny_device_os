target_sources(sim PRIVATE
    ssd1306_model.c
    bmp280_model.c
)

target_include_directories(sim PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)