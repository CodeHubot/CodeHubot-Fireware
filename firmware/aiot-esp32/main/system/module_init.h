/**
 * @file module_init.h
 * @brief 模块初始化管理
 * 
 * 按照FIRMWARE_MANUAL.md要求的顺序实现完整的初始化流程
 */

#ifndef MODULE_INIT_H
#define MODULE_INIT_H

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 设备UUID获取结果
 */
typedef struct {
    char device_id[64];         ///< 设备ID
    char device_uuid[128];      ///< 设备UUID
    char device_secret[128];    ///< 设备密钥
    char mac_address[18];       ///< MAC地址
} device_uuid_info_t;

/**
 * @brief 通过MAC地址从后端获取设备UUID
 * 
 * 使用动态构建的URL调用 /api/devices/mac/lookup 接口
 * 硬性约束：如果无法获取UUID，系统不得继续执行
 * 
 * @param srv_config 服务器配置（从NVS读取）
 * @param firmware_version 固件版本
 * @param hardware_version 硬件版本
 * @param uuid_info 输出参数，获取的设备UUID信息
 * @param max_retries 最大重试次数（默认3次）
 * @return esp_err_t 
 *   - ESP_OK: 成功获取UUID
 *   - ESP_ERR_NOT_FOUND: 后端返回404（设备未注册）
 *   - ESP_ERR_TIMEOUT: 超时
 *   - ESP_FAIL: 其他错误
 */
esp_err_t fetch_uuid_by_mac(
    const void *srv_config,  // unified_server_config_t*
    const char *firmware_version,
    const char *hardware_version,
    device_uuid_info_t *uuid_info,
    int max_retries
);

/**
 * @brief 初始化设备ID和MQTT主题（临时值）
 * 
 * 仅生成临时device_id（MAC基础），不构建任何主题（待获取UUID后再确定）
 * 
 * @param device_id 输出参数，临时设备ID（MAC基础）
 * @return esp_err_t 
 */
esp_err_t init_device_id_and_topics(char *device_id);

/**
 * @brief 初始化WiFi和网络
 * 
 * 从NVS加载WiFi配置，若缺失则进入配网模式
 * 
 * @return esp_err_t 
 */
esp_err_t init_wifi_and_network(void);

/**
 * @brief 初始化网络服务
 * 
 * 调用fetch_uuid_by_mac获取设备UUID
 * 如果失败，系统进入停机状态
 * 
 * @param srv_config 服务器配置
 * @param firmware_version 固件版本
 * @param hardware_version 硬件版本
 * @param uuid_info 输出参数，获取的设备UUID信息
 * @return esp_err_t 
 *   - ESP_OK: 成功
 *   - 其他: 失败，系统应停机
 */
esp_err_t init_network_services(
    const void *srv_config,  // unified_server_config_t*
    const char *firmware_version,
    const char *hardware_version,
    device_uuid_info_t *uuid_info
);

/**
 * @brief 处理配网模式
 * 
 * 进入配网模式，等待用户配置WiFi和服务器地址
 * 
 * @return esp_err_t 
 */
esp_err_t handle_config_mode(void);

#ifdef __cplusplus
}
#endif

#endif // MODULE_INIT_H

