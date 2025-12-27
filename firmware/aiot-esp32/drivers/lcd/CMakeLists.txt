# LCD驱动 CMakeLists.txt

idf_component_register(
    SRCS 
        "lcd_st7789.c"
    INCLUDE_DIRS 
        "."
    REQUIRES 
        driver
        spi_flash
        esp_timer
        esp_lcd
)