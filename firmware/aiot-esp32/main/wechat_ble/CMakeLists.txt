# WeChat BLE Component CMakeLists.txt

set(COMPONENT_SRCS 
    "wechat_ble.c"
    "wechat_ble_gatt.c"
    "wechat_ble_cmd.c"
    "wechat_ble_data.c"
)

set(COMPONENT_ADD_INCLUDEDIRS ".")

set(COMPONENT_REQUIRES 
    esp_common
    esp_system
    esp_timer
    nvs_flash
    bt
    esp_bt
    esp_netif
    esp_wifi
    esp_event
    freertos
    log
    json
)

register_component()