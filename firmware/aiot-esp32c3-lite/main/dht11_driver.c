/**
 * @file dht11_driver.c
 * @brief DHT11温湿度传感器驱动实现
 * 
 * DHT11单总线协议
 * 
 * @author AIOT Team
 * @date 2025-12-27
 */

#include "dht11_driver.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "rom/ets_sys.h"  // 使用 ets_delay_us

// 二进制打印宏（用于调试）
#define BYTE_TO_BINARY_PATTERN "%c%c%c%c%c%c%c%c"
#define BYTE_TO_BINARY(byte)  \
  ((byte) & 0x80 ? '1' : '0'), \
  ((byte) & 0x40 ? '1' : '0'), \
  ((byte) & 0x20 ? '1' : '0'), \
  ((byte) & 0x10 ? '1' : '0'), \
  ((byte) & 0x08 ? '1' : '0'), \
  ((byte) & 0x04 ? '1' : '0'), \
  ((byte) & 0x02 ? '1' : '0'), \
  ((byte) & 0x01 ? '1' : '0')

static const char *TAG = "DHT11";

static gpio_num_t dht11_gpio = DHT11_GPIO_PIN;
static bool dht11_initialized = false;
static portMUX_TYPE dht11_spinlock = portMUX_INITIALIZER_UNLOCKED;  // 保护读数据阶段的短临界区

// 微秒级延时 - 使用 ets_delay_us 而不是 esp_rom_delay_us
// ets_delay_us 基于CPU周期，不受APB时钟影响
static inline void delay_us(uint32_t us) {
    ets_delay_us(us);
}

// 设置GPIO为输出模式
__attribute__((unused))
static void dht11_gpio_output(void) {
    gpio_set_direction(dht11_gpio, GPIO_MODE_OUTPUT);
}

// 设置GPIO为输入模式
__attribute__((unused))
static void dht11_gpio_input(void) {
    gpio_set_direction(dht11_gpio, GPIO_MODE_INPUT);
}

// 写GPIO电平
__attribute__((unused))
static void dht11_gpio_write(uint8_t level) {
    gpio_set_level(dht11_gpio, level);
}

// 读GPIO电平
static uint8_t dht11_gpio_read(void) {
    return gpio_get_level(dht11_gpio);
}

// DHT11初始化
esp_err_t dht11_init(gpio_num_t gpio_num) {
    dht11_gpio = gpio_num;
    
    // 先复位GPIO（确保之前的配置被清除）
    gpio_reset_pin(dht11_gpio);
    
    // 配置GPIO为输入输出模式，启用上拉
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,  // 开漏模式（参考aiot-esp32）
        .pin_bit_mask = (1ULL << dht11_gpio),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,  // 启用内部上拉
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO配置失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 初始状态设为输出高电平
    gpio_set_direction(dht11_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(dht11_gpio, 1);
    
    // 等待DHT11上电稳定（至少1秒）
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    dht11_initialized = true;
    ESP_LOGI(TAG, "✅ DHT11初始化成功 (GPIO%d，已启用内部上拉)", dht11_gpio);
    ESP_LOGI(TAG, "⚠️  如DHT11读取失败，请确认：");
    ESP_LOGI(TAG, "    1. DHT11接线正确（VCC-3.3V, GND-GND, DATA-GPIO%d）", dht11_gpio);
    ESP_LOGI(TAG, "    2. 建议添加4.7K-10K外部上拉电阻（DATA到VCC）");
    ESP_LOGI(TAG, "    3. DHT11读取间隔至少2秒");
    
    return ESP_OK;
}

// 等待电平变化（未使用，保留以便调试）
__attribute__((unused)) static bool dht11_wait_level(uint8_t level, uint32_t timeout_us) {
    uint32_t start = esp_timer_get_time();
    while (dht11_gpio_read() != level) {
        if (esp_timer_get_time() - start > timeout_us) {
            ESP_LOGD(TAG, "等待电平%d超时（%luus）", level, timeout_us);
            return false;  // 超时
        }
        delay_us(1);  // 短暂延时，避免CPU占用过高
    }
    return true;
}

// 读取一位数据（完全参考aiot-esp32实现）
static uint8_t dht11_read_bit(void) {
    uint8_t retry = 0;
    
    // 等待变为低电平
    while (dht11_gpio_read() && retry < 100) {
        retry++;
        ets_delay_us(1);
    }
    
    retry = 0;
    
    // 等待变高电平
    while (!dht11_gpio_read() && retry < 100) {
        retry++;
        ets_delay_us(1);
    }
    
    // 等待40us
    ets_delay_us(40);
    
    if (dht11_gpio_read()) {
        return 1;
    } else {
        return 0;
    }
}

// 读取一个字节（完全参考aiot-esp32实现）
static bool dht11_read_byte(uint8_t *byte) {
    uint8_t data = 0;
    
    for (int i = 0; i < 8; i++) {
        data <<= 1;
        data |= dht11_read_bit();
    }
    
    *byte = data;
    return true;
}

// 读取DHT11数据
esp_err_t dht11_read(dht11_data_t *data) {
    if (!dht11_initialized) {
        ESP_LOGE(TAG, "DHT11未初始化");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!data) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint8_t raw_data[5] = {0};
    
    // 1. 复位DHT11（完全参考aiot-esp32实现）
    gpio_set_level(dht11_gpio, 0);  // 拉低DQ
    vTaskDelay(pdMS_TO_TICKS(20));  // 拉低至少18ms（使用vTaskDelay，不禁用中断）
    gpio_set_level(dht11_gpio, 1);  // DQ=1
    ets_delay_us(30);               // 主机拉高20~40us
    gpio_set_direction(dht11_gpio, GPIO_MODE_INPUT);  // 释放总线给DHT11
    
    // 2. 检查DHT11响应（参考aiot-esp32实现）
    uint8_t retry = 0;
    int err_status = 0;   // 0: ok, 1: no response, 2: resp high timeout, 3: read byte fail
    int err_index = -1;   // which byte failed

    // 在响应+数据阶段短暂关中断，避免WiFi抢占影响采样时序（约4ms）
    portENTER_CRITICAL(&dht11_spinlock);

    // DHT11会拉低40~80us
    while (dht11_gpio_read() && retry < 100) {
        retry++;
        ets_delay_us(1);
    }
    
    if (retry >= 100) {
        err_status = 1;  // 无响应
        goto exit_critical;
    }
    
    retry = 0;
    // DHT11拉低后会再次拉高80us
    while (!dht11_gpio_read() && retry < 100) {
        retry++;
        ets_delay_us(1);
    }
    
    if (retry >= 100) {
        err_status = 2;  // 响应信号异常
        goto exit_critical;
    }
    
    // 3. 读取40位数据（5字节）
    for (int i = 0; i < 5; i++) {
        if (!dht11_read_byte(&raw_data[i])) {
            err_status = 3;
            err_index = i;
            goto exit_critical;
        }
    }

exit_critical:
    portEXIT_CRITICAL(&dht11_spinlock);

    if (err_status != 0) {
        if (err_status == 1) {
            ESP_LOGW(TAG, "❌ DHT11无响应");
        } else if (err_status == 2) {
            ESP_LOGW(TAG, "❌ DHT11响应信号异常");
        } else if (err_status == 3) {
            ESP_LOGW(TAG, "读取字节%d失败", err_index);
        }
        data->valid = false;
        goto cleanup;
    }
    
    // 5. 校验数据（容忍±1误差，因为时序可能有微小偏差）
    uint8_t checksum = (raw_data[0] + raw_data[1] + raw_data[2] + raw_data[3]) & 0xFF;
    uint8_t checksum_diff = (checksum > raw_data[4]) ? (checksum - raw_data[4]) : (raw_data[4] - checksum);
    
    if (checksum_diff > 1) {
        // 校验和差异超过1，数据不可信
        ESP_LOGW(TAG, "校验和错误: 计算=%02X, 接收=%02X, 差值=%d", checksum, raw_data[4], checksum_diff);
        ESP_LOGW(TAG, "原始数据: [0]=%02X [1]=%02X [2]=%02X [3]=%02X [4]=%02X", 
                 raw_data[0], raw_data[1], raw_data[2], raw_data[3], raw_data[4]);
        data->valid = false;
        goto cleanup;
    }
    
    if (checksum_diff == 1) {
        // 容忍±1误差，可能是时序边界导致
        ESP_LOGD(TAG, "校验和容忍±1误差: 计算=%02X, 接收=%02X", checksum, raw_data[4]);
    }
    
    // 6. 解析数据
    // DHT11: 湿度整数.湿度小数.温度整数.温度小数.校验和
    // 注意：DHT11 小数部分通常为 0，DHT22 才会有小数
    
    // 打印原始数据（用于调试）
    ESP_LOGI(TAG, "📊 原始数据: [0x%02X][0x%02X][0x%02X][0x%02X][0x%02X]", 
             raw_data[0], raw_data[1], raw_data[2], raw_data[3], raw_data[4]);
    ESP_LOGI(TAG, "📊 二进制数据:");
    for (int i = 0; i < 5; i++) {
        ESP_LOGI(TAG, "   [%d] = 0x%02X = %3d = " BYTE_TO_BINARY_PATTERN, 
                 i, raw_data[i], raw_data[i], BYTE_TO_BINARY(raw_data[i]));
    }
    
    // DHT11标准格式：整数部分 + 小数部分×0.1
    data->humidity = raw_data[0] + raw_data[1] * 0.1f;
    data->temperature = raw_data[2] + raw_data[3] * 0.1f;
    data->timestamp = esp_timer_get_time() / 1000;  // 毫秒
    
    ESP_LOGI(TAG, "📊 解析结果: 湿度=%.1f%%, 温度=%.1f°C", data->humidity, data->temperature);
    
    // 温度合理性检查（扩展范围：-20°C ~ 80°C）
    // 注意：DHT11 官方规格是 0-50°C，但实际可能测到更高温度（如受 PCB 发热影响）
    if (data->temperature < -20.0f || data->temperature > 80.0f) {
        ESP_LOGW(TAG, "❌ 温度超出物理范围: %.1f°C（原始: 0x%02X.0x%02X = %d.%d）", 
                 data->temperature, raw_data[2], raw_data[3], raw_data[2], raw_data[3]);
        ESP_LOGW(TAG, "⚠️ 传感器可能已损坏或数据读取错误");
        data->valid = false;
        goto cleanup;
    }
    
    // 温度异常警告（但不拒绝数据）
    if (data->temperature > 50.0f) {
        ESP_LOGW(TAG, "⚠️ 温度偏高(%.1f°C)，超出DHT11规格范围(0-50°C)", data->temperature);
        ESP_LOGW(TAG, "💡 可能原因：传感器受PCB发热、WiFi模块或其他热源影响");
    }
    
    // 湿度合理性检查（DHT11规格：20-90%）
    if (data->humidity < 5.0f || data->humidity > 95.0f) {
        ESP_LOGW(TAG, "❌ 湿度超出合理范围: %.1f%% （原始: 0x%02X.0x%02X = %d.%d）", 
                 data->humidity, raw_data[0], raw_data[1], raw_data[0], raw_data[1]);
        ESP_LOGW(TAG, "⚠️ 可能是 WiFi 干扰或传感器故障");
        data->valid = false;
        goto cleanup;
    }
    
    data->valid = true;
    ESP_LOGI(TAG, "✅ DHT11 读取成功: 温度=%.1f°C, 湿度=%.1f%%", 
             data->temperature, data->humidity);
    
cleanup:
    // 每次读取结束后，都把总线拉回输出高电平（为下一次起始信号做好准备）
    gpio_set_direction(dht11_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(dht11_gpio, 1);

    if (data->valid) {
        return ESP_OK;
    } else {
        return ESP_FAIL;
    }
}

// 读取温度
esp_err_t dht11_read_temperature(float *temperature) {
    dht11_data_t data;
    esp_err_t ret = dht11_read(&data);
    if (ret == ESP_OK && data.valid) {
        *temperature = data.temperature;
    }
    return ret;
}

// 读取湿度
esp_err_t dht11_read_humidity(float *humidity) {
    dht11_data_t data;
    esp_err_t ret = dht11_read(&data);
    if (ret == ESP_OK && data.valid) {
        *humidity = data.humidity;
    }
    return ret;
}

// 检查DHT11是否可用
bool dht11_is_available(void) {
    return dht11_initialized;
}

// GPIO电平测试
void dht11_gpio_test(void) {
    ESP_LOGI(TAG, "=== GPIO%d电平测试 ===", dht11_gpio);
    
    // 测试输出模式
    gpio_set_direction(dht11_gpio, GPIO_MODE_OUTPUT);
    
    ESP_LOGI(TAG, "1. 设置输出高电平...");
    gpio_set_level(dht11_gpio, 1);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    ESP_LOGI(TAG, "2. 设置输出低电平...");
    gpio_set_level(dht11_gpio, 0);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    ESP_LOGI(TAG, "3. 恢复输出高电平...");
    gpio_set_level(dht11_gpio, 1);
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // 测试输入模式
    ESP_LOGI(TAG, "4. 切换到输入模式，读取电平...");
    gpio_set_direction(dht11_gpio, GPIO_MODE_INPUT);
    vTaskDelay(pdMS_TO_TICKS(100));
    
    for (int i = 0; i < 5; i++) {
        uint8_t level = gpio_get_level(dht11_gpio);
        ESP_LOGI(TAG, "   读取 #%d: 电平=%d (有上拉应该为1)", i + 1, level);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    // 恢复输出模式
    gpio_set_direction(dht11_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(dht11_gpio, 1);
    
    ESP_LOGI(TAG, "=== GPIO测试完成 ===");
    ESP_LOGI(TAG, "如果输入模式读到的都是1，说明GPIO正常且有上拉");
    ESP_LOGI(TAG, "如果读到的是0或不稳定，说明缺少上拉电阻");
}

// 多次读取取平均值（减少误差）
esp_err_t dht11_read_average(dht11_data_t *data, int samples) {
    if (samples < 1 || samples > 10) {
        ESP_LOGE(TAG, "样本数量应在 1-10 之间");
        return ESP_ERR_INVALID_ARG;
    }
    
    float temp_sum = 0.0f;
    float humi_sum = 0.0f;
    int valid_count = 0;
    
    ESP_LOGI(TAG, "开始读取 %d 次样本，取平均值...", samples);
    
    for (int i = 0; i < samples; i++) {
        dht11_data_t sample;
        esp_err_t ret = dht11_read(&sample);
        
        if (ret == ESP_OK && sample.valid) {
            temp_sum += sample.temperature;
            humi_sum += sample.humidity;
            valid_count++;
            ESP_LOGI(TAG, "  样本 %d/%d: 温度=%.1f°C, 湿度=%.1f%% ✅", 
                     i + 1, samples, sample.temperature, sample.humidity);
        } else {
            ESP_LOGW(TAG, "  样本 %d/%d: 读取失败 ❌", i + 1, samples);
        }
        
        if (i < samples - 1) {
            vTaskDelay(pdMS_TO_TICKS(2000));  // 等待2秒再读下一个样本
        }
    }
    
    if (valid_count == 0) {
        ESP_LOGE(TAG, "所有样本读取失败");
        data->valid = false;
        return ESP_FAIL;
    }
    
    data->temperature = temp_sum / valid_count;
    data->humidity = humi_sum / valid_count;
    data->valid = true;
    data->timestamp = esp_timer_get_time() / 1000;
    
    ESP_LOGI(TAG, "📊 平均值（%d/%d 个有效样本）: 温度=%.1f°C, 湿度=%.1f%%", 
             valid_count, samples, data->temperature, data->humidity);
    
    return ESP_OK;
}

// WiFi初始化后重新配置GPIO（WiFi可能改变GPIO配置）
esp_err_t dht11_reinit_after_wifi(void) {
    if (!dht11_initialized) {
        ESP_LOGW(TAG, "DHT11未初始化，跳过重新配置");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "WiFi初始化后重新配置DHT11 GPIO...");
    
    // 重新配置GPIO（与初始化时相同）
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT_OUTPUT_OD,  // 开漏模式
        .pin_bit_mask = (1ULL << dht11_gpio),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "重新配置GPIO失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 恢复初始状态
    gpio_set_direction(dht11_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(dht11_gpio, 1);
    
    ESP_LOGI(TAG, "✅ DHT11 GPIO重新配置成功");
    return ESP_OK;
}

// DHT11测试
void dht11_test(void) {
    ESP_LOGI(TAG, "开始DHT11测试...");
    
    if (!dht11_initialized) {
        ESP_LOGE(TAG, "❌ DHT11未初始化");
        return;
    }
    
    // 先做GPIO测试
    dht11_gpio_test();
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // 再做DHT11读取测试
    for (int i = 0; i < 5; i++) {
        dht11_data_t data;
        esp_err_t ret = dht11_read(&data);
        
        if (ret == ESP_OK && data.valid) {
            ESP_LOGI(TAG, "测试 %d/5: 温度=%.1f°C, 湿度=%.1f%% ✅", 
                     i + 1, data.temperature, data.humidity);
        } else {
            ESP_LOGE(TAG, "测试 %d/5: 读取失败 ❌ (%s)", 
                     i + 1, esp_err_to_name(ret));
        }
        
        vTaskDelay(pdMS_TO_TICKS(2000));  // DHT11需要至少2秒间隔
    }
    
    ESP_LOGI(TAG, "✅ DHT11测试完成");
}

