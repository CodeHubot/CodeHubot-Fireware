/**
 * @file bsp_interface.c
 * @brief 板级支持包(BSP)接口实现
 * 
 * @author AIOT Team
 * @date 2024
 */

#include "bsp_interface.h"
#include <stdio.h>
#include <string.h>

// 全局BSP接口指针
static const bsp_interface_t *g_bsp_interface = NULL;

hal_err_t bsp_register_interface(const bsp_interface_t *interface)
{
    if (interface == NULL) {
        return HAL_ERROR_INVALID_PARAM;
    }
    
    // 验证必要的接口函数
    if (interface->init == NULL || 
        interface->get_board_info == NULL || 
        interface->get_hw_config == NULL) {
        return HAL_ERROR_INVALID_PARAM;
    }
    
    g_bsp_interface = interface;
    return HAL_OK;
}

const bsp_interface_t* bsp_get_interface(void)
{
    return g_bsp_interface;
}

hal_err_t bsp_init(void)
{
    if (g_bsp_interface == NULL || g_bsp_interface->init == NULL) {
        return HAL_ERROR_NOT_INITIALIZED;
    }
    
    return g_bsp_interface->init();
}

hal_err_t bsp_deinit(void)
{
    if (g_bsp_interface == NULL || g_bsp_interface->deinit == NULL) {
        return HAL_ERROR_NOT_INITIALIZED;
    }
    
    return g_bsp_interface->deinit();
}

const bsp_board_info_t* bsp_get_board_info(void)
{
    if (g_bsp_interface == NULL || g_bsp_interface->get_board_info == NULL) {
        return NULL;
    }
    
    return g_bsp_interface->get_board_info();
}

const bsp_hardware_config_t* bsp_get_hardware_config(void)
{
    if (g_bsp_interface == NULL || g_bsp_interface->get_hw_config == NULL) {
        return NULL;
    }
    
    return g_bsp_interface->get_hw_config();
}

void bsp_print_board_info(void)
{
    const bsp_board_info_t *info = bsp_get_board_info();
    if (info == NULL) {
        printf("BSP: Board info not available\n");
        return;
    }
    
    printf("=== Board Information ===\n");
    printf("Board Name: %s\n", info->board_name ? info->board_name : "Unknown");
    printf("Chip Model: %s\n", info->chip_model ? info->chip_model : "Unknown");
    printf("Board Version: %s\n", info->board_version ? info->board_version : "Unknown");
    printf("Manufacturer: %s\n", info->manufacturer ? info->manufacturer : "Unknown");
    printf("Flash Size: %lu MB\n", (unsigned long)info->flash_size_mb);
    printf("PSRAM Size: %lu MB\n", (unsigned long)info->psram_size_mb);
    printf("WiFi: %s\n", info->has_wifi ? "Yes" : "No");
    printf("Ethernet: %s\n", info->has_ethernet ? "Yes" : "No");
    printf("========================\n");
}

hal_err_t bsp_validate_hardware_config(void)
{
    const bsp_hardware_config_t *config = bsp_get_hardware_config();
    if (config == NULL) {
        printf("BSP: Hardware config not available\n");
        return HAL_ERROR_NOT_INITIALIZED;
    }
    
    printf("=== Hardware Configuration ===\n");
    printf("LEDs: %d\n", config->led_count);
    printf("Relays: %d\n", config->relay_count);
    printf("Servos: %d\n", config->servo_count);
    printf("Sensors: %d\n", config->sensor_count);
    printf("Buttons: %d\n", config->button_count);
    printf("Display Type: %d\n", config->display_config.type);
    printf("==============================\n");
    
    return HAL_OK;
}


// MQTT客户端功能实现
hal_err_t bsp_mqtt_init(const mqtt_config_t* config)
{
    if (g_bsp_interface == NULL || g_bsp_interface->mqtt_init == NULL) {
        return HAL_ERROR_NOT_SUPPORTED;
    }
    
    return g_bsp_interface->mqtt_init(config);
}

hal_err_t bsp_mqtt_deinit(void)
{
    if (g_bsp_interface == NULL || g_bsp_interface->mqtt_deinit == NULL) {
        return HAL_ERROR_NOT_SUPPORTED;
    }
    
    return g_bsp_interface->mqtt_deinit();
}

hal_err_t bsp_mqtt_connect(void)
{
    if (g_bsp_interface == NULL || g_bsp_interface->mqtt_connect == NULL) {
        return HAL_ERROR_NOT_SUPPORTED;
    }
    
    return g_bsp_interface->mqtt_connect();
}

hal_err_t bsp_mqtt_disconnect(void)
{
    if (g_bsp_interface == NULL || g_bsp_interface->mqtt_disconnect == NULL) {
        return HAL_ERROR_NOT_SUPPORTED;
    }
    
    return g_bsp_interface->mqtt_disconnect();
}

// OTA管理器功能实现
hal_err_t bsp_ota_init(void)
{
    if (g_bsp_interface == NULL || g_bsp_interface->ota_init == NULL) {
        return HAL_ERROR_NOT_SUPPORTED;
    }
    
    return g_bsp_interface->ota_init();
}

hal_err_t bsp_ota_deinit(void)
{
    if (g_bsp_interface == NULL || g_bsp_interface->ota_deinit == NULL) {
        return HAL_ERROR_NOT_SUPPORTED;
    }
    
    return g_bsp_interface->ota_deinit();
}

hal_err_t bsp_ota_check_update(const char* url)
{
    if (g_bsp_interface == NULL || g_bsp_interface->ota_check_update == NULL) {
        return HAL_ERROR_NOT_SUPPORTED;
    }
    
    return g_bsp_interface->ota_check_update(url);
}

// 已弃用 - 使用新的startup_manager
/*
hal_err_t bsp_ota_start_update(const ota_config_t* config)
{
    if (g_bsp_interface == NULL || g_bsp_interface->ota_start_update == NULL) {
        return HAL_ERROR_NOT_SUPPORTED;
    }
    
    return g_bsp_interface->ota_start_update(config);
}
*/

// WiFi功能实现
hal_err_t bsp_wifi_init(void)
{
    if (g_bsp_interface == NULL || g_bsp_interface->wifi_init == NULL) {
        return HAL_ERROR_NOT_SUPPORTED;
    }
    
    return g_bsp_interface->wifi_init();
}

hal_err_t bsp_wifi_deinit(void)
{
    if (g_bsp_interface == NULL || g_bsp_interface->wifi_deinit == NULL) {
        return HAL_ERROR_NOT_SUPPORTED;
    }
    
    return g_bsp_interface->wifi_deinit();
}

hal_err_t bsp_wifi_connect(const char* ssid, const char* password)
{
    if (g_bsp_interface == NULL || g_bsp_interface->wifi_connect == NULL) {
        return HAL_ERROR_NOT_SUPPORTED;
    }
    
    return g_bsp_interface->wifi_connect(ssid, password);
}

hal_err_t bsp_wifi_disconnect(void)
{
    if (g_bsp_interface == NULL || g_bsp_interface->wifi_disconnect == NULL) {
        return HAL_ERROR_NOT_SUPPORTED;
    }
    
    return g_bsp_interface->wifi_disconnect();
}

// 传感器功能实现
hal_err_t bsp_sensor_init(void)
{
    if (g_bsp_interface == NULL || g_bsp_interface->sensor_init == NULL) {
        return HAL_ERROR_NOT_SUPPORTED;
    }
    
    return g_bsp_interface->sensor_init();
}

hal_err_t bsp_sensor_deinit(void)
{
    if (g_bsp_interface == NULL || g_bsp_interface->sensor_deinit == NULL) {
        return HAL_ERROR_NOT_SUPPORTED;
    }
    
    return g_bsp_interface->sensor_deinit();
}

hal_err_t bsp_sensor_read(uint8_t sensor_id, float* value)
{
    if (g_bsp_interface == NULL || g_bsp_interface->sensor_read == NULL) {
        return HAL_ERROR_NOT_SUPPORTED;
    }
    
    return g_bsp_interface->sensor_read(sensor_id, value);
}

// 系统监控功能实现
hal_err_t bsp_system_monitor_init(void)
{
    if (g_bsp_interface == NULL || g_bsp_interface->system_monitor_init == NULL) {
        return HAL_ERROR_NOT_SUPPORTED;
    }
    
    return g_bsp_interface->system_monitor_init();
}

hal_err_t bsp_system_monitor_get_status(void* status)
{
    if (g_bsp_interface == NULL || g_bsp_interface->system_monitor_get_status == NULL) {
        return HAL_ERROR_NOT_SUPPORTED;
    }
    
    return g_bsp_interface->system_monitor_get_status(status);
}