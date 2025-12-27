/**
 * @file ota_version.h
 * @brief OTA版本管理模块
 * @version 1.0
 * @date 2024-01-20
 */

#ifndef OTA_VERSION_H
#define OTA_VERSION_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 版本信息结构体 */
typedef struct {
    uint8_t major;
    uint8_t minor;
    uint8_t patch;
    uint32_t build;
    char version_string[32];
    char build_date[16];
    char build_time[16];
    char git_hash[16];
} ota_version_info_t;

/* 版本比较结果 */
typedef enum {
    VERSION_OLDER = -1,
    VERSION_SAME = 0,
    VERSION_NEWER = 1,
    VERSION_INVALID = -2
} version_compare_result_t;

/**
 * @brief 初始化版本管理模块
 * 
 * @return esp_err_t 
 */
esp_err_t ota_version_init(void);

/**
 * @brief 获取当前版本信息
 * 
 * @param version_info 版本信息结构体
 * @return esp_err_t 
 */
esp_err_t ota_version_get_current(ota_version_info_t *version_info);

/**
 * @brief 解析版本字符串
 * 
 * @param version_str 版本字符串 (如: "1.2.3")
 * @param version_info 版本信息结构体
 * @return esp_err_t 
 */
esp_err_t ota_version_parse_string(const char *version_str, ota_version_info_t *version_info);

/**
 * @brief 比较两个版本
 * 
 * @param version1 版本1
 * @param version2 版本2
 * @return version_compare_result_t 比较结果
 */
version_compare_result_t ota_version_compare(const ota_version_info_t *version1, const ota_version_info_t *version2);

/**
 * @brief 检查版本是否兼容
 * 
 * @param current_version 当前版本
 * @param target_version 目标版本
 * @return true 兼容
 * @return false 不兼容
 */
bool ota_version_is_compatible(const ota_version_info_t *current_version, const ota_version_info_t *target_version);

/**
 * @brief 格式化版本字符串
 * 
 * @param version_info 版本信息
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return esp_err_t 
 */
esp_err_t ota_version_format_string(const ota_version_info_t *version_info, char *buffer, size_t buffer_size);

/**
 * @brief 保存版本信息到NVS
 * 
 * @param version_info 版本信息
 * @return esp_err_t 
 */
esp_err_t ota_version_save_to_nvs(const ota_version_info_t *version_info);

/**
 * @brief 从NVS加载版本信息
 * 
 * @param version_info 版本信息
 * @return esp_err_t 
 */
esp_err_t ota_version_load_from_nvs(ota_version_info_t *version_info);

/**
 * @brief 获取版本历史记录
 * 
 * @param history 版本历史数组
 * @param max_count 最大记录数
 * @param actual_count 实际记录数
 * @return esp_err_t 
 */
esp_err_t ota_version_get_history(ota_version_info_t *history, size_t max_count, size_t *actual_count);

/**
 * @brief 清除版本历史记录
 * 
 * @return esp_err_t 
 */
esp_err_t ota_version_clear_history(void);

#ifdef __cplusplus
}
#endif

#endif /* OTA_VERSION_H */