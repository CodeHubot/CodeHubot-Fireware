/**
 * @file wechat_ble_data.c
 * @brief 微信小程序蓝牙数据管理模块实现
 * @version 1.0
 * @date 2024-01-20
 */

#include "wechat_ble_data.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include <string.h>

static const char* TAG = "WECHAT_BLE_DATA";

// NVS命名空间
#define NVS_NAMESPACE "wechat_ble"
#define NVS_WIFI_CONFIG_KEY "wifi_config"
#define NVS_MQTT_CONFIG_KEY "mqtt_config"

// 私有变量
static bool g_data_initialized = false;
static nvs_handle_t g_nvs_handle = 0;

// 私有函数声明
static esp_err_t wechat_ble_data_send_internal(const uint8_t *data, uint16_t len);

esp_err_t wechat_ble_data_init(void)
{
    if (g_data_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing WeChat BLE data module");

    // 打开NVS
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &g_nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(ret));
        return ret;
    }

    g_data_initialized = true;
    return ESP_OK;
}

esp_err_t wechat_ble_data_deinit(void)
{
    if (!g_data_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deinitializing WeChat BLE data module");

    if (g_nvs_handle != 0) {
        nvs_close(g_nvs_handle);
        g_nvs_handle = 0;
    }

    g_data_initialized = false;
    return ESP_OK;
}

esp_err_t wechat_ble_data_get_device_info(wechat_ble_device_info_t *device_info)
{
    if (!g_data_initialized || !device_info) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(device_info, 0, sizeof(wechat_ble_device_info_t));

    // 设置设备ID
    strncpy(device_info->device_id, "AIOT-ESP32-S3", sizeof(device_info->device_id) - 1);

    // 设置固件版本
    strncpy(device_info->firmware_version, "1.0.0", sizeof(device_info->firmware_version) - 1);

    // 设置硬件版本
    strncpy(device_info->hardware_version, "ESP32-S3", sizeof(device_info->hardware_version) - 1);

    // 获取MAC地址
    uint8_t mac[6];
    esp_read_mac(mac, ESP_IF_WIFI_STA);
    snprintf(device_info->mac_address, sizeof(device_info->mac_address), 
             "%02X:%02X:%02X:%02X:%02X:%02X", 
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // 获取运行时间
    device_info->uptime = esp_timer_get_time() / 1000000;

    // 获取空闲堆内存
    device_info->free_heap = esp_get_free_heap_size();

    // 设置RSSI（这里需要实际的WiFi连接状态）
    device_info->rssi = -50;

    // 设置连接状态
    device_info->wifi_connected = false;
    device_info->mqtt_connected = false;

    return ESP_OK;
}

esp_err_t wechat_ble_data_get_status(wechat_ble_status_t *status)
{
    if (!g_data_initialized || !status) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(status, 0, sizeof(wechat_ble_status_t));

    status->ble_connected = true;
    status->wifi_connected = false;
    status->mqtt_connected = false;
    status->wifi_rssi = -50;
    status->mqtt_msg_count = 0;
    status->last_error_code = 0;
    strncpy(status->last_error_msg, "No error", sizeof(status->last_error_msg) - 1);

    return ESP_OK;
}

esp_err_t wechat_ble_data_save_wifi_config(const wechat_ble_wifi_config_t *wifi_config)
{
    if (!g_data_initialized || !wifi_config) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = nvs_set_blob(g_nvs_handle, NVS_WIFI_CONFIG_KEY, wifi_config, sizeof(wechat_ble_wifi_config_t));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save WiFi config: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_commit(g_nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit WiFi config: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "WiFi config saved successfully");
    return ESP_OK;
}

esp_err_t wechat_ble_data_load_wifi_config(wechat_ble_wifi_config_t *wifi_config)
{
    if (!g_data_initialized || !wifi_config) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t required_size = sizeof(wechat_ble_wifi_config_t);
    esp_err_t ret = nvs_get_blob(g_nvs_handle, NVS_WIFI_CONFIG_KEY, wifi_config, &required_size);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load WiFi config: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "WiFi config loaded successfully");
    return ESP_OK;
}

esp_err_t wechat_ble_data_save_mqtt_config(const wechat_ble_mqtt_config_t *mqtt_config)
{
    if (!g_data_initialized || !mqtt_config) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = nvs_set_blob(g_nvs_handle, NVS_MQTT_CONFIG_KEY, mqtt_config, sizeof(wechat_ble_mqtt_config_t));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save MQTT config: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_commit(g_nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit MQTT config: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "MQTT config saved successfully");
    return ESP_OK;
}

esp_err_t wechat_ble_data_load_mqtt_config(wechat_ble_mqtt_config_t *mqtt_config)
{
    if (!g_data_initialized || !mqtt_config) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t required_size = sizeof(wechat_ble_mqtt_config_t);
    esp_err_t ret = nvs_get_blob(g_nvs_handle, NVS_MQTT_CONFIG_KEY, mqtt_config, &required_size);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to load MQTT config: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "MQTT config loaded successfully");
    return ESP_OK;
}

esp_err_t wechat_ble_data_send(const uint8_t *data, uint16_t len)
{
    if (!g_data_initialized || !data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    return wechat_ble_data_send_internal(data, len);
}

esp_err_t wechat_ble_data_clear_all_config(void)
{
    if (!g_data_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = nvs_erase_key(g_nvs_handle, NVS_WIFI_CONFIG_KEY);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Failed to clear WiFi config: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_erase_key(g_nvs_handle, NVS_MQTT_CONFIG_KEY);
    if (ret != ESP_OK && ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Failed to clear MQTT config: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_commit(g_nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit config clear: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "All config cleared successfully");
    return ESP_OK;
}

bool wechat_ble_data_has_wifi_config(void)
{
    if (!g_data_initialized) {
        return false;
    }

    size_t required_size = 0;
    esp_err_t ret = nvs_get_blob(g_nvs_handle, NVS_WIFI_CONFIG_KEY, NULL, &required_size);
    return (ret == ESP_OK && required_size == sizeof(wechat_ble_wifi_config_t));
}

bool wechat_ble_data_has_mqtt_config(void)
{
    if (!g_data_initialized) {
        return false;
    }

    size_t required_size = 0;
    esp_err_t ret = nvs_get_blob(g_nvs_handle, NVS_MQTT_CONFIG_KEY, NULL, &required_size);
    return (ret == ESP_OK && required_size == sizeof(wechat_ble_mqtt_config_t));
}

// 私有函数实现
static esp_err_t wechat_ble_data_send_internal(const uint8_t *data, uint16_t len)
{
    // 这里应该通过GATT特征值发送数据
    // 暂时只打印日志
    ESP_LOGI(TAG, "Sending data: %d bytes", len);
    ESP_LOG_BUFFER_HEX(TAG, data, len);
    
    // 实际实现需要调用GATT发送函数
    return ESP_OK;
}