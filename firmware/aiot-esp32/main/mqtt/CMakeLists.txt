# MQTT Component CMakeLists.txt

set(COMPONENT_SRCS 
    "mqtt_client.c"
    "mqtt_data.c"
    "mqtt_command.c"
)

set(COMPONENT_ADD_INCLUDEDIRS ".")

set(COMPONENT_REQUIRES 
    esp_common
    esp_system
    esp_timer
    nvs_flash
    esp_netif
    esp_wifi
    esp_event
    mqtt
    freertos
    log
    json
    esp_http_client
    esp_https_ota
    lwip
)

register_component()