/**
 * @file device_control.h
 * @brief 设备控制模块
 * 
 * 提供统一的设备控制接口，支持LED、继电器、舵机等设备控制
 * 按照FIRMWARE_MANUAL.md要求实现
 */

#ifndef DEVICE_CONTROL_H
#define DEVICE_CONTROL_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 设备控制命令类型
 */
typedef enum {
    DEVICE_CONTROL_CMD_LED = 0,      ///< LED控制命令
    DEVICE_CONTROL_CMD_RELAY,        ///< 继电器控制命令
    DEVICE_CONTROL_CMD_SERVO,        ///< 舵机控制命令
    DEVICE_CONTROL_CMD_PWM,          ///< PWM控制命令
    DEVICE_CONTROL_CMD_UNKNOWN       ///< 未知命令
} device_control_cmd_t;

/**
 * @brief 设备控制动作类型
 */
typedef enum {
    DEVICE_CONTROL_ACTION_ON,         ///< 打开
    DEVICE_CONTROL_ACTION_OFF,        ///< 关闭
    DEVICE_CONTROL_ACTION_BRIGHTNESS, ///< 设置亮度（LED）
    DEVICE_CONTROL_ACTION_ANGLE,      ///< 设置角度（舵机）
    DEVICE_CONTROL_ACTION_UNKNOWN     ///< 未知动作
} device_control_action_t;

/**
 * @brief 设备控制命令结构
 */
typedef struct {
    device_control_cmd_t cmd_type;    ///< 命令类型
    uint8_t device_id;                ///< 设备ID（led_id, relay_id, servo_id, pwm_channel）
    device_control_action_t action;   ///< 动作类型
    union {
        bool state;                   ///< 开关状态（on/off）
        uint8_t brightness;           ///< 亮度值（0-255）
        uint16_t angle;               ///< 角度值（0-180）
        struct {
            uint32_t frequency;       ///< PWM频率（Hz）
            float duty_cycle;         ///< PWM占空比（0.0-100.0）
        } pwm;                        ///< PWM参数
    } value;                          ///< 控制值
} device_control_command_t;

/**
 * @brief 设备控制结果
 */
typedef struct {
    bool success;                     ///< 是否成功
    const char *error_msg;            ///< 错误信息（如果失败）
} device_control_result_t;

/**
 * @brief 初始化设备控制模块
 * 
 * @return esp_err_t 
 *   - ESP_OK: 成功
 *   - 其他: 错误
 */
esp_err_t device_control_init(void);

/**
 * @brief 解析JSON控制命令
 * 
 * 支持的JSON格式：
 * - LED: {"cmd":"led","led_id":1,"action":"on"}
 * - LED: {"cmd":"led","led_id":1,"action":"brightness","brightness":128}
 * - 继电器: {"cmd":"relay","relay_id":1,"action":"on"}
 * - 舵机: {"cmd":"servo","servo_id":1,"angle":90}
 * 
 * @param json_str JSON字符串
 * @param command 输出参数，解析后的控制命令
 * @return esp_err_t 
 *   - ESP_OK: 解析成功
 *   - ESP_ERR_INVALID_ARG: 参数错误
 *   - ESP_ERR_NOT_FOUND: 未找到命令
 *   - ESP_FAIL: 解析失败
 */
esp_err_t device_control_parse_json_command(const char *json_str, device_control_command_t *command);

/**
 * @brief 执行设备控制命令
 * 
 * @param command 控制命令
 * @param result 输出参数，执行结果
 * @return esp_err_t 
 *   - ESP_OK: 执行成功
 *   - ESP_ERR_INVALID_ARG: 参数错误
 *   - ESP_FAIL: 执行失败
 */
esp_err_t device_control_execute(const device_control_command_t *command, device_control_result_t *result);

/**
 * @brief 控制LED
 * 
 * @param led_id LED ID（1-4）
 * @param state 开关状态（true=开，false=关）
 * @return esp_err_t 
 */
esp_err_t device_control_led(uint8_t led_id, bool state);

/**
 * @brief 设置LED亮度
 * 
 * @param led_id LED ID（1-4）
 * @param brightness 亮度值（0-255）
 * @return esp_err_t 
 */
esp_err_t device_control_led_brightness(uint8_t led_id, uint8_t brightness);

/**
 * @brief 控制继电器
 * 
 * @param relay_id 继电器ID（1-2）
 * @param state 开关状态（true=开，false=关）
 * @return esp_err_t 
 */
esp_err_t device_control_relay(uint8_t relay_id, bool state);

/**
 * @brief 控制舵机
 * 
 * @param servo_id 舵机ID（1-2）
 * @param angle 角度值（0-180）
 * @return esp_err_t 
 */
esp_err_t device_control_servo(uint8_t servo_id, uint16_t angle);

/**
 * @brief 控制PWM输出
 * 
 * @param channel PWM通道（2=M2）
 * @param frequency 频率（Hz, 1-40000）
 * @param duty_cycle 占空比（0.0-100.0）
 * @return esp_err_t 
 */
esp_err_t device_control_pwm(uint8_t channel, uint32_t frequency, float duty_cycle);

#ifdef __cplusplus
}
#endif

#endif // DEVICE_CONTROL_H

