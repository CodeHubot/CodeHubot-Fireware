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

static const char *TAG = "DHT11";

static gpio_num_t dht11_gpio = DHT11_GPIO_PIN;
static bool dht11_initialized = false;

// 微秒级延时
static inline void delay_us(uint32_t us) {
    esp_rom_delay_us(us);
}

// 设置GPIO为输出模式
static void dht11_gpio_output(void) {
    gpio_set_direction(dht11_gpio, GPIO_MODE_OUTPUT);
}

// 设置GPIO为输入模式
static void dht11_gpio_input(void) {
    gpio_set_direction(dht11_gpio, GPIO_MODE_INPUT);
}

// 写GPIO电平
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
    
    // 配置GPIO
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT_OD,  // 开漏输出
        .pin_bit_mask = (1ULL << dht11_gpio),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,  // 使能上拉
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO配置失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 初始状态设为高电平
    dht11_gpio_write(1);
    
    dht11_initialized = true;
    ESP_LOGI(TAG, "✅ DHT11初始化成功 (GPIO%d)", dht11_gpio);
    
    return ESP_OK;
}

// 等待电平变化
static bool dht11_wait_level(uint8_t level, uint32_t timeout_us) {
    uint32_t start = esp_timer_get_time();
    while (dht11_gpio_read() != level) {
        if (esp_timer_get_time() - start > timeout_us) {
            return false;  // 超时
        }
    }
    return true;
}

// 读取一个字节
static bool dht11_read_byte(uint8_t *byte) {
    uint8_t data = 0;
    
    for (int i = 0; i < 8; i++) {
        // 等待低电平（准备发送位）
        if (!dht11_wait_level(0, 100)) {
            ESP_LOGD(TAG, "等待低电平超时 (bit %d)", i);
            return false;
        }
        
        // 等待高电平（开始发送位）
        if (!dht11_wait_level(1, 100)) {
            ESP_LOGD(TAG, "等待高电平超时 (bit %d)", i);
            return false;
        }
        
        // 延时30us后检测电平
        delay_us(30);
        
        data <<= 1;
        if (dht11_gpio_read()) {
            data |= 1;  // 高电平持续时间长，表示'1'
        }
        // 低电平持续时间短，表示'0'
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
    
    // 关键段，禁止任务切换
    portDISABLE_INTERRUPTS();
    
    // 1. 主机发送起始信号（低电平至少18ms）
    dht11_gpio_output();
    dht11_gpio_write(0);
    delay_us(20000);  // 20ms低电平
    
    // 2. 主机拉高并切换到输入模式
    dht11_gpio_write(1);
    dht11_gpio_input();
    delay_us(30);
    
    // 3. DHT11响应信号（80us低电平 + 80us高电平）
    if (!dht11_wait_level(0, 100)) {
        portENABLE_INTERRUPTS();
        ESP_LOGW(TAG, "DHT11无响应（等待低电平超时）");
        data->valid = false;
        return ESP_ERR_TIMEOUT;
    }
    
    if (!dht11_wait_level(1, 100)) {
        portENABLE_INTERRUPTS();
        ESP_LOGW(TAG, "DHT11响应信号异常（等待高电平超时）");
        data->valid = false;
        return ESP_ERR_TIMEOUT;
    }
    
    if (!dht11_wait_level(0, 100)) {
        portENABLE_INTERRUPTS();
        ESP_LOGW(TAG, "DHT11响应信号异常（等待低电平超时）");
        data->valid = false;
        return ESP_ERR_TIMEOUT;
    }
    
    // 4. 读取40位数据（5字节）
    bool read_ok = true;
    for (int i = 0; i < 5; i++) {
        if (!dht11_read_byte(&raw_data[i])) {
            ESP_LOGW(TAG, "读取字节%d失败", i);
            read_ok = false;
            break;
        }
    }
    
    // 恢复中断
    portENABLE_INTERRUPTS();
    
    // 恢复GPIO为输出高电平
    dht11_gpio_output();
    dht11_gpio_write(1);
    
    if (!read_ok) {
        data->valid = false;
        return ESP_ERR_INVALID_RESPONSE;
    }
    
    // 5. 校验数据
    uint8_t checksum = raw_data[0] + raw_data[1] + raw_data[2] + raw_data[3];
    if (checksum != raw_data[4]) {
        ESP_LOGW(TAG, "校验和错误: 计算=%02X, 接收=%02X", checksum, raw_data[4]);
        ESP_LOGD(TAG, "原始数据: %02X %02X %02X %02X %02X", 
                 raw_data[0], raw_data[1], raw_data[2], raw_data[3], raw_data[4]);
        data->valid = false;
        return ESP_ERR_INVALID_CRC;
    }
    
    // 6. 解析数据
    // DHT11: 湿度整数.湿度小数.温度整数.温度小数.校验和
    data->humidity = raw_data[0] + raw_data[1] * 0.1f;
    data->temperature = raw_data[2] + raw_data[3] * 0.1f;
    data->valid = true;
    data->timestamp = esp_timer_get_time() / 1000;  // 毫秒
    
    ESP_LOGI(TAG, "✅ 温度: %.1f°C, 湿度: %.1f%%", 
             data->temperature, data->humidity);
    ESP_LOGD(TAG, "原始数据: H=%d.%d, T=%d.%d, CRC=%02X", 
             raw_data[0], raw_data[1], raw_data[2], raw_data[3], raw_data[4]);
    
    return ESP_OK;
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

// DHT11测试
void dht11_test(void) {
    ESP_LOGI(TAG, "开始DHT11测试...");
    
    if (!dht11_initialized) {
        ESP_LOGE(TAG, "❌ DHT11未初始化");
        return;
    }
    
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

