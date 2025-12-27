/**
 * @file wechat_ble_cmd.c
 * @brief 微信小程序蓝牙命令处理模块实现
 * @version 1.0
 * @date 2024-01-20
 */

#include "wechat_ble_cmd.h"
#include "wechat_ble_data.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "cJSON.h"
#include <string.h>

static const char* TAG = "WECHAT_BLE_CMD";

// 私有变量
static bool g_cmd_initialized = false;

esp_err_t wechat_ble_cmd_init(void)
{
    if (g_cmd_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing WeChat BLE command module");
    g_cmd_initialized = true;
    return ESP_OK;
}

esp_err_t wechat_ble_cmd_deinit(void)
{
    if (!g_cmd_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Deinitializing WeChat BLE command module");
    g_cmd_initialized = false;
    return ESP_OK;
}

esp_err_t wechat_ble_cmd_process(const uint8_t *data, uint16_t len)
{
    if (!g_cmd_initialized || !data || len < sizeof(wechat_ble_cmd_packet_t)) {
        return ESP_ERR_INVALID_ARG;
    }

    const wechat_ble_cmd_packet_t *packet = (const wechat_ble_cmd_packet_t *)data;
    
    ESP_LOGI(TAG, "Processing command: 0x%02X, seq: %d, len: %d", packet->cmd, packet->seq, packet->len);

    switch (packet->cmd) {
        case WECHAT_BLE_CMD_GET_DEVICE_INFO:
            return wechat_ble_cmd_handle_get_device_info(packet->seq);
            
        case WECHAT_BLE_CMD_SET_WIFI_CONFIG:
            return wechat_ble_cmd_handle_wifi_config(packet->seq, packet->data, packet->len);
            
        case WECHAT_BLE_CMD_SET_MQTT_CONFIG:
            return wechat_ble_cmd_handle_mqtt_config(packet->seq, packet->data, packet->len);
            
        case WECHAT_BLE_CMD_GET_STATUS:
            return wechat_ble_cmd_handle_get_status(packet->seq);
            
        case WECHAT_BLE_CMD_RESTART_DEVICE:
            return wechat_ble_cmd_handle_restart_device(packet->seq);
            
        case WECHAT_BLE_CMD_FACTORY_RESET:
            return wechat_ble_cmd_handle_factory_reset(packet->seq);
            
        case WECHAT_BLE_CMD_OTA_UPDATE:
            return wechat_ble_cmd_handle_ota_update(packet->seq, packet->data, packet->len);
            
        default:
            ESP_LOGW(TAG, "Unknown command: 0x%02X", packet->cmd);
            return wechat_ble_cmd_send_response(packet->cmd, packet->seq, WECHAT_BLE_STATUS_INVALID_CMD, NULL, 0);
    }
}

esp_err_t wechat_ble_cmd_send_response(uint8_t cmd, uint8_t seq, uint8_t status, const uint8_t *data, uint16_t len)
{
    if (!g_cmd_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    size_t packet_size = sizeof(wechat_ble_rsp_packet_t) + len;
    uint8_t *packet_buf = malloc(packet_size);
    if (!packet_buf) {
        return ESP_ERR_NO_MEM;
    }

    wechat_ble_rsp_packet_t *rsp = (wechat_ble_rsp_packet_t *)packet_buf;
    rsp->cmd = cmd;
    rsp->seq = seq;
    rsp->status = status;
    rsp->len = len;
    
    if (data && len > 0) {
        memcpy(rsp->data, data, len);
    }

    esp_err_t ret = wechat_ble_data_send(packet_buf, packet_size);
    free(packet_buf);
    
    return ret;
}

esp_err_t wechat_ble_cmd_handle_get_device_info(uint8_t seq)
{
    ESP_LOGI(TAG, "Handling get device info command");
    
    cJSON *json = cJSON_CreateObject();
    if (!json) {
        return wechat_ble_cmd_send_response(WECHAT_BLE_CMD_GET_DEVICE_INFO, seq, WECHAT_BLE_STATUS_ERROR, NULL, 0);
    }

    // 获取设备信息
    uint8_t mac[6];
    esp_read_mac(mac, ESP_IF_WIFI_STA);
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X", 
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    cJSON_AddStringToObject(json, "device_id", "AIOT-ESP32-S3");
    cJSON_AddStringToObject(json, "firmware_version", "1.0.0");
    cJSON_AddStringToObject(json, "hardware_version", "ESP32-S3");
    cJSON_AddStringToObject(json, "mac_address", mac_str);
    cJSON_AddNumberToObject(json, "uptime", esp_timer_get_time() / 1000000);
    cJSON_AddNumberToObject(json, "free_heap", esp_get_free_heap_size());

    char *json_str = cJSON_Print(json);
    esp_err_t ret = ESP_ERR_NO_MEM;
    if (json_str) {
        ret = wechat_ble_cmd_send_response(WECHAT_BLE_CMD_GET_DEVICE_INFO, seq, WECHAT_BLE_STATUS_SUCCESS, 
                                         (uint8_t*)json_str, strlen(json_str));
        free(json_str);
    }
    
    cJSON_Delete(json);
    return ret;
}

esp_err_t wechat_ble_cmd_handle_wifi_config(uint8_t seq, const uint8_t *data, uint16_t len)
{
    ESP_LOGI(TAG, "Handling WiFi config command");
    
    if (!data || len == 0) {
        return wechat_ble_cmd_send_response(WECHAT_BLE_CMD_SET_WIFI_CONFIG, seq, WECHAT_BLE_STATUS_INVALID_PARAM, NULL, 0);
    }

    // 解析JSON配置
    cJSON *json = cJSON_ParseWithLength((char*)data, len);
    if (!json) {
        return wechat_ble_cmd_send_response(WECHAT_BLE_CMD_SET_WIFI_CONFIG, seq, WECHAT_BLE_STATUS_INVALID_PARAM, NULL, 0);
    }

    cJSON *ssid_item = cJSON_GetObjectItem(json, "ssid");
    cJSON *password_item = cJSON_GetObjectItem(json, "password");
    
    if (!cJSON_IsString(ssid_item) || !cJSON_IsString(password_item)) {
        cJSON_Delete(json);
        return wechat_ble_cmd_send_response(WECHAT_BLE_CMD_SET_WIFI_CONFIG, seq, WECHAT_BLE_STATUS_INVALID_PARAM, NULL, 0);
    }

    // 这里应该保存WiFi配置到NVS
    ESP_LOGI(TAG, "WiFi config - SSID: %s", ssid_item->valuestring);
    
    cJSON_Delete(json);
    return wechat_ble_cmd_send_response(WECHAT_BLE_CMD_SET_WIFI_CONFIG, seq, WECHAT_BLE_STATUS_SUCCESS, NULL, 0);
}

esp_err_t wechat_ble_cmd_handle_mqtt_config(uint8_t seq, const uint8_t *data, uint16_t len)
{
    ESP_LOGI(TAG, "Handling MQTT config command");
    
    if (!data || len == 0) {
        return wechat_ble_cmd_send_response(WECHAT_BLE_CMD_SET_MQTT_CONFIG, seq, WECHAT_BLE_STATUS_INVALID_PARAM, NULL, 0);
    }

    // 解析JSON配置
    cJSON *json = cJSON_ParseWithLength((char*)data, len);
    if (!json) {
        return wechat_ble_cmd_send_response(WECHAT_BLE_CMD_SET_MQTT_CONFIG, seq, WECHAT_BLE_STATUS_INVALID_PARAM, NULL, 0);
    }

    // 这里应该保存MQTT配置到NVS
    ESP_LOGI(TAG, "MQTT config received");
    
    cJSON_Delete(json);
    return wechat_ble_cmd_send_response(WECHAT_BLE_CMD_SET_MQTT_CONFIG, seq, WECHAT_BLE_STATUS_SUCCESS, NULL, 0);
}

esp_err_t wechat_ble_cmd_handle_get_status(uint8_t seq)
{
    ESP_LOGI(TAG, "Handling get status command");
    
    cJSON *json = cJSON_CreateObject();
    if (!json) {
        return wechat_ble_cmd_send_response(WECHAT_BLE_CMD_GET_STATUS, seq, WECHAT_BLE_STATUS_ERROR, NULL, 0);
    }

    cJSON_AddBoolToObject(json, "ble_connected", true);
    cJSON_AddBoolToObject(json, "wifi_connected", false);
    cJSON_AddBoolToObject(json, "mqtt_connected", false);
    cJSON_AddNumberToObject(json, "free_heap", esp_get_free_heap_size());

    char *json_str = cJSON_Print(json);
    esp_err_t ret = ESP_ERR_NO_MEM;
    if (json_str) {
        ret = wechat_ble_cmd_send_response(WECHAT_BLE_CMD_GET_STATUS, seq, WECHAT_BLE_STATUS_SUCCESS, 
                                         (uint8_t*)json_str, strlen(json_str));
        free(json_str);
    }
    
    cJSON_Delete(json);
    return ret;
}

esp_err_t wechat_ble_cmd_handle_restart_device(uint8_t seq)
{
    ESP_LOGI(TAG, "Handling restart device command");
    
    esp_err_t ret = wechat_ble_cmd_send_response(WECHAT_BLE_CMD_RESTART_DEVICE, seq, WECHAT_BLE_STATUS_SUCCESS, NULL, 0);
    
    // 延迟重启以确保响应发送完成
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    
    return ret;
}

esp_err_t wechat_ble_cmd_handle_factory_reset(uint8_t seq)
{
    ESP_LOGI(TAG, "Handling factory reset command");
    
    // 清除NVS存储
    nvs_flash_erase();
    
    esp_err_t ret = wechat_ble_cmd_send_response(WECHAT_BLE_CMD_FACTORY_RESET, seq, WECHAT_BLE_STATUS_SUCCESS, NULL, 0);
    
    // 延迟重启
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    
    return ret;
}

esp_err_t wechat_ble_cmd_handle_ota_update(uint8_t seq, const uint8_t *data, uint16_t len)
{
    ESP_LOGI(TAG, "Handling OTA update command");
    
    // OTA更新功能暂未实现
    return wechat_ble_cmd_send_response(WECHAT_BLE_CMD_OTA_UPDATE, seq, WECHAT_BLE_STATUS_NOT_SUPPORTED, NULL, 0);
}