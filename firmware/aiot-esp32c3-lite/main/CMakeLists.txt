# ESP32-C3 Lite Main组件CMake配置

idf_component_register(
    SRCS 
        "main.c"
        "ssd1306_oled.c"
        "dht11_driver.c"
        "device_config.c"
    INCLUDE_DIRS "."
    REQUIRES 
        nvs_flash
        esp_wifi
        esp_event
        esp_netif
        esp_http_server
        esp_http_client
        mqtt
        driver
        esp_timer
        app_update
        esp_system
        json
)

# 定义编译宏
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    AIOT_NO_OTA=1
    AIOT_NO_LVGL=1
    AIOT_HAS_OLED=1
    AIOT_HAS_DHT11=1
    AIOT_ESP32C3=1
    AIOT_CUSTOM_BOARD=1
)

