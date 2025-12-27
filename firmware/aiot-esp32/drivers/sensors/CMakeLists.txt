# 传感器驱动 CMakeLists.txt

idf_component_register(
    SRCS 
        "dht11.c"
        "ds18b20.c"
        "rain_sensor.c"
    INCLUDE_DIRS 
        "."
    REQUIRES 
        driver
        esp_timer
)

# 设置组件名称
set(COMPONENT_NAME "sensors")