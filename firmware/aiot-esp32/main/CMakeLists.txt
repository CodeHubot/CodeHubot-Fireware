# AIOT ESP32-S3 Main Component
# 集成BSP架构的ESP-IDF主组件

# 源文件列表 - 移除已重构到drivers和components的文件
# 根据Kconfig配置选择板子BSP文件
if(CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_RAIN)
    set(BOARD_BSP_FILE "../boards/esp32-s3-devkit-rain/bsp_esp32_s3_devkit_rain.c")
    set(BOARD_INCLUDE_DIR "../boards/esp32-s3-devkit-rain")
elseif(CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_LITE)
    set(BOARD_BSP_FILE "../boards/esp32-s3-devkit-lite/bsp_esp32_s3_devkit_lite.c")
    set(BOARD_INCLUDE_DIR "../boards/esp32-s3-devkit-lite")
else()
    set(BOARD_BSP_FILE "../boards/esp32-s3-devkit/bsp_esp32_s3_devkit.c")
    set(BOARD_INCLUDE_DIR "../boards/esp32-s3-devkit")
endif()

set(COMPONENT_SRCS 
    "main.c"
    "bsp/bsp_interface.c"
    ${BOARD_BSP_FILE}
    "ota/ota_manager.c"
    "provisioning/provisioning_client.c"
    "startup/startup_manager.c"
    "mqtt/aiot_mqtt_client.c"
    "wifi_config/wifi_config.c"
    "server/server_config.c"
    "button/button_handler.c"
    "device/device_registration.c"
    "device/device_control.c"
    "device/preset_control.c"
    "device/pwm_control.c"
    "system/module_init.c"
    # Captive Portal - 强制门户功能（学习xiaozhi-esp32架构）
    "captive_portal/captive_portal.c"
    # 以下文件已移动到drivers和components目录
    # "lcd_st7789.c"      -> drivers/display/
    # "lvgl_display.c"    -> components/display/
    # "lvgl_ui_demo.c"    -> components/ui/
    # "simple_display.c"  -> components/display/
    # "dht11.c"           -> drivers/sensors/
    # "ds18b20.c"         -> drivers/sensors/
)

# 包含目录
set(COMPONENT_ADD_INCLUDEDIRS 
    "."
    "hal"
    "bsp"
    "mqtt"
    "ota"
    "provisioning"
    "startup"
    "wifi_config"
    "server"
    "button"
    "device"
    "system"
    "captive_portal"
    ${BOARD_INCLUDE_DIR}
)

# 注册组件
idf_component_register(
    SRCS ${COMPONENT_SRCS}
    INCLUDE_DIRS ${COMPONENT_ADD_INCLUDEDIRS}
    REQUIRES 
        driver 
        esp_driver_gpio 
        esp_driver_ledc 
        esp_driver_spi
        esp_lcd
        nvs_flash 
        esp_timer
        esp_wifi
        esp_netif
        esp_event
        esp_http_client
        esp_http_server
        esp_https_ota
        mqtt
        bt
        json
        spiffs
        fatfs
        app_update
        bootloader_support
        esp_partition
        esp_system
        esp_common
        log
        freertos
        # 新增的组件依赖 - 按照分层架构
        sensors          # drivers/sensors
        lcd              # drivers/lcd  
        display          # components/display
        ui               # components/ui
        # captive_portal 已移到 main/captive_portal/，不再作为外部组件
    PRIV_REQUIRES
        bt
    WHOLE_ARCHIVE
)

# 添加编译定义以禁用微信蓝牙功能
target_compile_definitions(${COMPONENT_LIB} PRIVATE DISABLE_WECHAT_BLE)