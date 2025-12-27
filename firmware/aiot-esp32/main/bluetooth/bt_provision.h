/**
 * @file bt_provision.h
 * @brief 蓝牙配网功能头文件
 * 
 * 提供通过蓝牙BLE配置WiFi和服务器信息的功能
 * 
 * @author AIOT Team
 * @date 2024-01-01
 */

#ifndef BT_PROVISION_H
#define BT_PROVISION_H

#include <stdint.h>
#include <stdbool.h>

#ifdef ESP_PLATFORM
#include "esp_err.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#else
// 非ESP平台的兼容性定义
typedef void* esp_timer_handle_t;
typedef void* EventGroupHandle_t;
typedef void* esp_event_handler_instance_t;
typedef int esp_err_t;
typedef int esp_gatt_if_t;
typedef int esp_gatts_cb_event_t;
typedef int esp_gap_ble_cb_event_t;
typedef int esp_event_base_t;
typedef void esp_ble_gap_cb_param_t;
typedef void esp_ble_gatts_cb_param_t;
typedef void cJSON;

#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_GATT_IF_NONE -1
#define ESP_EVENT_ANY_ID -1
#define WIFI_EVENT 0
#define IP_EVENT 1
#define IP_EVENT_STA_GOT_IP 2
#endif

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 常量定义 ====================

// 服务和特征UUID
#define BT_PROVISION_SERVICE_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define BT_PROVISION_WRITE_CHAR_UUID     "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define BT_PROVISION_READ_CHAR_UUID      "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"
#define BT_PROVISION_NOTIFY_CHAR_UUID    "6E400004-B5A3-F393-E0A9-E50E24DCCA9E"

// 配置参数限制
#define BT_PROVISION_DEVICE_NAME_MAX     32
#define BT_PROVISION_SSID_MAX           32
#define BT_PROVISION_PASSWORD_MAX       64
#define BT_PROVISION_SERVER_URL_MAX     128
#define BT_PROVISION_DEVICE_ID_MAX      32
#define BT_PROVISION_API_KEY_MAX        64
#define BT_PROVISION_MESSAGE_MAX        256

// 超时设置（毫秒）
#define BT_PROVISION_TIMEOUT_MS         (5 * 60 * 1000)  // 5分钟总超时
#define BT_PROVISION_WIFI_TIMEOUT_MS    (30 * 1000)      // 30秒WiFi连接超时
#define BT_PROVISION_SERVER_TIMEOUT_MS  (10 * 1000)      // 10秒服务器测试超时
#define BT_PROVISION_ADV_TIMEOUT_MS     (2 * 60 * 1000)  // 2分钟广播超时

// 重试次数
#define BT_PROVISION_WIFI_RETRY_COUNT   3
#define BT_PROVISION_SERVER_RETRY_COUNT 2

// NVS命名空间
#define NVS_NAMESPACE_WIFI              "wifi_config"
#define NVS_NAMESPACE_SERVER            "server_config"
#define NVS_NAMESPACE_PROVISION         "provision_config"

// ==================== 枚举定义 ====================

/**
 * @brief 配网状态枚举
 */
typedef enum {
    BT_PROVISION_STATE_IDLE = 0,        ///< 空闲状态
    BT_PROVISION_STATE_ADVERTISING,     ///< 蓝牙广播中
    BT_PROVISION_STATE_CONNECTED,       ///< 蓝牙已连接
    BT_PROVISION_STATE_CONFIGURING,     ///< 配置中
    BT_PROVISION_STATE_WIFI_CONNECTING, ///< WiFi连接中
    BT_PROVISION_STATE_SERVER_TESTING,  ///< 服务器测试中
    BT_PROVISION_STATE_SUCCESS,         ///< 配网成功
    BT_PROVISION_STATE_FAILED,          ///< 配网失败
    BT_PROVISION_STATE_TIMEOUT          ///< 超时
} bt_provision_state_t;

/**
 * @brief 配网错误代码
 */
typedef enum {
    BT_PROVISION_ERR_OK = 0,                ///< 成功
    BT_PROVISION_ERR_INVALID_PARAM,         ///< 无效参数
    BT_PROVISION_ERR_WIFI_CONNECT_FAILED,   ///< WiFi连接失败
    BT_PROVISION_ERR_SERVER_CONNECT_FAILED, ///< 服务器连接失败
    BT_PROVISION_ERR_TIMEOUT,               ///< 超时
    BT_PROVISION_ERR_STORAGE_FAILED,        ///< 存储失败
    BT_PROVISION_ERR_BLE_FAILED,            ///< 蓝牙失败
    BT_PROVISION_ERR_ALREADY_CONFIGURED,    ///< 已经配置
    BT_PROVISION_ERR_NOT_INITIALIZED,       ///< 未初始化
    BT_PROVISION_ERR_JSON_PARSE_FAILED      ///< JSON解析失败
} bt_provision_err_t;

/**
 * @brief WiFi安全类型
 */
typedef enum {
    BT_PROVISION_WIFI_AUTH_OPEN = 0,    ///< 开放网络
    BT_PROVISION_WIFI_AUTH_WEP,         ///< WEP
    BT_PROVISION_WIFI_AUTH_WPA_PSK,     ///< WPA PSK
    BT_PROVISION_WIFI_AUTH_WPA2_PSK,    ///< WPA2 PSK
    BT_PROVISION_WIFI_AUTH_WPA_WPA2_PSK ///< WPA/WPA2 PSK
} bt_provision_wifi_auth_t;

// ==================== 结构体定义 ====================

/**
 * @brief WiFi配置结构体
 */
typedef struct {
    char ssid[BT_PROVISION_SSID_MAX];           ///< WiFi SSID
    char password[BT_PROVISION_PASSWORD_MAX];   ///< WiFi密码
    bt_provision_wifi_auth_t security;          ///< 安全类型
    bool configured;                            ///< 是否已配置
} bt_provision_wifi_config_t;

/**
 * @brief 服务器配置结构体
 */
typedef struct {
    char server_url[BT_PROVISION_SERVER_URL_MAX];   ///< 服务器URL
    uint16_t server_port;                           ///< 服务器端口
    char device_id[BT_PROVISION_DEVICE_ID_MAX];     ///< 设备ID
    char api_key[BT_PROVISION_API_KEY_MAX];         ///< API密钥
    bool configured;                                ///< 是否已配置
} bt_provision_server_config_t;

/**
 * @brief 设备信息结构体
 */
typedef struct {
    char device_name[BT_PROVISION_DEVICE_NAME_MAX]; ///< 设备名称
    char mac_address[18];                           ///< MAC地址
    char firmware_version[16];                      ///< 固件版本
    char chip_model[16];                            ///< 芯片型号
    char wifi_status[16];                           ///< WiFi状态
    char provision_status[16];                      ///< 配网状态
} bt_provision_device_info_t;

/**
 * @brief 配网状态信息结构体
 */
typedef struct {
    bt_provision_state_t state;                     ///< 当前状态
    char wifi_status[16];                           ///< WiFi状态
    char server_status[16];                         ///< 服务器状态
    uint8_t progress;                               ///< 进度百分比
    char message[BT_PROVISION_MESSAGE_MAX];         ///< 状态消息
    char wifi_ip[16];                               ///< WiFi IP地址
} bt_provision_status_t;

/**
 * @brief 配网事件回调函数类型
 */
typedef void (*bt_provision_event_cb_t)(bt_provision_state_t state, bt_provision_err_t error, const char* message);

/**
 * @brief 配网配置结构体
 */
typedef struct {
    char device_name[BT_PROVISION_DEVICE_NAME_MAX]; ///< 设备名称
    bt_provision_event_cb_t event_callback;         ///< 事件回调函数
    bool auto_start_on_boot;                        ///< 启动时自动开始配网
    uint32_t advertising_timeout_ms;                ///< 广播超时时间
} bt_provision_config_t;

// ==================== 函数声明 ====================

/**
 * @brief 初始化蓝牙配网功能
 * 
 * @param config 配网配置
 * @return bt_provision_err_t 错误代码
 */
bt_provision_err_t bt_provision_init(const bt_provision_config_t* config);

/**
 * @brief 反初始化蓝牙配网功能
 * 
 * @return bt_provision_err_t 错误代码
 */
bt_provision_err_t bt_provision_deinit(void);

/**
 * @brief 启动蓝牙配网
 * 
 * @return bt_provision_err_t 错误代码
 */
bt_provision_err_t bt_provision_start(void);

/**
 * @brief 停止蓝牙配网
 * 
 * @return bt_provision_err_t 错误代码
 */
bt_provision_err_t bt_provision_stop(void);

/**
 * @brief 获取当前配网状态
 * 
 * @return bt_provision_state_t 当前状态
 */
bt_provision_state_t bt_provision_get_state(void);

/**
 * @brief 获取详细状态信息
 * 
 * @param status 状态信息结构体指针
 * @return bt_provision_err_t 错误代码
 */
bt_provision_err_t bt_provision_get_status(bt_provision_status_t* status);

/**
 * @brief 获取设备信息
 * 
 * @param info 设备信息结构体指针
 * @return bt_provision_err_t 错误代码
 */
bt_provision_err_t bt_provision_get_device_info(bt_provision_device_info_t* info);

/**
 * @brief 检查是否已配置WiFi
 * 
 * @return true 已配置，false 未配置
 */
bool bt_provision_is_wifi_configured(void);

/**
 * @brief 检查是否已配置服务器
 * 
 * @return true 已配置，false 未配置
 */
bool bt_provision_is_server_configured(void);

/**
 * @brief 获取WiFi配置
 * 
 * @param config WiFi配置结构体指针
 * @return bt_provision_err_t 错误代码
 */
bt_provision_err_t bt_provision_get_wifi_config(bt_provision_wifi_config_t* config);

/**
 * @brief 获取服务器配置
 * 
 * @param config 服务器配置结构体指针
 * @return bt_provision_err_t 错误代码
 */
bt_provision_err_t bt_provision_get_server_config(bt_provision_server_config_t* config);

/**
 * @brief 设置WiFi配置
 * 
 * @param config WiFi配置结构体指针
 * @return bt_provision_err_t 错误代码
 */
bt_provision_err_t bt_provision_set_wifi_config(const bt_provision_wifi_config_t* config);

/**
 * @brief 设置服务器配置
 * 
 * @param config 服务器配置结构体指针
 * @return bt_provision_err_t 错误代码
 */
bt_provision_err_t bt_provision_set_server_config(const bt_provision_server_config_t* config);

/**
 * @brief 重置所有配置
 * 
 * @param reset_wifi 是否重置WiFi配置
 * @param reset_server 是否重置服务器配置
 * @return bt_provision_err_t 错误代码
 */
bt_provision_err_t bt_provision_reset_config(bool reset_wifi, bool reset_server);

/**
 * @brief 测试WiFi连接
 * 
 * @return bt_provision_err_t 错误代码
 */
bt_provision_err_t bt_provision_test_wifi(void);

/**
 * @brief 测试服务器连接
 * 
 * @return bt_provision_err_t 错误代码
 */
bt_provision_err_t bt_provision_test_server(void);

/**
 * @brief 获取错误描述字符串
 * 
 * @param error 错误代码
 * @return const char* 错误描述
 */
const char* bt_provision_get_error_string(bt_provision_err_t error);

/**
 * @brief 获取状态描述字符串
 * 
 * @param state 状态代码
 * @return const char* 状态描述
 */
const char* bt_provision_get_state_string(bt_provision_state_t state);

#ifdef __cplusplus
}
#endif

#endif // BT_PROVISION_H