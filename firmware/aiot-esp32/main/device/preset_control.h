/**
 * @file preset_control.h
 * @brief 预设控制模块
 * 
 * 支持预设命令和组合动作，按照FIRMWARE_MANUAL.md要求实现
 */

#ifndef PRESET_CONTROL_H
#define PRESET_CONTROL_H

#include "esp_err.h"
#include "cJSON.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 预设设备类型
 */
typedef enum {
    PRESET_DEVICE_TYPE_LED = 0,    ///< LED设备
    PRESET_DEVICE_TYPE_SERVO,      ///< 舵机设备
    PRESET_DEVICE_TYPE_RELAY,      ///< 继电器设备
    PRESET_DEVICE_TYPE_PWM,        ///< PWM输出设备
    PRESET_DEVICE_TYPE_UNKNOWN     ///< 未知设备类型
} preset_device_type_t;

/**
 * @brief 预设命令结构（新格式）
 * 
 * 按照FIRMWARE_MANUAL.md要求的新格式：
 * {
 *   "cmd":"preset",
 *   "device_type":"led|servo|relay",
 *   "preset_type":"...",
 *   "device_id":0,
 *   "parameters":{...}
 * }
 */
typedef struct {
    preset_device_type_t device_type;  ///< 设备类型
    char preset_type[32];              ///< 预设类型（如"blink", "wave", "sequence"等）
    uint8_t device_id;                 ///< 设备ID（0表示所有设备）
    cJSON *parameters;                  ///< 参数JSON对象（需要解析）
} preset_control_command_t;

/**
 * @brief 预设控制结果
 */
typedef struct {
    bool success;                      ///< 是否成功
    const char *error_msg;             ///< 错误信息（如果失败）
} preset_control_result_t;

/**
 * @brief 初始化预设控制模块
 * 
 * @return esp_err_t 
 *   - ESP_OK: 成功
 *   - 其他: 错误
 */
esp_err_t preset_control_init(void);

/**
 * @brief 解析预设控制命令（新格式）
 * 
 * 支持的JSON格式：
 * {
 *   "cmd":"preset",
 *   "device_type":"led|servo|relay",
 *   "preset_type":"...",
 *   "device_id":0,
 *   "parameters":{...}
 * }
 * 
 * @param json_str JSON字符串
 * @param command 输出参数，解析后的预设命令
 * @return esp_err_t 
 *   - ESP_OK: 解析成功
 *   - ESP_ERR_INVALID_ARG: 参数错误
 *   - ESP_ERR_NOT_FOUND: 未找到预设命令
 *   - ESP_FAIL: 解析失败
 */
esp_err_t preset_control_parse_json_command(const char *json_str, preset_control_command_t *command);

/**
 * @brief 执行预设控制命令
 * 
 * @param command 预设命令
 * @param result 输出参数，执行结果
 * @return esp_err_t 
 *   - ESP_OK: 执行成功
 *   - ESP_ERR_INVALID_ARG: 参数错误
 *   - ESP_FAIL: 执行失败
 */
esp_err_t preset_control_execute(const preset_control_command_t *command, preset_control_result_t *result);

/**
 * @brief 释放预设命令资源
 * 
 * @param command 预设命令
 */
void preset_control_free_command(preset_control_command_t *command);

#ifdef __cplusplus
}
#endif

#endif // PRESET_CONTROL_H

