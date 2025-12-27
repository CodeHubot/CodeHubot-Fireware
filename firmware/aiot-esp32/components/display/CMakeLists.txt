# 显示组件 CMakeLists.txt

idf_component_register(
    SRCS 
        "lvgl_display.c"
        "simple_display.c"
    INCLUDE_DIRS 
        "."
    REQUIRES 
        driver
        lvgl
        esp_timer
        esp_lcd
        lcd
        espressif__esp_lvgl_port
)