/**
 * @file wifi_config.h
 * @brief WiFi配网模块
 * 
 * 提供简单的WiFi配网功能，支持：
 * - Boot按键触发配网模式
 * - AP模式热点配网
 * - Web界面配置WiFi和服务器地址（服务器地址统一用于HTTP API、MQTT和OTA）
 * 
 * @author AIOT Team
 * @date 2024
 */

#ifndef WIFI_CONFIG_H
#define WIFI_CONFIG_H

#include <stdbool.h>
#include "esp_err.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

// 配网状态
typedef enum {
    WIFI_CONFIG_STATE_IDLE = 0,         // 空闲状态
    WIFI_CONFIG_STATE_AP_STARTING,      // AP模式启动中
    WIFI_CONFIG_STATE_AP_STARTED,       // AP模式已启动
    WIFI_CONFIG_STATE_CONFIGURING,      // 配网中
    WIFI_CONFIG_STATE_CONNECTING,       // 连接WiFi中
    WIFI_CONFIG_STATE_CONNECTED,        // WiFi已连接
    WIFI_CONFIG_STATE_FAILED,           // 配网失败
} wifi_config_state_t;

// 配网事件类型
typedef enum {
    WIFI_CONFIG_EVENT_AP_STARTED = 0,   // AP模式启动
    WIFI_CONFIG_EVENT_CLIENT_CONNECTED, // 客户端连接
    WIFI_CONFIG_EVENT_CONFIG_RECEIVED,  // 收到配置
    WIFI_CONFIG_EVENT_WIFI_CONNECTED,   // WiFi连接成功
    WIFI_CONFIG_EVENT_WIFI_FAILED,      // WiFi连接失败
    WIFI_CONFIG_EVENT_TIMEOUT,          // 配网超时
} wifi_config_event_t;

// WiFi配置结构体
// 注意：服务器地址统一使用server_config命名空间中的base_address，不再单独存储OTA地址
typedef struct {
    char ssid[32];          // WiFi SSID
    char password[64];      // WiFi密码
    bool configured;        // 是否已配置
} wifi_config_data_t;

// 配网事件回调函数类型
typedef void (*wifi_config_event_cb_t)(wifi_config_event_t event, void *data);

/**
 * @brief 初始化WiFi配网模块
 * 
 * @param event_cb 事件回调函数
 * @return esp_err_t 
 */
esp_err_t wifi_config_init(wifi_config_event_cb_t event_cb);

/**
 * @brief 启动配网模式
 * 
 * 启动AP模式热点，提供Web配置界面
 * 
 * @return esp_err_t 
 */
esp_err_t wifi_config_start(void);

/**
 * @brief 停止配网模式
 * 
 * @return esp_err_t 
 */
esp_err_t wifi_config_stop(void);

/**
 * @brief 获取当前配网状态
 * 
 * @return wifi_config_state_t 
 */
wifi_config_state_t wifi_config_get_state(void);

/**
 * @brief 检查是否需要进入配网模式
 * 
 * 检查NVS中的force_config标志
 * 
 * @return true 需要进入配网模式
 * @return false 不需要进入配网模式
 */
bool wifi_config_should_start(void);

/**
 * @brief 设置强制配网标志
 * 
 * 设置后重启将进入配网模式
 * 
 * @return esp_err_t 
 */
esp_err_t wifi_config_set_force_flag(void);

/**
 * @brief 清除强制配网标志
 * 
 * @return esp_err_t 
 */
esp_err_t wifi_config_clear_force_flag(void);

/**
 * @brief 保存WiFi配置到NVS
 * 
 * @param config WiFi配置数据
 * @return esp_err_t 
 */
esp_err_t wifi_config_save(const wifi_config_data_t *config);

/**
 * @brief 从NVS加载WiFi配置
 * 
 * @param config 输出的WiFi配置数据
 * @return esp_err_t 
 */
esp_err_t wifi_config_load(wifi_config_data_t *config);

/**
 * @brief 获取AP模式的SSID
 * 
 * @return const char* AP模式SSID
 */
const char* wifi_config_get_ap_ssid(void);

/**
 * @brief 获取Web服务器URL
 * 
 * @return const char* Web服务器URL
 */
const char* wifi_config_get_web_url(void);

#ifdef __cplusplus
}
#endif

#endif // WIFI_CONFIG_H