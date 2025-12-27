/**
 * @file preset_control.c
 * @brief 预设控制模块实现
 * 
 * 按照FIRMWARE_MANUAL.md要求实现预设控制功能
 */

#include "preset_control.h"
#include "device_control.h"
#include "pwm_control.h"
#include "esp_log.h"
#include "cJSON.h"
#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif
#include <string.h>
#include <stdlib.h>

static const char *TAG = "PRESET_CONTROL";
static bool s_initialized = false;

/**
 * @brief 初始化预设控制模块
 */
esp_err_t preset_control_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "Preset control module already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing preset control module...");
    
    // 确保设备控制模块已初始化
    esp_err_t ret = device_control_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Device control module initialization failed");
        return ret;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "✅ Preset control module initialized successfully");
    return ESP_OK;
}

/**
 * @brief 解析预设控制命令（新格式）
 */
esp_err_t preset_control_parse_json_command(const char *json_str, preset_control_command_t *command)
{
    if (!json_str || !command) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(command, 0, sizeof(preset_control_command_t));

    // 解析JSON
    cJSON *json = cJSON_Parse(json_str);
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse JSON: %s", cJSON_GetErrorPtr());
        return ESP_FAIL;
    }

    // 检查cmd字段
    cJSON *cmd_item = cJSON_GetObjectItem(json, "cmd");
    if (!cmd_item || !cJSON_IsString(cmd_item)) {
        ESP_LOGE(TAG, "Missing or invalid 'cmd' field");
        cJSON_Delete(json);
        return ESP_ERR_INVALID_ARG;
    }

    const char *cmd_str = cmd_item->valuestring;
    if (strcmp(cmd_str, "preset") != 0) {
        ESP_LOGE(TAG, "Not a preset command: %s", cmd_str);
        cJSON_Delete(json);
        return ESP_ERR_NOT_FOUND;
    }

    // 解析device_type
    cJSON *device_type_item = cJSON_GetObjectItem(json, "device_type");
    if (!device_type_item || !cJSON_IsString(device_type_item)) {
        ESP_LOGE(TAG, "Missing or invalid 'device_type' field");
        cJSON_Delete(json);
        return ESP_ERR_INVALID_ARG;
    }

    const char *device_type_str = device_type_item->valuestring;
    if (strcmp(device_type_str, "led") == 0) {
        command->device_type = PRESET_DEVICE_TYPE_LED;
    } else if (strcmp(device_type_str, "servo") == 0) {
        command->device_type = PRESET_DEVICE_TYPE_SERVO;
    } else if (strcmp(device_type_str, "relay") == 0) {
        command->device_type = PRESET_DEVICE_TYPE_RELAY;
    } else if (strcmp(device_type_str, "pwm") == 0) {
        command->device_type = PRESET_DEVICE_TYPE_PWM;
    } else {
        ESP_LOGE(TAG, "Unknown device type: %s", device_type_str);
        command->device_type = PRESET_DEVICE_TYPE_UNKNOWN;
        cJSON_Delete(json);
        return ESP_ERR_INVALID_ARG;
    }

    // 解析preset_type
    cJSON *preset_type_item = cJSON_GetObjectItem(json, "preset_type");
    if (!preset_type_item || !cJSON_IsString(preset_type_item)) {
        ESP_LOGE(TAG, "Missing or invalid 'preset_type' field");
        cJSON_Delete(json);
        return ESP_ERR_INVALID_ARG;
    }
    strncpy(command->preset_type, preset_type_item->valuestring, sizeof(command->preset_type) - 1);
    command->preset_type[sizeof(command->preset_type) - 1] = '\0';

    // 解析device_id（可选，默认为0表示所有设备）
    cJSON *device_id_item = cJSON_GetObjectItem(json, "device_id");
    if (device_id_item && cJSON_IsNumber(device_id_item)) {
        command->device_id = (uint8_t)cJSON_GetNumberValue(device_id_item);
    } else {
        command->device_id = 0;  // 默认所有设备
    }

    // 解析parameters（可选）
    cJSON *parameters_item = cJSON_GetObjectItem(json, "parameters");
    if (parameters_item && cJSON_IsObject(parameters_item)) {
        // 复制parameters对象（需要手动释放）
        command->parameters = cJSON_Duplicate(parameters_item, 1);
        if (!command->parameters) {
            ESP_LOGE(TAG, "Failed to duplicate parameters");
            cJSON_Delete(json);
            return ESP_ERR_NO_MEM;
        }
    } else {
        command->parameters = NULL;
    }

    cJSON_Delete(json);
    return ESP_OK;
}

/**
 * @brief 执行预设控制命令
 */
esp_err_t preset_control_execute(const preset_control_command_t *command, preset_control_result_t *result)
{
    if (!command || !result) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_initialized) {
        result->success = false;
        result->error_msg = "Preset control module not initialized";
        return ESP_ERR_INVALID_STATE;
    }

    memset(result, 0, sizeof(preset_control_result_t));

    ESP_LOGI(TAG, "Executing preset command: device_type=%d, preset_type=%s, device_id=%d",
             command->device_type, command->preset_type, command->device_id);

    // 根据预设类型执行不同的预设动作
    if (strcmp(command->preset_type, "blink") == 0) {
        // 闪烁预设
        if (command->device_type == PRESET_DEVICE_TYPE_LED) {
            // 获取参数（支持count/on_time/off_time，兼容times/interval_ms）
            int count = 3;
            int on_time_ms = 500;
            int off_time_ms = 500;
            if (command->parameters) {
                cJSON *count_item = cJSON_GetObjectItem(command->parameters, "count");
                if (count_item && cJSON_IsNumber(count_item)) {
                    count = (int)cJSON_GetNumberValue(count_item);
                } else {
                    // 兼容旧参数名times
                cJSON *times_item = cJSON_GetObjectItem(command->parameters, "times");
                if (times_item && cJSON_IsNumber(times_item)) {
                        count = (int)cJSON_GetNumberValue(times_item);
                    }
                }
                cJSON *on_time_item = cJSON_GetObjectItem(command->parameters, "on_time");
                if (on_time_item && cJSON_IsNumber(on_time_item)) {
                    on_time_ms = (int)cJSON_GetNumberValue(on_time_item);
                }
                cJSON *off_time_item = cJSON_GetObjectItem(command->parameters, "off_time");
                if (off_time_item && cJSON_IsNumber(off_time_item)) {
                    off_time_ms = (int)cJSON_GetNumberValue(off_time_item);
                } else {
                    // 兼容旧参数名interval_ms
                cJSON *interval_item = cJSON_GetObjectItem(command->parameters, "interval_ms");
                if (interval_item && cJSON_IsNumber(interval_item)) {
                        int interval_ms = (int)cJSON_GetNumberValue(interval_item);
                        on_time_ms = interval_ms / 2;
                        off_time_ms = interval_ms / 2;
                    }
                }
            }

            // 执行闪烁动作
            uint8_t start_id = command->device_id > 0 ? command->device_id : 1;
            uint8_t end_id = command->device_id > 0 ? command->device_id : 4;
            
            for (int i = 0; i < count; i++) {
                // 打开LED
                for (uint8_t id = start_id; id <= end_id; id++) {
                    device_control_led(id, true);
                }
                vTaskDelay(pdMS_TO_TICKS(on_time_ms));
                
                // 关闭LED
                for (uint8_t id = start_id; id <= end_id; id++) {
                    device_control_led(id, false);
                }
                vTaskDelay(pdMS_TO_TICKS(off_time_ms));
            }
            
            result->success = true;
            ESP_LOGI(TAG, "✅ LED blink preset executed: count=%d, on_time=%dms, off_time=%dms", count, on_time_ms, off_time_ms);
            return ESP_OK;
        }
    } else if (strcmp(command->preset_type, "wave") == 0) {
        // 波浪灯预设（LED依次点亮/熄灭，支持自定义序列、循环和方向）
        if (command->device_type == PRESET_DEVICE_TYPE_LED) {
            int interval_ms = 200;
            int cycles = 1;  // 默认循环1次
            bool reverse = false;  // 默认正向
            uint8_t led_sequence[10] = {0};  // 自定义LED序列，最多10个
            int sequence_len = 0;
            
            if (command->parameters) {
                cJSON *interval_item = cJSON_GetObjectItem(command->parameters, "interval_ms");
                if (interval_item && cJSON_IsNumber(interval_item)) {
                    interval_ms = (int)cJSON_GetNumberValue(interval_item);
                }
                cJSON *cycles_item = cJSON_GetObjectItem(command->parameters, "cycles");
                if (cycles_item && cJSON_IsNumber(cycles_item)) {
                    cycles = (int)cJSON_GetNumberValue(cycles_item);
                }
                cJSON *reverse_item = cJSON_GetObjectItem(command->parameters, "reverse");
                if (reverse_item && cJSON_IsBool(reverse_item)) {
                    reverse = cJSON_IsTrue(reverse_item);
                }
                
                // 解析自定义LED序列（新增）
                cJSON *sequence_item = cJSON_GetObjectItem(command->parameters, "led_sequence");
                if (sequence_item && cJSON_IsArray(sequence_item)) {
                    int array_size = cJSON_GetArraySize(sequence_item);
                    sequence_len = array_size < 10 ? array_size : 10;  // 最多10个
                    for (int i = 0; i < sequence_len; i++) {
                        cJSON *led_id_item = cJSON_GetArrayItem(sequence_item, i);
                        if (led_id_item && cJSON_IsNumber(led_id_item)) {
                            led_sequence[i] = (uint8_t)cJSON_GetNumberValue(led_id_item);
                        }
                    }
                    ESP_LOGI(TAG, "📋 使用自定义LED序列，长度: %d", sequence_len);
                }
            }

            // 如果没有提供自定义序列，使用默认序列
            if (sequence_len == 0) {
            uint8_t start_id = command->device_id > 0 ? command->device_id : 1;
            uint8_t end_id = command->device_id > 0 ? command->device_id : 4;
                sequence_len = end_id - start_id + 1;
                for (int i = 0; i < sequence_len; i++) {
                    led_sequence[i] = start_id + i;
                }
                ESP_LOGI(TAG, "📋 使用默认LED序列: %d-%d", start_id, end_id);
            }
            
            // 先关闭所有可能用到的LED
            for (int i = 0; i < sequence_len; i++) {
                device_control_led(led_sequence[i], false);
            }
            
            // 执行波浪效果（支持循环和自定义序列）
            for (int cycle = 0; cycle < cycles; cycle++) {
                if (reverse) {
                    // 反向：从序列末尾到开头
                    for (int i = sequence_len - 1; i >= 0; i--) {
                        device_control_led(led_sequence[i], true);
                vTaskDelay(pdMS_TO_TICKS(interval_ms));
                        device_control_led(led_sequence[i], false);
                    }
                } else {
                    // 正向：从序列开头到末尾
                    for (int i = 0; i < sequence_len; i++) {
                        device_control_led(led_sequence[i], true);
                vTaskDelay(pdMS_TO_TICKS(interval_ms));
                        device_control_led(led_sequence[i], false);
                    }
                }
            }
            
            result->success = true;
            ESP_LOGI(TAG, "✅ LED wave preset executed: sequence_len=%d, interval=%dms, cycles=%d, reverse=%s", 
                     sequence_len, interval_ms, cycles, reverse ? "true" : "false");
            return ESP_OK;
        }
    } else if (strcmp(command->preset_type, "sequence") == 0) {
        // 序列预设（按顺序执行多个动作）
        if (command->parameters) {
            cJSON *actions_item = cJSON_GetObjectItem(command->parameters, "actions");
            if (actions_item && cJSON_IsArray(actions_item)) {
                int array_size = cJSON_GetArraySize(actions_item);
                for (int i = 0; i < array_size; i++) {
                    cJSON *action_item = cJSON_GetArrayItem(actions_item, i);
                    if (action_item && cJSON_IsObject(action_item)) {
                        // 解析单个动作并执行
                        char *action_json = cJSON_Print(action_item);
                        if (action_json) {
                            device_control_command_t device_cmd;
                            if (device_control_parse_json_command(action_json, &device_cmd) == ESP_OK) {
                                device_control_result_t device_result;
                                device_control_execute(&device_cmd, &device_result);
                            }
                            free(action_json);
                        }
                        
                        // 等待间隔
                        int delay_ms = 100;
                        cJSON *delay_item = cJSON_GetObjectItem(action_item, "delay_ms");
                        if (delay_item && cJSON_IsNumber(delay_item)) {
                            delay_ms = (int)cJSON_GetNumberValue(delay_item);
                        }
                        vTaskDelay(pdMS_TO_TICKS(delay_ms));
                    }
                }
                
                result->success = true;
                ESP_LOGI(TAG, "✅ Sequence preset executed: %d actions", array_size);
                return ESP_OK;
            }
        }
    } else if (strcmp(command->preset_type, "swing") == 0) {
        // 摆动预设（用于普通180度舵机，如机器狗尾巴）
        if (command->device_type == PRESET_DEVICE_TYPE_SERVO) {
            // 获取参数
            int center_angle = 90;  // 默认中心角度90度
            int swing_angle = 30;   // 默认摆动幅度±30度
            int speed_ms = 500;     // 默认摆动速度500ms
            int cycles = 3;         // 默认摆动3次
            
            if (command->parameters) {
                cJSON *center_item = cJSON_GetObjectItem(command->parameters, "center_angle");
                if (center_item && cJSON_IsNumber(center_item)) {
                    center_angle = (int)cJSON_GetNumberValue(center_item);
                }
                cJSON *swing_item = cJSON_GetObjectItem(command->parameters, "swing_angle");
                if (swing_item && cJSON_IsNumber(swing_item)) {
                    swing_angle = (int)cJSON_GetNumberValue(swing_item);
                }
                cJSON *speed_item = cJSON_GetObjectItem(command->parameters, "speed");
                if (speed_item && cJSON_IsNumber(speed_item)) {
                    speed_ms = (int)cJSON_GetNumberValue(speed_item);
                }
                cJSON *cycles_item = cJSON_GetObjectItem(command->parameters, "cycles");
                if (cycles_item && cJSON_IsNumber(cycles_item)) {
                    cycles = (int)cJSON_GetNumberValue(cycles_item);
                }
            }

            uint8_t servo_id = command->device_id > 0 ? command->device_id : 1;
            
            // 计算左右边界角度
            int left_angle = center_angle - swing_angle;
            int right_angle = center_angle + swing_angle;
            
            // 限制在0-180度范围内
            if (left_angle < 0) left_angle = 0;
            if (right_angle > 180) right_angle = 180;
            
            ESP_LOGI(TAG, "舵机%d 摆动预设: 中心=%d°, 幅度=±%d°, 速度=%dms, 次数=%d", 
                     servo_id, center_angle, swing_angle, speed_ms, cycles);
            
            // 先移动到中心位置
            device_control_servo(servo_id, center_angle);
            vTaskDelay(pdMS_TO_TICKS(300));
            
            // 执行摆动
            for (int cycle = 0; cycle < cycles; cycle++) {
                // 向左摆
                device_control_servo(servo_id, left_angle);
                vTaskDelay(pdMS_TO_TICKS(speed_ms));
                
                // 向右摆
                device_control_servo(servo_id, right_angle);
                vTaskDelay(pdMS_TO_TICKS(speed_ms));
            }
            
            // 回到中心位置
            device_control_servo(servo_id, center_angle);
            
            result->success = true;
            ESP_LOGI(TAG, "✅ Servo swing preset executed: servo_id=%d, center=%d°, swing=±%d°, speed=%dms, cycles=%d", 
                     servo_id, center_angle, swing_angle, speed_ms, cycles);
            return ESP_OK;
        }
    } else if (strcmp(command->preset_type, "rotate") == 0) {
        // 正反转预设（用于360度连续旋转舵机）
        if (command->device_type == PRESET_DEVICE_TYPE_SERVO) {
            // 获取参数
            int cycles = 3;  // 默认循环3次
            int forward_duration_ms = 3000;  // 默认正转3秒
            int reverse_duration_ms = 3000;  // 默认反转3秒
            int pause_time_ms = 500;  // 默认暂停500ms
            
            if (command->parameters) {
                cJSON *cycles_item = cJSON_GetObjectItem(command->parameters, "cycles");
                if (cycles_item && cJSON_IsNumber(cycles_item)) {
                    cycles = (int)cJSON_GetNumberValue(cycles_item);
                }
                cJSON *forward_duration_item = cJSON_GetObjectItem(command->parameters, "forward_duration");
                if (forward_duration_item && cJSON_IsNumber(forward_duration_item)) {
                    forward_duration_ms = (int)cJSON_GetNumberValue(forward_duration_item);
                }
                cJSON *reverse_duration_item = cJSON_GetObjectItem(command->parameters, "reverse_duration");
                if (reverse_duration_item && cJSON_IsNumber(reverse_duration_item)) {
                    reverse_duration_ms = (int)cJSON_GetNumberValue(reverse_duration_item);
                }
                cJSON *pause_time_item = cJSON_GetObjectItem(command->parameters, "pause_time");
                if (pause_time_item && cJSON_IsNumber(pause_time_item)) {
                    pause_time_ms = (int)cJSON_GetNumberValue(pause_time_item);
                }
            }

            uint8_t servo_id = command->device_id > 0 ? command->device_id : 1;
            
            // 360度舵机控制：0-89=反转，90=停止，91-180=正转
            // 使用中等速度（135度=正转，45度=反转）
            uint16_t forward_angle = 135;  // 正转角度（91-180之间）
            uint16_t reverse_angle = 45;   // 反转角度（0-89之间）
            uint16_t stop_angle = 90;      // 停止角度
            
            // 执行循环正反转
            for (int cycle = 0; cycle < cycles; cycle++) {
                // 正转
                ESP_LOGI(TAG, "舵机%d 正转 (%dms)", servo_id, forward_duration_ms);
                device_control_servo(servo_id, forward_angle);
                vTaskDelay(pdMS_TO_TICKS(forward_duration_ms));
                
                // 停止
                device_control_servo(servo_id, stop_angle);
                vTaskDelay(pdMS_TO_TICKS(pause_time_ms));
                
                // 反转
                ESP_LOGI(TAG, "舵机%d 反转 (%dms)", servo_id, reverse_duration_ms);
                device_control_servo(servo_id, reverse_angle);
                vTaskDelay(pdMS_TO_TICKS(reverse_duration_ms));
                
                // 停止
                device_control_servo(servo_id, stop_angle);
                vTaskDelay(pdMS_TO_TICKS(pause_time_ms));
            }
            
            result->success = true;
            ESP_LOGI(TAG, "✅ Servo rotate preset executed: servo_id=%d, cycles=%d, forward=%dms, reverse=%dms, pause=%dms", 
                     servo_id, cycles, forward_duration_ms, reverse_duration_ms, pause_time_ms);
            return ESP_OK;
        }
    } else if (strcmp(command->preset_type, "timed_switch") == 0) {
        // 定时开关预设（用于继电器）
        if (command->device_type == PRESET_DEVICE_TYPE_RELAY) {
            // 获取参数
            int duration_ms = 1000;  // 默认1秒
            bool initial_state = true;  // 默认先打开
            if (command->parameters) {
                cJSON *duration_item = cJSON_GetObjectItem(command->parameters, "duration");
                if (duration_item && cJSON_IsNumber(duration_item)) {
                    duration_ms = (int)cJSON_GetNumberValue(duration_item);
                }
                cJSON *state_item = cJSON_GetObjectItem(command->parameters, "initial_state");
                if (state_item && cJSON_IsBool(state_item)) {
                    initial_state = cJSON_IsTrue(state_item);
                }
            }

            uint8_t start_id = command->device_id > 0 ? command->device_id : 1;
            uint8_t end_id = command->device_id > 0 ? command->device_id : 2;
            
            // 打开继电器
            for (uint8_t id = start_id; id <= end_id; id++) {
                device_control_relay(id, initial_state);
            }
            
            // 等待指定时间
            vTaskDelay(pdMS_TO_TICKS(duration_ms));
            
            // 关闭继电器
            for (uint8_t id = start_id; id <= end_id; id++) {
                device_control_relay(id, !initial_state);
            }
            
            result->success = true;
            ESP_LOGI(TAG, "✅ Relay timed_switch preset executed: device_id=%d, duration=%dms, initial_state=%s", 
                     command->device_id, duration_ms, initial_state ? "ON" : "OFF");
            return ESP_OK;
        }
    } else if (strcmp(command->preset_type, "fade") == 0) {
        // PWM渐变预设
        if (command->device_type == PRESET_DEVICE_TYPE_PWM) {
            // 获取参数
            uint32_t frequency = 5000;
            float start_duty = 0.0;
            float end_duty = 100.0;
            int duration_ms = 2000;
            int step_interval_ms = 50;
            
            if (command->parameters) {
                cJSON *freq_item = cJSON_GetObjectItem(command->parameters, "frequency");
                if (freq_item && cJSON_IsNumber(freq_item)) {
                    frequency = (uint32_t)cJSON_GetNumberValue(freq_item);
                }
                cJSON *start_item = cJSON_GetObjectItem(command->parameters, "start_duty");
                if (start_item && cJSON_IsNumber(start_item)) {
                    start_duty = (float)cJSON_GetNumberValue(start_item);
                }
                cJSON *end_item = cJSON_GetObjectItem(command->parameters, "end_duty");
                if (end_item && cJSON_IsNumber(end_item)) {
                    end_duty = (float)cJSON_GetNumberValue(end_item);
                }
                cJSON *duration_item = cJSON_GetObjectItem(command->parameters, "duration");
                if (duration_item && cJSON_IsNumber(duration_item)) {
                    duration_ms = (int)cJSON_GetNumberValue(duration_item);
                }
                cJSON *step_item = cJSON_GetObjectItem(command->parameters, "step_interval");
                if (step_item && cJSON_IsNumber(step_item)) {
                    step_interval_ms = (int)cJSON_GetNumberValue(step_item);
                }
            }
            
            uint8_t channel = command->device_id > 0 ? command->device_id : 2;  // 默认通道2(M2)
            
            ESP_LOGI(TAG, "PWM渐变: 通道=%d, 频率=%lu Hz, %.1f%% -> %.1f%%, 时长=%dms",
                     channel, frequency, start_duty, end_duty, duration_ms);
            
            // 计算步数
            int steps = duration_ms / step_interval_ms;
            if (steps < 1) steps = 1;
            float duty_step = (end_duty - start_duty) / steps;
            
            // 执行渐变
            for (int i = 0; i <= steps; i++) {
                float current_duty = start_duty + (duty_step * i);
                pwm_control_set(channel, frequency, current_duty);
                if (i < steps) {
                    vTaskDelay(pdMS_TO_TICKS(step_interval_ms));
                }
            }
            
            result->success = true;
            ESP_LOGI(TAG, "✅ PWM fade preset executed: channel=%d, %.1f%% -> %.1f%%", 
                     channel, start_duty, end_duty);
            return ESP_OK;
        }
    } else if (strcmp(command->preset_type, "breathe") == 0) {
        // PWM呼吸灯预设
        if (command->device_type == PRESET_DEVICE_TYPE_PWM) {
            // 获取参数
            uint32_t frequency = 5000;
            float min_duty = 0.0;
            float max_duty = 100.0;
            int fade_in_time = 1500;
            int fade_out_time = 1500;
            int hold_time = 500;
            int cycles = 5;
            
            if (command->parameters) {
                cJSON *freq_item = cJSON_GetObjectItem(command->parameters, "frequency");
                if (freq_item && cJSON_IsNumber(freq_item)) {
                    frequency = (uint32_t)cJSON_GetNumberValue(freq_item);
                }
                cJSON *min_item = cJSON_GetObjectItem(command->parameters, "min_duty");
                if (min_item && cJSON_IsNumber(min_item)) {
                    min_duty = (float)cJSON_GetNumberValue(min_item);
                }
                cJSON *max_item = cJSON_GetObjectItem(command->parameters, "max_duty");
                if (max_item && cJSON_IsNumber(max_item)) {
                    max_duty = (float)cJSON_GetNumberValue(max_item);
                }
                cJSON *fade_in_item = cJSON_GetObjectItem(command->parameters, "fade_in_time");
                if (fade_in_item && cJSON_IsNumber(fade_in_item)) {
                    fade_in_time = (int)cJSON_GetNumberValue(fade_in_item);
                }
                cJSON *fade_out_item = cJSON_GetObjectItem(command->parameters, "fade_out_time");
                if (fade_out_item && cJSON_IsNumber(fade_out_item)) {
                    fade_out_time = (int)cJSON_GetNumberValue(fade_out_item);
                }
                cJSON *hold_item = cJSON_GetObjectItem(command->parameters, "hold_time");
                if (hold_item && cJSON_IsNumber(hold_item)) {
                    hold_time = (int)cJSON_GetNumberValue(hold_item);
                }
                cJSON *cycles_item = cJSON_GetObjectItem(command->parameters, "cycles");
                if (cycles_item && cJSON_IsNumber(cycles_item)) {
                    cycles = (int)cJSON_GetNumberValue(cycles_item);
                }
            }
            
            uint8_t channel = command->device_id > 0 ? command->device_id : 2;
            
            ESP_LOGI(TAG, "PWM呼吸灯: 通道=%d, %.1f%%-%.1f%%, 循环=%d次",
                     channel, min_duty, max_duty, cycles);
            
            const int step_ms = 50;  // 固定步进间隔50ms
            
            for (int cycle = 0; cycle < cycles; cycle++) {
                // 渐亮（从min到max）
                int fade_in_steps = fade_in_time / step_ms;
                float fade_in_step = (max_duty - min_duty) / fade_in_steps;
                for (int i = 0; i <= fade_in_steps; i++) {
                    float duty = min_duty + (fade_in_step * i);
                    pwm_control_set(channel, frequency, duty);
                    if (i < fade_in_steps) {
                        vTaskDelay(pdMS_TO_TICKS(step_ms));
                    }
                }
                
                // 保持最大亮度
                if (hold_time > 0) {
                    vTaskDelay(pdMS_TO_TICKS(hold_time));
                }
                
                // 渐暗（从max到min）
                int fade_out_steps = fade_out_time / step_ms;
                float fade_out_step = (max_duty - min_duty) / fade_out_steps;
                for (int i = 0; i <= fade_out_steps; i++) {
                    float duty = max_duty - (fade_out_step * i);
                    pwm_control_set(channel, frequency, duty);
                    if (i < fade_out_steps) {
                        vTaskDelay(pdMS_TO_TICKS(step_ms));
                    }
                }
                
                // 保持最小亮度
                if (hold_time > 0 && cycle < cycles - 1) {
                    vTaskDelay(pdMS_TO_TICKS(hold_time));
                }
            }
            
            result->success = true;
            ESP_LOGI(TAG, "✅ PWM breathe preset executed: channel=%d, %d cycles", channel, cycles);
            return ESP_OK;
        }
    } else if (strcmp(command->preset_type, "step") == 0) {
        // PWM步进预设
        if (command->device_type == PRESET_DEVICE_TYPE_PWM) {
            // 获取参数
            uint32_t frequency = 5000;
            float start_duty = 0.0;
            float end_duty = 100.0;
            float step_value = 10.0;
            int step_delay_ms = 300;
            
            if (command->parameters) {
                cJSON *freq_item = cJSON_GetObjectItem(command->parameters, "frequency");
                if (freq_item && cJSON_IsNumber(freq_item)) {
                    frequency = (uint32_t)cJSON_GetNumberValue(freq_item);
                }
                cJSON *start_item = cJSON_GetObjectItem(command->parameters, "start_duty");
                if (start_item && cJSON_IsNumber(start_item)) {
                    start_duty = (float)cJSON_GetNumberValue(start_item);
                }
                cJSON *end_item = cJSON_GetObjectItem(command->parameters, "end_duty");
                if (end_item && cJSON_IsNumber(end_item)) {
                    end_duty = (float)cJSON_GetNumberValue(end_item);
                }
                cJSON *step_item = cJSON_GetObjectItem(command->parameters, "step_value");
                if (step_item && cJSON_IsNumber(step_item)) {
                    step_value = (float)cJSON_GetNumberValue(step_item);
                }
                cJSON *delay_item = cJSON_GetObjectItem(command->parameters, "step_delay");
                if (delay_item && cJSON_IsNumber(delay_item)) {
                    step_delay_ms = (int)cJSON_GetNumberValue(delay_item);
                }
            }
            
            uint8_t channel = command->device_id > 0 ? command->device_id : 2;
            
            ESP_LOGI(TAG, "PWM步进: 通道=%d, %.1f%% -> %.1f%%, 步进值=%.1f%%",
                     channel, start_duty, end_duty, step_value);
            
            // 执行步进
            float current_duty = start_duty;
            bool increasing = end_duty > start_duty;
            
            while ((increasing && current_duty <= end_duty) || (!increasing && current_duty >= end_duty)) {
                pwm_control_set(channel, frequency, current_duty);
                vTaskDelay(pdMS_TO_TICKS(step_delay_ms));
                
                if (increasing) {
                    current_duty += step_value;
                    if (current_duty > end_duty) current_duty = end_duty;
                } else {
                    current_duty -= step_value;
                    if (current_duty < end_duty) current_duty = end_duty;
                }
            }
            
            // 确保达到最终值
            pwm_control_set(channel, frequency, end_duty);
            
            result->success = true;
            ESP_LOGI(TAG, "✅ PWM step preset executed: channel=%d", channel);
            return ESP_OK;
        }
    } else if (strcmp(command->preset_type, "pulse") == 0) {
        // PWM脉冲预设
        if (command->device_type == PRESET_DEVICE_TYPE_PWM) {
            // 获取参数
            uint32_t frequency = 5000;
            float duty_high = 80.0;
            float duty_low = 20.0;
            int high_time_ms = 500;
            int low_time_ms = 500;
            int cycles = 10;
            
            if (command->parameters) {
                cJSON *freq_item = cJSON_GetObjectItem(command->parameters, "frequency");
                if (freq_item && cJSON_IsNumber(freq_item)) {
                    frequency = (uint32_t)cJSON_GetNumberValue(freq_item);
                }
                cJSON *high_item = cJSON_GetObjectItem(command->parameters, "duty_high");
                if (high_item && cJSON_IsNumber(high_item)) {
                    duty_high = (float)cJSON_GetNumberValue(high_item);
                }
                cJSON *low_item = cJSON_GetObjectItem(command->parameters, "duty_low");
                if (low_item && cJSON_IsNumber(low_item)) {
                    duty_low = (float)cJSON_GetNumberValue(low_item);
                }
                cJSON *high_time_item = cJSON_GetObjectItem(command->parameters, "high_time");
                if (high_time_item && cJSON_IsNumber(high_time_item)) {
                    high_time_ms = (int)cJSON_GetNumberValue(high_time_item);
                }
                cJSON *low_time_item = cJSON_GetObjectItem(command->parameters, "low_time");
                if (low_time_item && cJSON_IsNumber(low_time_item)) {
                    low_time_ms = (int)cJSON_GetNumberValue(low_time_item);
                }
                cJSON *cycles_item = cJSON_GetObjectItem(command->parameters, "cycles");
                if (cycles_item && cJSON_IsNumber(cycles_item)) {
                    cycles = (int)cJSON_GetNumberValue(cycles_item);
                }
            }
            
            uint8_t channel = command->device_id > 0 ? command->device_id : 2;
            
            ESP_LOGI(TAG, "PWM脉冲: 通道=%d, %.1f%%<->%.1f%%, %d次",
                     channel, duty_low, duty_high, cycles);
            
            // 执行脉冲
            for (int i = 0; i < cycles; i++) {
                // 高电平
                pwm_control_set(channel, frequency, duty_high);
                vTaskDelay(pdMS_TO_TICKS(high_time_ms));
                
                // 低电平
                pwm_control_set(channel, frequency, duty_low);
                if (i < cycles - 1) {  // 最后一次不需要延迟
                    vTaskDelay(pdMS_TO_TICKS(low_time_ms));
                }
            }
            
            result->success = true;
            ESP_LOGI(TAG, "✅ PWM pulse preset executed: channel=%d, %d cycles", channel, cycles);
            return ESP_OK;
        }
    } else if (strcmp(command->preset_type, "fixed") == 0) {
        // PWM固定输出预设
        if (command->device_type == PRESET_DEVICE_TYPE_PWM) {
            // 获取参数
            uint32_t frequency = 5000;
            float duty_cycle = 50.0;
            int duration_ms = 0;  // 0表示持续输出
            
            if (command->parameters) {
                cJSON *freq_item = cJSON_GetObjectItem(command->parameters, "frequency");
                if (freq_item && cJSON_IsNumber(freq_item)) {
                    frequency = (uint32_t)cJSON_GetNumberValue(freq_item);
                }
                cJSON *duty_item = cJSON_GetObjectItem(command->parameters, "duty_cycle");
                if (duty_item && cJSON_IsNumber(duty_item)) {
                    duty_cycle = (float)cJSON_GetNumberValue(duty_item);
                }
                cJSON *duration_item = cJSON_GetObjectItem(command->parameters, "duration");
                if (duration_item && cJSON_IsNumber(duration_item)) {
                    duration_ms = (int)cJSON_GetNumberValue(duration_item);
                }
            }
            
            uint8_t channel = command->device_id > 0 ? command->device_id : 2;
            
            ESP_LOGI(TAG, "PWM固定输出: 通道=%d, 频率=%lu Hz, 占空比=%.1f%%",
                     channel, frequency, duty_cycle);
            
            // 设置PWM输出
            pwm_control_set(channel, frequency, duty_cycle);
            
            // 如果指定了持续时间
            if (duration_ms > 0) {
                vTaskDelay(pdMS_TO_TICKS(duration_ms));
                // 时间到后停止输出（设置为0%）
                pwm_control_set(channel, frequency, 0.0);
                ESP_LOGI(TAG, "PWM输出已停止（持续时间：%dms）", duration_ms);
            } else {
                ESP_LOGI(TAG, "PWM持续输出中（duration=0）");
            }
            
            result->success = true;
            ESP_LOGI(TAG, "✅ PWM fixed preset executed: channel=%d", channel);
            return ESP_OK;
        }
    } else {
        ESP_LOGE(TAG, "Unknown preset type: %s", command->preset_type);
        result->success = false;
        result->error_msg = "Unknown preset type";
        return ESP_ERR_INVALID_ARG;
    }

    result->success = false;
    result->error_msg = "Preset execution failed";
    return ESP_FAIL;
}

/**
 * @brief 释放预设命令资源
 */
void preset_control_free_command(preset_control_command_t *command)
{
    if (!command) {
        return;
    }

    if (command->parameters) {
        cJSON_Delete(command->parameters);
        command->parameters = NULL;
    }
}

