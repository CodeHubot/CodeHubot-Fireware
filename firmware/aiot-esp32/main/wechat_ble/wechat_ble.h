/**
 * @file wechat_ble.h
 * @brief 微信小程序蓝牙功能模块
 * @version 1.0
 * @date 2024-01-20
 * 
 * 提供与微信小程序的蓝牙通信功能，支持设备信息查看、WiFi配置、MQTT配置等
 */

#ifndef WECHAT_BLE_H
#define WECHAT_BLE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 微信小程序蓝牙服务配置 */
#define WECHAT_BLE_SERVICE_UUID         0x1234
#define WECHAT_BLE_CHAR_DEVICE_INFO     0x2345
#define WECHAT_BLE_CHAR_WIFI_CONFIG     0x2346
#define WECHAT_BLE_CHAR_MQTT_CONFIG     0x2347
#define WECHAT_BLE_CHAR_CONTROL         0x2348
#define WECHAT_BLE_CHAR_STATUS          0x2349

#define WECHAT_BLE_MAX_DATA_LEN         512
#define WECHAT_BLE_DEVICE_NAME          "AIOT-ESP32-S3"

/* 命令类型定义 */
typedef enum {
    WECHAT_BLE_CMD_GET_DEVICE_INFO = 0x01,
    WECHAT_BLE_CMD_SET_WIFI_CONFIG = 0x02,
    WECHAT_BLE_CMD_SET_MQTT_CONFIG = 0x03,
    WECHAT_BLE_CMD_GET_STATUS = 0x04,
    WECHAT_BLE_CMD_RESTART_DEVICE = 0x05,
    WECHAT_BLE_CMD_FACTORY_RESET = 0x06,
    WECHAT_BLE_CMD_OTA_UPDATE = 0x07,
    WECHAT_BLE_CMD_MAX
} wechat_ble_cmd_t;

/* 设备信息结构体 */
typedef struct {
    char device_id[32];
    char firmware_version[16];
    char hardware_version[16];
    char mac_address[18];
    uint32_t uptime;
    uint32_t free_heap;
    int8_t rssi;
    bool wifi_connected;
    bool mqtt_connected;
} wechat_ble_device_info_t;

/* WiFi配置结构体 */
typedef struct {
    char ssid[32];
    char password[64];
    uint8_t security_type;
    bool auto_connect;
} wechat_ble_wifi_config_t;

/* MQTT配置结构体 */
typedef struct {
    char broker_host[64];
    uint16_t broker_port;
    char username[32];
    char password[64];
    char client_id[32];
    bool use_ssl;
    uint16_t keepalive;
    uint8_t qos_level;
} wechat_ble_mqtt_config_t;

/* 状态信息结构体 */
typedef struct {
    bool ble_connected;
    bool wifi_connected;
    bool mqtt_connected;
    int8_t wifi_rssi;
    uint32_t mqtt_msg_count;
    uint32_t last_error_code;
    char last_error_msg[64];
} wechat_ble_status_t;

/* 事件类型定义 */
typedef enum {
    WECHAT_BLE_EVENT_CONNECTED,
    WECHAT_BLE_EVENT_DISCONNECTED,
    WECHAT_BLE_EVENT_WIFI_CONFIG_RECEIVED,
    WECHAT_BLE_EVENT_MQTT_CONFIG_RECEIVED,
    WECHAT_BLE_EVENT_CONTROL_COMMAND,
    WECHAT_BLE_EVENT_DATA_SENT,
    WECHAT_BLE_EVENT_ERROR
} wechat_ble_event_type_t;

/* 事件数据结构体 */
typedef struct {
    wechat_ble_event_type_t event_type;
    union {
        wechat_ble_wifi_config_t wifi_config;
        wechat_ble_mqtt_config_t mqtt_config;
        wechat_ble_cmd_t control_cmd;
        esp_err_t error_code;
    } data;
} wechat_ble_event_t;

/* 事件回调函数类型 */
typedef void (*wechat_ble_event_cb_t)(wechat_ble_event_t *event);

/* 配置结构体 */
typedef struct {
    char device_name[32];
    uint16_t adv_interval;
    bool security_enabled;
    uint8_t max_connections;
    wechat_ble_event_cb_t event_callback;
} wechat_ble_config_t;

/**
 * @brief 初始化微信小程序蓝牙功能
 * 
 * @param config 配置参数
 * @return esp_err_t 
 */
esp_err_t wechat_ble_init(const wechat_ble_config_t *config);

/**
 * @brief 反初始化微信小程序蓝牙功能
 * 
 * @return esp_err_t 
 */
esp_err_t wechat_ble_deinit(void);

/**
 * @brief 开始蓝牙广播
 * 
 * @return esp_err_t 
 */
esp_err_t wechat_ble_start_advertising(void);

/**
 * @brief 停止蓝牙广播
 * 
 * @return esp_err_t 
 */
esp_err_t wechat_ble_stop_advertising(void);

/**
 * @brief 发送设备信息
 * 
 * @param device_info 设备信息
 * @return esp_err_t 
 */
esp_err_t wechat_ble_send_device_info(const wechat_ble_device_info_t *device_info);

/**
 * @brief 发送状态信息
 * 
 * @param status 状态信息
 * @return esp_err_t 
 */
esp_err_t wechat_ble_send_status(const wechat_ble_status_t *status);

/**
 * @brief 发送响应数据
 * 
 * @param cmd 命令类型
 * @param data 响应数据
 * @param len 数据长度
 * @return esp_err_t 
 */
esp_err_t wechat_ble_send_response(wechat_ble_cmd_t cmd, const uint8_t *data, uint16_t len);

/**
 * @brief 获取连接状态
 * 
 * @return true 已连接
 * @return false 未连接
 */
bool wechat_ble_is_connected(void);

/**
 * @brief 获取连接的设备数量
 * 
 * @return uint8_t 连接数量
 */
uint8_t wechat_ble_get_connection_count(void);

/**
 * @brief 断开所有连接
 * 
 * @return esp_err_t 
 */
esp_err_t wechat_ble_disconnect_all(void);

#ifdef __cplusplus
}
#endif

#endif /* WECHAT_BLE_H */