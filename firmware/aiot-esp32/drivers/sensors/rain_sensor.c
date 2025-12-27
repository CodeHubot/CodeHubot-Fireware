/** 
 **************************************************************************************************** 
 * @file        rain_sensor.c 
 * @author      AIOT Project Team
 * @version     V1.0 
 * @date        2024-01-01 
 * @brief       雨水传感器驱动代码
 * 
 * 雨水传感器工作原理：
 * - 传感器输出数字信号，通过GPIO读取
 * - 高电平(1): 无雨水，传感器表面干燥
 * - 低电平(0): 有雨水，传感器表面湿润导致短路
 * - 需要上拉电阻确保高电平稳定
 **************************************************************************************************** 
 */ 

#include "rain_sensor.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "RAIN_SENSOR";

// 全局配置和状态
static rain_sensor_config_t g_rain_sensor_config;
static rain_sensor_data_t g_last_data = {0};
static bool g_initialized = false;

/**
 * @brief 初始化雨水传感器
 * 
 * @param config 传感器配置
 * @return esp_err_t ESP_OK表示成功
 */
esp_err_t rain_sensor_init(const rain_sensor_config_t *config)
{
    if (!config) {
        ESP_LOGE(TAG, "Config is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (config->data_pin < 0) {
        ESP_LOGE(TAG, "Invalid data pin: %d", config->data_pin);
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Initializing rain sensor on GPIO%d", config->data_pin);
    
    // 配置GPIO为输入模式
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << config->data_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = config->pull_up_enable ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO configuration failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 保存配置
    memcpy(&g_rain_sensor_config, config, sizeof(rain_sensor_config_t));
    g_initialized = true;
    
    // 等待GPIO稳定
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // 读取一次初始值
    rain_sensor_data_t init_data;
    rain_sensor_read(&init_data);
    
    ESP_LOGI(TAG, "Rain sensor initialized successfully on GPIO%d", config->data_pin);
    ESP_LOGI(TAG, "Initial reading: %s (level=%d)", 
             init_data.is_raining ? "RAINING" : "NO RAIN", init_data.level);
    
    return ESP_OK;
}

/**
 * @brief 读取雨水传感器数据（带防抖）
 * 
 * @param data 输出数据结构
 * @return esp_err_t ESP_OK表示成功
 */
esp_err_t rain_sensor_read(rain_sensor_data_t *data)
{
    if (!g_initialized) {
        ESP_LOGE(TAG, "Rain sensor not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!data) {
        ESP_LOGE(TAG, "Data pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    // 读取GPIO电平值
    int level = gpio_get_level(g_rain_sensor_config.data_pin);
    
    // 防抖处理：连续读取多次，确保电平稳定
    uint32_t debounce_ms = g_rain_sensor_config.debounce_ms;
    if (debounce_ms > 0) {
        int stable_count = 0;
        const int required_stable = 3;  // 需要连续3次相同才认为稳定
        
        for (int i = 0; i < required_stable; i++) {
            int current_level = gpio_get_level(g_rain_sensor_config.data_pin);
            if (current_level == level) {
                stable_count++;
            } else {
                level = current_level;
                stable_count = 1;
            }
            if (i < required_stable - 1) {
                vTaskDelay(pdMS_TO_TICKS(debounce_ms / required_stable));
            }
        }
    }
    
    // 填充数据
    data->level = (uint8_t)level;
    data->is_raining = (level == 0);  // 低电平表示有雨水
    data->valid = true;
    
    // 保存最后读取的数据
    memcpy(&g_last_data, data, sizeof(rain_sensor_data_t));
    
    return ESP_OK;
}

/**
 * @brief 检查是否有雨水
 * 
 * @return true 有雨水
 * @return false 无雨水
 */
bool rain_sensor_is_raining(void)
{
    if (!g_initialized) {
        return false;
    }
    
    rain_sensor_data_t data;
    if (rain_sensor_read(&data) == ESP_OK && data.valid) {
        return data.is_raining;
    }
    
    return false;
}

/**
 * @brief 获取当前电平值
 * 
 * @return uint8_t 0=低电平(有雨), 1=高电平(无雨)
 */
uint8_t rain_sensor_get_level(void)
{
    if (!g_initialized) {
        return 1;  // 默认返回高电平（无雨）
    }
    
    return (uint8_t)gpio_get_level(g_rain_sensor_config.data_pin);
}

/**
 * @brief 检查传感器是否已初始化
 * 
 * @return true 已初始化
 * @return false 未初始化
 */
bool rain_sensor_is_ready(void)
{
    return g_initialized;
}

/**
 * @brief 去初始化雨水传感器
 * 
 * @return esp_err_t ESP_OK表示成功
 */
esp_err_t rain_sensor_deinit(void)
{
    if (!g_initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Deinitializing rain sensor...");
    
    // 重置GPIO配置（可选）
    gpio_reset_pin(g_rain_sensor_config.data_pin);
    
    g_initialized = false;
    memset(&g_rain_sensor_config, 0, sizeof(rain_sensor_config_t));
    memset(&g_last_data, 0, sizeof(rain_sensor_data_t));
    
    ESP_LOGI(TAG, "Rain sensor deinitialized");
    
    return ESP_OK;
}

