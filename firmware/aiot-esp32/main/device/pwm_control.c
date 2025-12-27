/**
 * @file pwm_control.c
 * @brief PWM控制模块实现
 */

#include "pwm_control.h"
#include "driver/ledc.h"
#include "esp_log.h"

static const char *TAG = "PWM_CONTROL";

// PWM通道配置（使用LEDC外设）
#define PWM_TIMER_M1           LEDC_TIMER_0  // M1使用独立定时器
#define PWM_TIMER_M2           LEDC_TIMER_1  // M2使用独立定时器
#define PWM_MODE               LEDC_LOW_SPEED_MODE
#define PWM_DUTY_RESOLUTION    LEDC_TIMER_13_BIT  // 13位分辨率 (0-8191)

// M1 PWM端口配置（GPIO48 - 原Servo1引脚）
#define PWM_M1_GPIO            48
#define PWM_M1_CHANNEL         LEDC_CHANNEL_0

// M2 PWM端口配置（GPIO40 - 原Servo2引脚）
#define PWM_M2_GPIO            40
#define PWM_M2_CHANNEL         LEDC_CHANNEL_2

static bool s_initialized = false;
static pwm_config_t s_pwm_configs[8] = {0};

/**
 * @brief 初始化PWM控制模块
 */
esp_err_t pwm_control_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "PWM control already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing PWM control module...");

    // 配置M1 LEDC定时器
    ledc_timer_config_t ledc_timer_m1 = {
        .speed_mode       = PWM_MODE,
        .timer_num        = PWM_TIMER_M1,
        .duty_resolution  = PWM_DUTY_RESOLUTION,
        .freq_hz          = 1000,  // 默认1kHz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    esp_err_t ret = ledc_timer_config(&ledc_timer_m1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure M1 LEDC timer: %s", esp_err_to_name(ret));
        return ret;
    }

    // 配置M2 LEDC定时器
    ledc_timer_config_t ledc_timer_m2 = {
        .speed_mode       = PWM_MODE,
        .timer_num        = PWM_TIMER_M2,
        .duty_resolution  = PWM_DUTY_RESOLUTION,
        .freq_hz          = 1000,  // 默认1kHz
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ret = ledc_timer_config(&ledc_timer_m2);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure M2 LEDC timer: %s", esp_err_to_name(ret));
        return ret;
    }

    // 配置M1 PWM通道
    ledc_channel_config_t ledc_channel_m1 = {
        .speed_mode     = PWM_MODE,
        .channel        = PWM_M1_CHANNEL,
        .timer_sel      = PWM_TIMER_M1,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PWM_M1_GPIO,
        .duty           = 0,  // 初始占空比为0
        .hpoint         = 0
    };
    ret = ledc_channel_config(&ledc_channel_m1);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure M1 LEDC channel: %s", esp_err_to_name(ret));
        return ret;
    }

    // 配置M2 PWM通道
    ledc_channel_config_t ledc_channel_m2 = {
        .speed_mode     = PWM_MODE,
        .channel        = PWM_M2_CHANNEL,
        .timer_sel      = PWM_TIMER_M2,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = PWM_M2_GPIO,
        .duty           = 0,  // 初始占空比为0
        .hpoint         = 0
    };
    ret = ledc_channel_config(&ledc_channel_m2);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure M2 LEDC channel: %s", esp_err_to_name(ret));
        return ret;
    }

    // 初始化M1配置
    s_pwm_configs[0].gpio_num = PWM_M1_GPIO;  // channel 1 = M1
    s_pwm_configs[0].frequency = 1000;
    s_pwm_configs[0].duty_cycle = 0.0;
    s_pwm_configs[0].enabled = false;

    // 初始化M2配置
    s_pwm_configs[1].gpio_num = PWM_M2_GPIO;  // channel 2 = M2
    s_pwm_configs[1].frequency = 1000;
    s_pwm_configs[1].duty_cycle = 0.0;
    s_pwm_configs[1].enabled = false;

    s_initialized = true;
    ESP_LOGI(TAG, "✅ PWM control module initialized (M1 on GPIO%d, M2 on GPIO%d)", 
             PWM_M1_GPIO, PWM_M2_GPIO);
    return ESP_OK;
}

/**
 * @brief 设置PWM通道参数
 */
esp_err_t pwm_control_set(uint8_t channel, uint32_t frequency, float duty_cycle)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "PWM control not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    // 支持M1 (channel 1) 和 M2 (channel 2)
    if (channel != 1 && channel != 2) {
        ESP_LOGE(TAG, "Unsupported PWM channel: %d (supported: 1=M1, 2=M2)", channel);
        return ESP_ERR_INVALID_ARG;
    }

    // 参数验证
    if (frequency < 1 || frequency > 40000) {
        ESP_LOGE(TAG, "Invalid frequency: %lu (must be 1-40000 Hz)", frequency);
        return ESP_ERR_INVALID_ARG;
    }
    if (duty_cycle < 0.0 || duty_cycle > 100.0) {
        ESP_LOGE(TAG, "Invalid duty cycle: %.2f (must be 0.0-100.0)", duty_cycle);
        return ESP_ERR_INVALID_ARG;
    }

    // 确定通道配置
    uint8_t config_index = (channel == 1) ? 0 : 1;  // channel 1 -> index 0, channel 2 -> index 1
    ledc_channel_t ledc_channel = (channel == 1) ? PWM_M1_CHANNEL : PWM_M2_CHANNEL;
    ledc_timer_t ledc_timer = (channel == 1) ? PWM_TIMER_M1 : PWM_TIMER_M2;
    const char* port_name = (channel == 1) ? "M1" : "M2";

    ESP_LOGI(TAG, "Setting PWM %s: freq=%lu Hz, duty=%.2f%%", port_name, frequency, duty_cycle);

    // 更新频率（如果改变）
    if (s_pwm_configs[config_index].frequency != frequency) {
        esp_err_t ret = ledc_set_freq(PWM_MODE, ledc_timer, frequency);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set frequency: %s", esp_err_to_name(ret));
            return ret;
        }
        s_pwm_configs[config_index].frequency = frequency;
    }

    // 计算占空比（13位分辨率）
    uint32_t duty = (uint32_t)((duty_cycle / 100.0) * ((1 << PWM_DUTY_RESOLUTION) - 1));
    
    // 设置占空比
    esp_err_t ret = ledc_set_duty(PWM_MODE, ledc_channel, duty);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set duty cycle: %s", esp_err_to_name(ret));
        return ret;
    }

    // 更新占空比
    ret = ledc_update_duty(PWM_MODE, ledc_channel);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update duty cycle: %s", esp_err_to_name(ret));
        return ret;
    }

    s_pwm_configs[config_index].duty_cycle = duty_cycle;
    s_pwm_configs[config_index].enabled = (duty_cycle > 0.0);

    ESP_LOGI(TAG, "✅ PWM %s set: %lu Hz, %.2f%% (duty value: %lu)", 
             port_name, frequency, duty_cycle, duty);
    
    return ESP_OK;
}

/**
 * @brief 启用/禁用PWM输出
 */
esp_err_t pwm_control_enable(uint8_t channel, bool enable)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "PWM control not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    if (channel != 1 && channel != 2) {
        ESP_LOGE(TAG, "Unsupported PWM channel: %d (supported: 1=M1, 2=M2)", channel);
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t config_index = (channel == 1) ? 0 : 1;

    if (enable) {
        // 恢复之前的占空比
        float duty = s_pwm_configs[config_index].duty_cycle;
        if (duty <= 0.0) {
            duty = 50.0;  // 默认50%
        }
        return pwm_control_set(channel, s_pwm_configs[config_index].frequency, duty);
    } else {
        // 设置为0关闭输出
        return pwm_control_set(channel, s_pwm_configs[config_index].frequency, 0.0);
    }
}

/**
 * @brief 获取PWM通道配置
 */
esp_err_t pwm_control_get_config(uint8_t channel, pwm_config_t *config)
{
    if (!s_initialized || !config) {
        return ESP_ERR_INVALID_ARG;
    }

    if (channel != 1 && channel != 2) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t config_index = (channel == 1) ? 0 : 1;
    *config = s_pwm_configs[config_index];
    return ESP_OK;
}

