/**
 * @file device_config.h
 * @brief 设备配置获取客户端
 * 
 * 从服务器获取设备配置信息，检查设备是否已注册
 * 
 * @author AIOT Team
 * @date 2026-01-01
 */

#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 设备配置信息结构体 */
typedef struct {
    // 设备信息
    char device_id[128];           ///< 设备ID
    char device_uuid[128];         ///< 设备UUID
    char mac_address[18];          ///< MAC地址
    
    // MQTT配置
    bool has_mqtt_config;          ///< 是否有MQTT配置
    char mqtt_broker[256];         ///< MQTT服务器地址
    int mqtt_port;                 ///< MQTT端口
    char mqtt_username[128];       ///< MQTT用户名（通常是device_uuid）
    char mqtt_password[128];       ///< MQTT密码（通常是device_secret）
    
    // MQTT主题
    char mqtt_topic_data[256];     ///< 数据上报主题
    char mqtt_topic_control[256];  ///< 控制命令主题
    char mqtt_topic_status[256];   ///< 状态上报主题
    char mqtt_topic_heartbeat[256];///< 心跳主题
} device_config_t;

/**
 * @brief 从服务器获取设备配置
 * 
 * GET请求：http://server/device/info?mac=AA:BB:CC&product_id=ESP32-C3-OLED-01&firmware_version=1.0.0
 * 
 * @param server_address 服务器地址（如：conf.aiot.powertechhub.com）
 * @param product_id 产品ID
 * @param firmware_version 固件版本
 * @param config 输出参数，设备配置
 * 
 * @return 
 *   - ESP_OK: 设备已注册，配置获取成功
 *   - ESP_ERR_NOT_FOUND: 设备未注册（404）
 *   - ESP_FAIL: 其他错误
 */
esp_err_t device_config_get_from_server(
    const char *server_address,
    const char *product_id,
    const char *firmware_version,
    device_config_t *config
);

#ifdef __cplusplus
}
#endif

#endif // DEVICE_CONFIG_H

