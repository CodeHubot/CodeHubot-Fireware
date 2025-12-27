/**
 * @file provisioning_client.h
 * @brief 配置服务客户端（GET请求方式）
 * 
 * 调用新的配置服务GET接口获取设备配置
 */

#ifndef PROVISIONING_CLIENT_H
#define PROVISIONING_CLIENT_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 配置信息结构体 */
typedef struct {
    // 设备信息
    char device_id[128];           ///< 设备ID
    char device_uuid[128];         ///< 设备UUID
    char mac_address[18];          ///< MAC地址
    char product_id[64];           ///< 产品标识符
    
    // MQTT配置
    bool has_mqtt_config;          ///< 是否有MQTT配置
    char mqtt_broker[256];         ///< MQTT服务器地址
    int mqtt_port;                 ///< MQTT端口
    char mqtt_username[128];       ///< MQTT用户名
    char mqtt_password[128];       ///< MQTT密码
    bool mqtt_use_ssl;             ///< 是否使用SSL
    char mqtt_topic_data[256];     ///< 数据上报主题
    char mqtt_topic_control[256];  ///< 控制命令主题
    char mqtt_topic_status[256];   ///< 状态上报主题
    char mqtt_topic_heartbeat[256];///< 心跳主题
    
    // 固件更新信息
    bool has_firmware_update;      ///< 是否有固件更新
    char firmware_version[32];     ///< 固件版本
    char firmware_url[512];        ///< 固件下载URL
    uint32_t firmware_size;        ///< 固件大小
    char firmware_checksum[128];   ///< 固件校验和
    char firmware_changelog[256];  ///< 更新日志
} provisioning_config_t;

/**
 * @brief 从配置服务获取设备配置
 * 
 * 使用GET请求：/device/info?mac=AA:BB:CC&product_id=ESP32-S3-Dev-01&firmware_version=1.0.0
 * 
 * @param server_address 配置服务地址（如：http://provision.example.com）
 * @param product_id 产品标识符（必需，如："ESP32-S3-Dev-01"）
 * @param firmware_version 固件版本（可选，用于检查更新）
 * @param config 输出参数，配置信息
 * 
 * @return 
 *   - ESP_OK: 成功
 *   - ESP_ERR_NOT_FOUND: 设备未注册（404）
 *   - ESP_FAIL: 其他错误
 */
esp_err_t provisioning_client_get_config(
    const char *server_address,
    const char *product_id,
    const char *firmware_version,
    provisioning_config_t *config
);

#ifdef __cplusplus
}
#endif

#endif // PROVISIONING_CLIENT_H
