/**
 * @file pwm_control.h
 * @brief PWM控制模块头文件
 * 
 * 支持自定义频率和占空比的PWM输出，可用于：
 * - LED亮度调节
 * - 电机速度控制
 * - 蜂鸣器音调控制
 * - 其他需要PWM信号的设备
 */

#ifndef PWM_CONTROL_H
#define PWM_CONTROL_H

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief PWM通道配置
 */
typedef struct {
    uint32_t frequency;    ///< PWM频率 (Hz)
    float duty_cycle;      ///< 占空比 (0.0-100.0)
    uint8_t gpio_num;      ///< GPIO引脚号
    bool enabled;          ///< 是否启用
} pwm_config_t;

/**
 * @brief 初始化PWM控制模块
 * 
 * @return esp_err_t 
 */
esp_err_t pwm_control_init(void);

/**
 * @brief 设置PWM通道参数
 * 
 * @param channel PWM通道ID (1-8)
 * @param frequency 频率 (Hz, 1-40000)
 * @param duty_cycle 占空比 (0.0-100.0)
 * @return esp_err_t 
 */
esp_err_t pwm_control_set(uint8_t channel, uint32_t frequency, float duty_cycle);

/**
 * @brief 启用/禁用PWM输出
 * 
 * @param channel PWM通道ID
 * @param enable true=启用, false=禁用
 * @return esp_err_t 
 */
esp_err_t pwm_control_enable(uint8_t channel, bool enable);

/**
 * @brief 获取PWM通道配置
 * 
 * @param channel PWM通道ID
 * @param config 输出配置
 * @return esp_err_t 
 */
esp_err_t pwm_control_get_config(uint8_t channel, pwm_config_t *config);

#ifdef __cplusplus
}
#endif

#endif // PWM_CONTROL_H

