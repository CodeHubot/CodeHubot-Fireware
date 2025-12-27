# UI组件 CMakeLists.txt

idf_component_register(
    SRCS 
        "lvgl_ui_demo.c"
    INCLUDE_DIRS 
        "."
    REQUIRES 
        lvgl
        esp_timer
        display
)