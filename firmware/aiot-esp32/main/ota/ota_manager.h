/**
 * @file ota_manager.h
 * @brief OTA固件更新管理器
 * 
 * 参考xiaozhi-esp32项目的OTA实现
 * https://github.com/78/xiaozhi-esp32
 */

#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** OTA进度回调函数类型 */
typedef void (*ota_progress_callback_t)(int progress, size_t speed);

/** 固件信息 */
typedef struct {
    char version[32];           ///< 固件版本号
    char download_url[256];     ///< 下载URL
    uint32_t file_size;         ///< 文件大小
    char checksum[128];         ///< SHA256校验和
    char changelog[256];        ///< 更新日志
    bool force_update;          ///< 是否强制更新
    bool available;             ///< 是否有新版本可用
} firmware_info_t;

/**
 * @brief 初始化OTA管理器
 * 
 * @return 
 *   - ESP_OK: 成功
 *   - 其他: 错误
 */
esp_err_t ota_manager_init(void);

/**
 * @brief 检查固件版本
 * 
 * 从配置服务获取设备配置，检查是否有新固件可用
 * 
 * @param provision_server 配置服务器地址
 * @param mac_address 设备MAC地址
 * @param current_version 当前固件版本
 * @param fw_info 输出参数，固件信息
 * 
 * @return 
 *   - ESP_OK: 检查成功（fw_info->available表示是否有更新）
 *   - ESP_FAIL: 检查失败
 */
esp_err_t ota_manager_check_version(
    const char *provision_server,
    const char *mac_address,
    const char *current_version,
    firmware_info_t *fw_info
);

/**
 * @brief 开始OTA升级
 * 
 * 从指定URL下载固件并安装
 * 参考xiaozhi的流式下载实现
 * 
 * @param firmware_url 固件下载URL
 * @param callback 进度回调函数（可选）
 * 
 * @return 
 *   - ESP_OK: 升级成功，准备重启
 *   - ESP_FAIL: 升级失败
 */
esp_err_t ota_manager_start_upgrade(
    const char *firmware_url,
    ota_progress_callback_t callback
);

/**
 * @brief 标记当前固件为有效
 * 
 * OTA更新后首次启动时调用，防止回滚
 * 参考xiaozhi的MarkCurrentVersionValid()
 * 
 * @return 
 *   - ESP_OK: 成功
 *   - 其他: 错误
 */
esp_err_t ota_manager_mark_valid(void);

/**
 * @brief 比较版本号
 * 
 * 参考xiaozhi的IsNewVersionAvailable()
 * 支持格式: "1.2.3"
 * 
 * @param current_version 当前版本
 * @param new_version 新版本
 * 
 * @return 
 *   - true: 新版本可用
 *   - false: 新版本不可用或相同
 */
bool ota_manager_is_new_version(const char *current_version, const char *new_version);

/**
 * @brief 获取当前运行的固件版本
 * 
 * @return 版本字符串
 */
const char* ota_manager_get_current_version(void);

#ifdef __cplusplus
}
#endif

#endif // OTA_MANAGER_H
