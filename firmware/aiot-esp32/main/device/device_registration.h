#ifndef DEVICE_REGISTRATION_H
#define DEVICE_REGISTRATION_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 设备注册信息结构体
 */
typedef struct {
    char device_id[64];         // 设备ID
    char device_uuid[128];      // 设备UUID
    char device_secret[128];    // 设备密钥
    char mac_address[18];       // MAC地址
    char message[256];          // 响应消息
    uint64_t registered_at;     // 注册时间戳
} device_registration_info_t;

/**
 * @brief 设备注册状态枚举
 */
typedef enum {
    DEVICE_REG_STATE_IDLE = 0,           ///< 空闲状态
    DEVICE_REG_STATE_REGISTERING,       ///< 注册中
    DEVICE_REG_STATE_REGISTERED,        ///< 已注册
    DEVICE_REG_STATE_FAILED              ///< 注册失败
} device_registration_state_t;

/**
 * @brief 设备注册事件类型
 */
typedef enum {
    DEVICE_REG_EVENT_STARTED = 0,        // 开始注册
    DEVICE_REG_EVENT_SUCCESS,            // 注册成功
    DEVICE_REG_EVENT_FAILED,             // 注册失败
    DEVICE_REG_EVENT_TIMEOUT             // 注册超时
} device_registration_event_t;

/**
 * @brief 设备注册事件回调函数类型
 */
typedef void (*device_registration_callback_t)(device_registration_event_t event, 
                                               const device_registration_info_t *info);

/**
 * @brief 设备注册配置
 */
typedef struct {
    char server_url[256];                           // 服务器URL
    uint16_t server_port;                          // 服务器端口
    uint32_t timeout_ms;                           // 超时时间(毫秒)
    uint8_t max_retry_count;                       // 最大重试次数
    device_registration_callback_t event_callback; // 事件回调函数
} device_registration_config_t;

/**
 * @brief 初始化设备注册模块
 * 
 * @param config 注册配置
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t device_registration_init(const device_registration_config_t *config);

/**
 * @brief 反初始化设备注册模块
 * 
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t device_registration_deinit(void);

/**
 * @brief 开始设备注册流程
 * 
 * @param firmware_version 固件版本
 * @param hardware_version 硬件版本
 * @return esp_err_t ESP_OK成功，其他值失败
 */
esp_err_t device_registration_start(const char *firmware_version, const char *hardware_version);

/**
 * @brief 获取注册状态
 * @return 当前注册状态
 */
device_registration_state_t device_registration_get_state(void);

/**
 * @brief 获取注册信息
 * @param info 输出参数，存储注册信息
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t device_registration_get_info(device_registration_info_t *info);

/**
 * @brief 检查设备是否已注册
 * @return true 已注册，false 未注册
 */
bool device_registration_is_registered(void);

/**
 * @brief 清除注册信息
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t device_registration_clear(void);

/**
 * @brief 从NVS加载注册信息
 * @param info 输出参数，存储注册信息
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t device_registration_load_from_nvs(device_registration_info_t *info);

/**
 * @brief 保存注册信息到NVS
 * @param info 要保存的注册信息
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t device_registration_save_to_nvs(const device_registration_info_t *info);

#ifdef __cplusplus
}
#endif

#endif // DEVICE_REGISTRATION_H