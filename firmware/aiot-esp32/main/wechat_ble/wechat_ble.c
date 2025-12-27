/**
 * @file wechat_ble.c
 * @brief 微信小程序蓝牙功能模块实现
 * @version 1.0
 * @date 2024-01-20
 */

#include "wechat_ble.h"
#include "wechat_ble_gatt.h"
#include "wechat_ble_cmd.h"
#include "wechat_ble_data.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "nvs_flash.h"
#include "cJSON.h"

static const char *TAG = "WECHAT_BLE";

/* 全局变量 */
static wechat_ble_config_t g_wechat_ble_config;
static bool g_wechat_ble_initialized = false;
static bool g_wechat_ble_connected = false;
static uint8_t g_connection_count = 0;
static EventGroupHandle_t g_wechat_ble_event_group;

/* 事件位定义 */
#define WECHAT_BLE_INIT_BIT         BIT0
#define WECHAT_BLE_CONNECTED_BIT    BIT1
#define WECHAT_BLE_ADVERTISING_BIT  BIT2

/* 内部函数声明 */
static void wechat_ble_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static esp_err_t wechat_ble_setup_advertising(void);

/**
 * @brief 初始化微信小程序蓝牙功能
 */
esp_err_t wechat_ble_init(const wechat_ble_config_t *config)
{
    if (g_wechat_ble_initialized) {
        ESP_LOGW(TAG, "WeChat BLE already initialized");
        return ESP_OK;
    }

    if (!config) {
        ESP_LOGE(TAG, "Invalid config parameter");
        return ESP_ERR_INVALID_ARG;
    }

    /* 复制配置 */
    memcpy(&g_wechat_ble_config, config, sizeof(wechat_ble_config_t));

    /* 创建事件组 */
    g_wechat_ble_event_group = xEventGroupCreate();
    if (!g_wechat_ble_event_group) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_ERR_NO_MEM;
    }

    /* 初始化蓝牙控制器 */
    esp_err_t ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Bluetooth controller release classic bt memory failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Initialize controller failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Enable controller failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 初始化蓝牙栈 */
    ret = esp_bluedroid_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Init bluetooth failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bluedroid_enable();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Enable bluetooth failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 注册回调函数 */
    ret = esp_ble_gap_register_callback(wechat_ble_gap_event_handler);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GAP register failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 初始化GATT服务 (包含回调注册) */
    ret = wechat_ble_gatt_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GATT init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 初始化命令处理模块 */
    ret = wechat_ble_cmd_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Command init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 初始化数据管理模块 */
    ret = wechat_ble_data_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Data init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    g_wechat_ble_initialized = true;
    xEventGroupSetBits(g_wechat_ble_event_group, WECHAT_BLE_INIT_BIT);

    ESP_LOGI(TAG, "WeChat BLE initialized successfully");
    return ESP_OK;
}

/**
 * @brief 反初始化微信小程序蓝牙功能
 */
esp_err_t wechat_ble_deinit(void)
{
    if (!g_wechat_ble_initialized) {
        return ESP_OK;
    }

    /* 停止广播 */
    wechat_ble_stop_advertising();

    /* 断开所有连接 */
    wechat_ble_disconnect_all();

    /* 反初始化各模块 */
    wechat_ble_data_deinit();
    wechat_ble_cmd_deinit();
    wechat_ble_gatt_deinit();

    /* 禁用蓝牙 */
    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();
    esp_bt_controller_deinit();

    /* 删除事件组 */
    if (g_wechat_ble_event_group) {
        vEventGroupDelete(g_wechat_ble_event_group);
        g_wechat_ble_event_group = NULL;
    }

    g_wechat_ble_initialized = false;
    g_wechat_ble_connected = false;
    g_connection_count = 0;

    ESP_LOGI(TAG, "WeChat BLE deinitialized");
    return ESP_OK;
}

/**
 * @brief 开始蓝牙广播
 */
esp_err_t wechat_ble_start_advertising(void)
{
    if (!g_wechat_ble_initialized) {
        ESP_LOGE(TAG, "WeChat BLE not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = wechat_ble_setup_advertising();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Setup advertising failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_ble_gap_start_advertising(&wechat_ble_adv_params);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Start advertising failed: %s", esp_err_to_name(ret));
        return ret;
    }

    xEventGroupSetBits(g_wechat_ble_event_group, WECHAT_BLE_ADVERTISING_BIT);
    ESP_LOGI(TAG, "WeChat BLE advertising started successfully");
    return ESP_OK;
}

/**
 * @brief 停止蓝牙广播
 */
esp_err_t wechat_ble_stop_advertising(void)
{
    if (!g_wechat_ble_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = esp_ble_gap_stop_advertising();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Stop advertising failed: %s", esp_err_to_name(ret));
        return ret;
    }

    xEventGroupClearBits(g_wechat_ble_event_group, WECHAT_BLE_ADVERTISING_BIT);
    ESP_LOGI(TAG, "Stopped advertising");
    return ESP_OK;
}

/**
 * @brief 发送设备信息
 */
esp_err_t wechat_ble_send_device_info(const wechat_ble_device_info_t *device_info)
{
    if (!g_wechat_ble_initialized || !g_wechat_ble_connected) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!device_info) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 将设备信息转换为JSON格式 */
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "device_id", device_info->device_id);
    cJSON_AddStringToObject(json, "firmware_version", device_info->firmware_version);
    cJSON_AddStringToObject(json, "hardware_version", device_info->hardware_version);
    cJSON_AddStringToObject(json, "mac_address", device_info->mac_address);
    cJSON_AddNumberToObject(json, "uptime", device_info->uptime);
    cJSON_AddNumberToObject(json, "free_heap", device_info->free_heap);
    cJSON_AddNumberToObject(json, "rssi", device_info->rssi);
    cJSON_AddBoolToObject(json, "wifi_connected", device_info->wifi_connected);
    cJSON_AddBoolToObject(json, "mqtt_connected", device_info->mqtt_connected);

    char *json_string = cJSON_Print(json);
    esp_err_t ret = wechat_ble_send_response(WECHAT_BLE_CMD_GET_DEVICE_INFO, 
                                           (uint8_t*)json_string, strlen(json_string));

    free(json_string);
    cJSON_Delete(json);

    return ret;
}

/**
 * @brief 发送状态信息
 */
esp_err_t wechat_ble_send_status(const wechat_ble_status_t *status)
{
    if (!g_wechat_ble_initialized || !g_wechat_ble_connected) {
        return ESP_ERR_INVALID_STATE;
    }

    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }

    /* 将状态信息转换为JSON格式 */
    cJSON *json = cJSON_CreateObject();
    cJSON_AddBoolToObject(json, "ble_connected", status->ble_connected);
    cJSON_AddBoolToObject(json, "wifi_connected", status->wifi_connected);
    cJSON_AddBoolToObject(json, "mqtt_connected", status->mqtt_connected);
    cJSON_AddNumberToObject(json, "wifi_rssi", status->wifi_rssi);
    cJSON_AddNumberToObject(json, "mqtt_msg_count", status->mqtt_msg_count);
    cJSON_AddNumberToObject(json, "last_error_code", status->last_error_code);
    cJSON_AddStringToObject(json, "last_error_msg", status->last_error_msg);

    char *json_string = cJSON_Print(json);
    esp_err_t ret = wechat_ble_send_response(WECHAT_BLE_CMD_GET_STATUS, 
                                           (uint8_t*)json_string, strlen(json_string));

    free(json_string);
    cJSON_Delete(json);

    return ret;
}

/**
 * @brief 发送响应数据
 */
esp_err_t wechat_ble_send_response(wechat_ble_cmd_t cmd, const uint8_t *data, uint16_t len)
{
    if (!g_wechat_ble_initialized || !g_wechat_ble_connected) {
        return ESP_ERR_INVALID_STATE;
    }

    return wechat_ble_gatt_send_response(cmd, data, len);
}

/**
 * @brief 获取连接状态
 */
bool wechat_ble_is_connected(void)
{
    return g_wechat_ble_connected;
}

/**
 * @brief 获取连接的设备数量
 */
uint8_t wechat_ble_get_connection_count(void)
{
    return g_connection_count;
}

/**
 * @brief 断开所有连接
 */
esp_err_t wechat_ble_disconnect_all(void)
{
    if (!g_wechat_ble_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    return wechat_ble_gatt_disconnect_all();
}

/**
 * @brief GAP事件处理函数
 */
static void wechat_ble_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
        case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
            ESP_LOGI(TAG, "Advertising data set complete");
            break;

        case ESP_GAP_BLE_SCAN_RSP_DATA_SET_COMPLETE_EVT:
            ESP_LOGI(TAG, "Scan response data set complete");
            break;

        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "Advertising started successfully");
                xEventGroupSetBits(g_wechat_ble_event_group, WECHAT_BLE_ADVERTISING_BIT);
            } else {
                ESP_LOGE(TAG, "Advertising start failed");
            }
            break;

        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            ESP_LOGI(TAG, "Advertising stopped");
            xEventGroupClearBits(g_wechat_ble_event_group, WECHAT_BLE_ADVERTISING_BIT);
            break;

        default:
            break;
    }
}

/**
 * @brief 设置连接状态 (供GATT模块调用)
 */
void wechat_ble_set_connection_state(bool connected, uint16_t conn_id)
{
    if (connected) {
        g_wechat_ble_connected = true;
        g_connection_count++;
        xEventGroupSetBits(g_wechat_ble_event_group, WECHAT_BLE_CONNECTED_BIT);
        ESP_LOGI(TAG, "Connection state updated: connected, conn_id: %d", conn_id);
    } else {
        g_wechat_ble_connected = false;
        if (g_connection_count > 0) {
            g_connection_count--;
        }
        if (g_connection_count == 0) {
            xEventGroupClearBits(g_wechat_ble_event_group, WECHAT_BLE_CONNECTED_BIT);
        }
        ESP_LOGI(TAG, "Connection state updated: disconnected");
    }
}

/**
 * @brief 触发事件回调 (供GATT模块调用)
 */
void wechat_ble_trigger_event_callback(wechat_ble_event_type_t event_type)
{
    if (g_wechat_ble_config.event_callback) {
        wechat_ble_event_t event_data = {
            .event_type = event_type
        };
        g_wechat_ble_config.event_callback(&event_data);
    }
}

/**
 * @brief GATTS事件处理函数
 */


/**
 * @brief 设置蓝牙广播
 */
static esp_err_t wechat_ble_setup_advertising(void)
{
    esp_err_t ret;

    /* 设置设备名称 */
    ret = esp_ble_gap_set_device_name(g_wechat_ble_config.device_name);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Set device name failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 设置广播数据 */
    ret = esp_ble_gap_config_adv_data(&wechat_ble_adv_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Config advertising data failed: %s", esp_err_to_name(ret));
        return ret;
    }

    /* 设置扫描响应数据 */
    ret = esp_ble_gap_config_adv_data(&wechat_ble_scan_rsp_data);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Config scan response data failed: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}