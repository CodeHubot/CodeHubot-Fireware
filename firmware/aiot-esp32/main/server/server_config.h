/**
 * @file server_config.h
 * @brief 统一服务器配置模块
 * 
 * 从NVS读取服务器基础地址，提供动态URL构建功能
 * 重要约束：服务器地址必须从NVS读取并动态构建URL，禁止硬编码
 */

#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// NVS命名空间
#define SERVER_CONFIG_NAMESPACE "server_config"

// NVS键名
#define NVS_KEY_BASE_ADDRESS "base_address"

// 默认服务器地址（仅用于占位，不用于实际连接，包含协议前缀）
#define DEFAULT_SERVER_BASE_ADDRESS ""  // 通过配网获取

// 默认端口配置（不从NVS读取）
#define DEFAULT_HTTP_PORT 8000
#define DEFAULT_MQTT_PORT 1883

/**
 * @brief 统一服务器配置结构体
 */
typedef struct {
    char base_address[64];      ///< 服务器基础地址（包含协议前缀：http://或https://，结尾不包含/，从NVS读取）
    uint16_t http_port;          ///< HTTP端口（编译配置，不从NVS读取）
    uint16_t mqtt_port;          ///< MQTT端口（编译配置，不从NVS读取）
} unified_server_config_t;

/**
 * @brief 从NVS加载服务器配置
 * 
 * @param config 输出参数，加载的配置
 * @return esp_err_t 
 *   - ESP_OK: 成功
 *   - ESP_ERR_NOT_FOUND: NVS中不存在配置
 *   - 其他: 错误
 */
esp_err_t server_config_load_from_nvs(unified_server_config_t *config);

/**
 * @brief 获取默认服务器配置（用于兜底）
 * 
 * @param config 输出参数，默认配置
 * @return esp_err_t 总是返回ESP_OK
 */
esp_err_t server_config_get_default(unified_server_config_t *config);

/**
 * @brief 保存服务器配置到NVS
 * 
 * @param config 要保存的配置
 * @return esp_err_t 
 *   - ESP_OK: 成功
 *   - 其他: 错误
 */
esp_err_t server_config_save_to_nvs(const unified_server_config_t *config);

/**
 * @brief 根据服务器配置构建完整URL
 * 
 * base_address已经包含协议前缀（http://或https://），直接拼接端口和路径
 * 格式：base_address:port/path
 * 
 * @param config 服务器配置（base_address包含协议前缀）
 * @param protocol 协议类型（已废弃，保留用于兼容性，实际从base_address中提取）
 * @param path API路径（如"/api/devices/mac/lookup"）
 * @param out_buf 输出缓冲区
 * @param out_buf_size 输出缓冲区大小
 * @return esp_err_t 
 *   - ESP_OK: 成功
 *   - ESP_ERR_INVALID_ARG: 参数错误
 *   - ESP_ERR_INVALID_SIZE: 缓冲区太小
 */
esp_err_t server_config_build_url(
    const unified_server_config_t *config,
    const char *protocol,
    const char *path,
    char *out_buf,
    size_t out_buf_size
);

/**
 * @brief 构建HTTP API URL（使用默认HTTP端口）
 * 
 * @param config 服务器配置
 * @param path API路径
 * @param out_buf 输出缓冲区
 * @param out_buf_size 输出缓冲区大小
 * @return esp_err_t 
 */
esp_err_t server_config_build_http_url(
    const unified_server_config_t *config,
    const char *path,
    char *out_buf,
    size_t out_buf_size
);

/**
 * @brief 构建MQTT Broker地址（用于MQTT客户端）
 * 
 * @param config 服务器配置
 * @param out_buf 输出缓冲区
 * @param out_buf_size 输出缓冲区大小
 * @return esp_err_t 
 */
esp_err_t server_config_build_mqtt_broker_url(
    const unified_server_config_t *config,
    char *out_buf,
    size_t out_buf_size
);

#ifdef __cplusplus
}
#endif

#endif // SERVER_CONFIG_H

