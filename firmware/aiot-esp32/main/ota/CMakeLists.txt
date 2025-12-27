# OTA Component CMakeLists.txt

set(COMPONENT_SRCS 
    "ota_manager.c"
    "ota_version.c"
    "ota_security.c"
)

set(COMPONENT_ADD_INCLUDEDIRS ".")

set(COMPONENT_REQUIRES 
    esp_common
    esp_system
    esp_timer
    nvs_flash
    esp_ota_ops
    esp_partition
    esp_http_client
    esp_https_ota
    esp_netif
    esp_wifi
    esp_event
    freertos
    log
    json
    mbedtls
    bootloader_support
    spi_flash
)

register_component()