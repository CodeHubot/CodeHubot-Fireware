/**
 * @file device_control.c
 * @brief 设备控制模块实现
 * 
 * 按照FIRMWARE_MANUAL.md要求实现设备控制功能
 */

#include "device_control.h"
#include "pwm_control.h"
#include "../bsp/bsp_interface.h"
// 根据Kconfig配置选择板子BSP头文件
#ifdef CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_RAIN
    #include "../boards/esp32-s3-devkit-rain/bsp_esp32_s3_devkit_rain.h"
    #define BSP_LED1_CONTROL bsp_esp32_s3_devkit_rain_led1_control
    #define BSP_LED2_CONTROL bsp_esp32_s3_devkit_rain_led2_control
    #define BSP_LED3_CONTROL bsp_esp32_s3_devkit_rain_led3_control
    #define BSP_LED4_CONTROL bsp_esp32_s3_devkit_rain_led4_control
    #define BSP_LED1_SET_BRIGHTNESS bsp_esp32_s3_devkit_rain_led1_set_brightness
    #define BSP_LED2_SET_BRIGHTNESS bsp_esp32_s3_devkit_rain_led2_set_brightness
    #define BSP_LED3_SET_BRIGHTNESS bsp_esp32_s3_devkit_rain_led3_set_brightness
    #define BSP_LED4_SET_BRIGHTNESS bsp_esp32_s3_devkit_rain_led4_set_brightness
    #define BSP_RELAY1_CONTROL bsp_esp32_s3_devkit_rain_relay1_control
    #define BSP_RELAY2_CONTROL bsp_esp32_s3_devkit_rain_relay2_control
    #define BSP_SERVO1_SET_ANGLE bsp_esp32_s3_devkit_rain_servo1_set_angle
    #define BSP_SERVO2_SET_ANGLE bsp_esp32_s3_devkit_rain_servo2_set_angle
#elif defined(CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_LITE)
    #include "../boards/esp32-s3-devkit-lite/bsp_esp32_s3_devkit_lite.h"
    #define BSP_LED1_CONTROL bsp_esp32_s3_devkit_lite_led1_control
    #define BSP_LED2_CONTROL bsp_esp32_s3_devkit_lite_led2_control
    #define BSP_LED3_CONTROL bsp_esp32_s3_devkit_lite_led3_control
    #define BSP_LED4_CONTROL bsp_esp32_s3_devkit_lite_led4_control
    #define BSP_LED1_SET_BRIGHTNESS bsp_esp32_s3_devkit_lite_led1_set_brightness
    #define BSP_LED2_SET_BRIGHTNESS bsp_esp32_s3_devkit_lite_led2_set_brightness
    #define BSP_LED3_SET_BRIGHTNESS bsp_esp32_s3_devkit_lite_led3_set_brightness
    #define BSP_LED4_SET_BRIGHTNESS bsp_esp32_s3_devkit_lite_led4_set_brightness
    #define BSP_RELAY1_CONTROL bsp_esp32_s3_devkit_lite_relay1_control
    #define BSP_RELAY2_CONTROL bsp_esp32_s3_devkit_lite_relay2_control
    // Lite板子没有舵机，需要定义空函数
    static inline hal_err_t bsp_esp32_s3_devkit_lite_servo1_set_angle(uint16_t angle) { (void)angle; return HAL_ERROR_NOT_SUPPORTED; }
    static inline hal_err_t bsp_esp32_s3_devkit_lite_servo2_set_angle(uint16_t angle) { (void)angle; return HAL_ERROR_NOT_SUPPORTED; }
    #define BSP_SERVO1_SET_ANGLE bsp_esp32_s3_devkit_lite_servo1_set_angle
    #define BSP_SERVO2_SET_ANGLE bsp_esp32_s3_devkit_lite_servo2_set_angle
#else
    #include "../boards/esp32-s3-devkit/bsp_esp32_s3_devkit.h"
    #define BSP_LED1_CONTROL bsp_esp32_s3_devkit_led1_control
    #define BSP_LED2_CONTROL bsp_esp32_s3_devkit_led2_control
    #define BSP_LED3_CONTROL bsp_esp32_s3_devkit_led3_control
    #define BSP_LED4_CONTROL bsp_esp32_s3_devkit_led4_control
    #define BSP_LED1_SET_BRIGHTNESS bsp_esp32_s3_devkit_led1_set_brightness
    #define BSP_LED2_SET_BRIGHTNESS bsp_esp32_s3_devkit_led2_set_brightness
    #define BSP_LED3_SET_BRIGHTNESS bsp_esp32_s3_devkit_led3_set_brightness
    #define BSP_LED4_SET_BRIGHTNESS bsp_esp32_s3_devkit_led4_set_brightness
    #define BSP_RELAY1_CONTROL bsp_esp32_s3_devkit_relay1_control
    #define BSP_RELAY2_CONTROL bsp_esp32_s3_devkit_relay2_control
    #define BSP_SERVO1_SET_ANGLE bsp_esp32_s3_devkit_servo1_set_angle
    #define BSP_SERVO2_SET_ANGLE bsp_esp32_s3_devkit_servo2_set_angle
#endif
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "DEVICE_CONTROL";
static bool s_initialized = false;

/**
 * @brief 初始化设备控制模块
 */
esp_err_t device_control_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Device control module already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing device control module...");
    
    // 检查BSP接口是否可用
    const bsp_interface_t* bsp = bsp_get_interface();
    if (!bsp) {
        ESP_LOGE(TAG, "BSP interface not available");
        return ESP_ERR_INVALID_STATE;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "✅ Device control module initialized successfully");
    return ESP_OK;
}

/**
 * @brief 解析JSON控制命令
 */
esp_err_t device_control_parse_json_command(const char *json_str, device_control_command_t *command)
{
    if (!json_str || !command) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(command, 0, sizeof(device_control_command_t));

    // 解析JSON
    cJSON *json = cJSON_Parse(json_str);
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse JSON: %s", cJSON_GetErrorPtr());
        return ESP_FAIL;
    }

    // 解析cmd字段
    cJSON *cmd_item = cJSON_GetObjectItem(json, "cmd");
    if (!cmd_item || !cJSON_IsString(cmd_item)) {
        ESP_LOGE(TAG, "Missing or invalid 'cmd' field");
        cJSON_Delete(json);
        return ESP_ERR_INVALID_ARG;
    }

    const char *cmd_str = cmd_item->valuestring;
    if (strcmp(cmd_str, "led") == 0) {
        command->cmd_type = DEVICE_CONTROL_CMD_LED;
        
        // 解析device_id（用于区分LED1、LED2等）
        cJSON *device_id_item = cJSON_GetObjectItem(json, "device_id");
        if (!device_id_item || !cJSON_IsNumber(device_id_item)) {
            ESP_LOGE(TAG, "Missing or invalid 'device_id' field");
            cJSON_Delete(json);
            return ESP_ERR_INVALID_ARG;
        }
        command->device_id = (uint8_t)cJSON_GetNumberValue(device_id_item);

        // 解析action
        cJSON *action_item = cJSON_GetObjectItem(json, "action");
        if (!action_item || !cJSON_IsString(action_item)) {
            ESP_LOGE(TAG, "Missing or invalid 'action' field");
            cJSON_Delete(json);
            return ESP_ERR_INVALID_ARG;
        }

        const char *action_str = action_item->valuestring;
        if (strcmp(action_str, "on") == 0) {
            command->action = DEVICE_CONTROL_ACTION_ON;
            command->value.state = true;
        } else if (strcmp(action_str, "off") == 0) {
            command->action = DEVICE_CONTROL_ACTION_OFF;
            command->value.state = false;
        } else if (strcmp(action_str, "brightness") == 0) {
            command->action = DEVICE_CONTROL_ACTION_BRIGHTNESS;
            cJSON *brightness_item = cJSON_GetObjectItem(json, "brightness");
            if (!brightness_item || !cJSON_IsNumber(brightness_item)) {
                ESP_LOGE(TAG, "Missing or invalid 'brightness' field");
                cJSON_Delete(json);
                return ESP_ERR_INVALID_ARG;
            }
            command->value.brightness = (uint8_t)cJSON_GetNumberValue(brightness_item);
        } else {
            ESP_LOGE(TAG, "Unknown LED action: %s", action_str);
            command->action = DEVICE_CONTROL_ACTION_UNKNOWN;
            cJSON_Delete(json);
            return ESP_ERR_INVALID_ARG;
        }

    } else if (strcmp(cmd_str, "relay") == 0) {
        command->cmd_type = DEVICE_CONTROL_CMD_RELAY;
        
        // 解析device_id（用于区分Relay1、Relay2等）
        cJSON *device_id_item = cJSON_GetObjectItem(json, "device_id");
        if (!device_id_item || !cJSON_IsNumber(device_id_item)) {
            ESP_LOGE(TAG, "Missing or invalid 'device_id' field");
            cJSON_Delete(json);
            return ESP_ERR_INVALID_ARG;
        }
        command->device_id = (uint8_t)cJSON_GetNumberValue(device_id_item);

        // 解析action
        cJSON *action_item = cJSON_GetObjectItem(json, "action");
        if (!action_item || !cJSON_IsString(action_item)) {
            ESP_LOGE(TAG, "Missing or invalid 'action' field");
            cJSON_Delete(json);
            return ESP_ERR_INVALID_ARG;
        }

        const char *action_str = action_item->valuestring;
        if (strcmp(action_str, "on") == 0) {
            command->action = DEVICE_CONTROL_ACTION_ON;
            command->value.state = true;
        } else if (strcmp(action_str, "off") == 0) {
            command->action = DEVICE_CONTROL_ACTION_OFF;
            command->value.state = false;
        } else {
            ESP_LOGE(TAG, "Unknown relay action: %s", action_str);
            command->action = DEVICE_CONTROL_ACTION_UNKNOWN;
            cJSON_Delete(json);
            return ESP_ERR_INVALID_ARG;
        }

    } else if (strcmp(cmd_str, "servo") == 0) {
        command->cmd_type = DEVICE_CONTROL_CMD_SERVO;
        
        // 解析device_id（用于区分Servo1、Servo2等）
        cJSON *device_id_item = cJSON_GetObjectItem(json, "device_id");
        if (!device_id_item || !cJSON_IsNumber(device_id_item)) {
            ESP_LOGE(TAG, "Missing or invalid 'device_id' field");
            cJSON_Delete(json);
            return ESP_ERR_INVALID_ARG;
        }
        command->device_id = (uint8_t)cJSON_GetNumberValue(device_id_item);

        // 解析angle
        cJSON *angle_item = cJSON_GetObjectItem(json, "angle");
        if (!angle_item || !cJSON_IsNumber(angle_item)) {
            ESP_LOGE(TAG, "Missing or invalid 'angle' field");
            cJSON_Delete(json);
            return ESP_ERR_INVALID_ARG;
        }
        command->action = DEVICE_CONTROL_ACTION_ANGLE;
        command->value.angle = (uint16_t)cJSON_GetNumberValue(angle_item);
        if (command->value.angle > 180) {
            ESP_LOGW(TAG, "Servo angle out of range, clamping to 180");
            command->value.angle = 180;
        }

    } else if (strcmp(cmd_str, "pwm") == 0) {
        command->cmd_type = DEVICE_CONTROL_CMD_PWM;
        
        // 解析channel（PWM通道，支持1=M1, 2=M2）
        cJSON *channel_item = cJSON_GetObjectItem(json, "channel");
        if (!channel_item || !cJSON_IsNumber(channel_item)) {
            ESP_LOGE(TAG, "Missing or invalid 'channel' field");
            cJSON_Delete(json);
            return ESP_ERR_INVALID_ARG;
        }
        uint8_t channel = (uint8_t)cJSON_GetNumberValue(channel_item);
        if (channel != 1 && channel != 2) {
            ESP_LOGE(TAG, "Invalid PWM channel: %d (supported: 1=M1, 2=M2)", channel);
            cJSON_Delete(json);
            return ESP_ERR_INVALID_ARG;
        }
        command->device_id = channel;

        // 解析frequency
        cJSON *freq_item = cJSON_GetObjectItem(json, "frequency");
        if (!freq_item || !cJSON_IsNumber(freq_item)) {
            ESP_LOGE(TAG, "Missing or invalid 'frequency' field");
            cJSON_Delete(json);
            return ESP_ERR_INVALID_ARG;
        }
        command->value.pwm.frequency = (uint32_t)cJSON_GetNumberValue(freq_item);

        // 解析duty_cycle
        cJSON *duty_item = cJSON_GetObjectItem(json, "duty_cycle");
        if (!duty_item || !cJSON_IsNumber(duty_item)) {
            ESP_LOGE(TAG, "Missing or invalid 'duty_cycle' field");
            cJSON_Delete(json);
            return ESP_ERR_INVALID_ARG;
        }
        command->value.pwm.duty_cycle = (float)cJSON_GetNumberValue(duty_item);
        
        // 参数验证
        if (command->value.pwm.frequency < 1 || command->value.pwm.frequency > 40000) {
            ESP_LOGE(TAG, "PWM frequency out of range: %lu (must be 1-40000)", command->value.pwm.frequency);
            cJSON_Delete(json);
            return ESP_ERR_INVALID_ARG;
        }
        if (command->value.pwm.duty_cycle < 0.0 || command->value.pwm.duty_cycle > 100.0) {
            ESP_LOGE(TAG, "PWM duty_cycle out of range: %.2f (must be 0-100)", command->value.pwm.duty_cycle);
            cJSON_Delete(json);
            return ESP_ERR_INVALID_ARG;
        }
        
        command->action = DEVICE_CONTROL_ACTION_UNKNOWN;  // PWM不需要action字段

    } else {
        ESP_LOGE(TAG, "Unknown command type: %s", cmd_str);
        command->cmd_type = DEVICE_CONTROL_CMD_UNKNOWN;
        cJSON_Delete(json);
        return ESP_ERR_NOT_FOUND;
    }

    cJSON_Delete(json);
    return ESP_OK;
}

/**
 * @brief 执行设备控制命令
 */
esp_err_t device_control_execute(const device_control_command_t *command, device_control_result_t *result)
{
    if (!command || !result) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized) {
        result->success = false;
        result->error_msg = "Device control module not initialized";
        return ESP_ERR_INVALID_STATE;
    }

    memset(result, 0, sizeof(device_control_result_t));

    esp_err_t ret = ESP_OK;

    switch (command->cmd_type) {
        case DEVICE_CONTROL_CMD_LED:
            if (command->action == DEVICE_CONTROL_ACTION_BRIGHTNESS) {
                ret = device_control_led_brightness(command->device_id, command->value.brightness);
            } else {
                ret = device_control_led(command->device_id, command->value.state);
            }
            break;

        case DEVICE_CONTROL_CMD_RELAY:
            ret = device_control_relay(command->device_id, command->value.state);
            break;

        case DEVICE_CONTROL_CMD_SERVO:
            ret = device_control_servo(command->device_id, command->value.angle);
            break;

        case DEVICE_CONTROL_CMD_PWM:
            ret = device_control_pwm(command->device_id, 
                                    command->value.pwm.frequency, 
                                    command->value.pwm.duty_cycle);
            break;

        default:
            result->success = false;
            result->error_msg = "Unknown command type";
            return ESP_ERR_INVALID_ARG;
    }

    if (ret == ESP_OK) {
        result->success = true;
        ESP_LOGI(TAG, "✅ Device control command executed successfully");
    } else {
        result->success = false;
        result->error_msg = esp_err_to_name(ret);
        ESP_LOGE(TAG, "❌ Device control command failed: %s", result->error_msg);
    }

    return ret;
}

/**
 * @brief 控制LED
 */
esp_err_t device_control_led(uint8_t led_id, bool state)
{
    if (led_id < 1 || led_id > 4) {
        ESP_LOGE(TAG, "Invalid LED ID: %d (supported: 1-4)", led_id);
        return ESP_ERR_INVALID_ARG;
    }

    hal_err_t ret = HAL_ERROR;
    switch (led_id) {
        case 1:
            ret = BSP_LED1_CONTROL(state);
            break;
        case 2:
            ret = BSP_LED2_CONTROL(state);
            break;
        case 3:
            ret = BSP_LED3_CONTROL(state);
            break;
        case 4:
            ret = BSP_LED4_CONTROL(state);
            break;
    }

    if (ret == HAL_OK) {
        ESP_LOGI(TAG, "LED%d %s", led_id, state ? "ON" : "OFF");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "LED%d control failed", led_id);
        return ESP_FAIL;
    }
}

/**
 * @brief 设置LED亮度
 */
esp_err_t device_control_led_brightness(uint8_t led_id, uint8_t brightness)
{
    if (led_id < 1 || led_id > 4) {
        ESP_LOGE(TAG, "Invalid LED ID: %d (supported: 1-4)", led_id);
        return ESP_ERR_INVALID_ARG;
    }

    hal_err_t ret = HAL_ERROR;
    switch (led_id) {
        case 1:
            ret = BSP_LED1_SET_BRIGHTNESS(brightness);
            break;
        case 2:
            ret = BSP_LED2_SET_BRIGHTNESS(brightness);
            break;
        case 3:
            ret = BSP_LED3_SET_BRIGHTNESS(brightness);
            break;
        case 4:
            ret = BSP_LED4_SET_BRIGHTNESS(brightness);
            break;
    }

    if (ret == HAL_OK) {
        ESP_LOGI(TAG, "LED%d brightness set to %d", led_id, brightness);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "LED%d brightness set failed", led_id);
        return ESP_FAIL;
    }
}

/**
 * @brief 控制继电器
 */
esp_err_t device_control_relay(uint8_t relay_id, bool state)
{
    if (relay_id < 1 || relay_id > 2) {
        ESP_LOGE(TAG, "Invalid relay ID: %d (supported: 1-2)", relay_id);
        return ESP_ERR_INVALID_ARG;
    }

    hal_err_t ret = HAL_ERROR;
    switch (relay_id) {
        case 1:
            ret = BSP_RELAY1_CONTROL(state);
            break;
        case 2:
            ret = BSP_RELAY2_CONTROL(state);
            break;
    }

    if (ret == HAL_OK) {
        ESP_LOGI(TAG, "Relay%d %s", relay_id, state ? "ON" : "OFF");
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Relay%d control failed", relay_id);
        return ESP_FAIL;
    }
}

/**
 * @brief 控制舵机
 */
esp_err_t device_control_servo(uint8_t servo_id, uint16_t angle)
{
    if (servo_id < 1 || servo_id > 2) {
        ESP_LOGE(TAG, "Invalid servo ID: %d (supported: 1-2)", servo_id);
        return ESP_ERR_INVALID_ARG;
    }

    if (angle > 180) {
        ESP_LOGW(TAG, "Servo angle out of range, clamping to 180");
        angle = 180;
    }

    hal_err_t ret = HAL_ERROR;
    switch (servo_id) {
        case 1:
            ret = BSP_SERVO1_SET_ANGLE(angle);
            break;
        case 2:
            ret = BSP_SERVO2_SET_ANGLE(angle);
            break;
    }

    if (ret == HAL_OK) {
        ESP_LOGI(TAG, "Servo%d angle set to %d degrees", servo_id, angle);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "Servo%d control failed", servo_id);
        return ESP_FAIL;
    }
}

/**
 * @brief 控制PWM输出
 */
esp_err_t device_control_pwm(uint8_t channel, uint32_t frequency, float duty_cycle)
{
    ESP_LOGI(TAG, "Setting PWM channel %d: freq=%lu Hz, duty=%.2f%%", 
             channel, frequency, duty_cycle);
    
    esp_err_t ret = pwm_control_set(channel, frequency, duty_cycle);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ PWM channel %d configured successfully", channel);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "❌ PWM channel %d configuration failed: %s", 
                 channel, esp_err_to_name(ret));
        return ret;
    }
}

