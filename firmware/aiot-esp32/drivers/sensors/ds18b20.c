/**
 * @file ds18b20.c
 * @brief DS18B20温度传感器驱动实现
 */

#include "ds18b20.h"
#include <string.h>

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "rom/ets_sys.h"
#else
// 非ESP平台的模拟实现
#include <stdio.h>
#include <unistd.h>
#define ESP_LOGI(tag, format, ...) printf("[%s] " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) printf("[%s] " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGE(tag, format, ...) printf("[%s] " format "\n", tag, ##__VA_ARGS__)
#define ESP_OK 0
#define ESP_FAIL -1
#define esp_timer_get_time() 0
#define ets_delay_us(us) usleep(us)
#define gpio_set_level(pin, level) 
#define gpio_get_level(pin) 1
#define gpio_set_direction(pin, mode) 
#define GPIO_MODE_OUTPUT 1
#define GPIO_MODE_INPUT 0
#endif

static const char *TAG = "DS18B20";

// DS18B20命令
#define DS18B20_CMD_SKIP_ROM        0xCC
#define DS18B20_CMD_CONVERT_T       0x44
#define DS18B20_CMD_READ_SCRATCHPAD 0xBE

// DS18B20时序参数（微秒）
#define DS18B20_RESET_PULSE_TIME    480
#define DS18B20_RESET_WAIT_TIME     70
#define DS18B20_PRESENCE_TIMEOUT    410
#define DS18B20_WRITE_1_LOW_TIME    1
#define DS18B20_WRITE_0_LOW_TIME    60
#define DS18B20_WRITE_RECOVERY_TIME 1
#define DS18B20_READ_LOW_TIME       1
#define DS18B20_READ_SAMPLE_TIME    15
#define DS18B20_READ_RECOVERY_TIME  45

// 转换时间
#define DS18B20_CONVERSION_TIME_MS  750

// 全局变量
static ds18b20_config_t g_ds18b20_config;
static bool g_ds18b20_initialized = false;

/**
 * @brief 微秒级延时
 */
static void ds18b20_delay_us(uint32_t us)
{
    ets_delay_us(us);
}

/**
 * @brief 设置GPIO为输出模式
 */
static void ds18b20_set_output(gpio_num_t pin)
{
    gpio_set_direction(pin, GPIO_MODE_OUTPUT);
}

/**
 * @brief 设置GPIO为输入模式
 */
static void ds18b20_set_input(gpio_num_t pin)
{
    gpio_set_direction(pin, GPIO_MODE_INPUT);
}

/**
 * @brief 设置GPIO输出电平
 */
static void ds18b20_write_pin(gpio_num_t pin, uint32_t level)
{
    gpio_set_level(pin, level);
}

/**
 * @brief 读取GPIO输入电平
 */
static uint32_t ds18b20_read_pin(gpio_num_t pin)
{
    return gpio_get_level(pin);
}

/**
 * @brief DS18B20复位序列
 */
static bool ds18b20_reset(gpio_num_t pin)
{
    bool presence = false;
    
    // 发送复位脉冲：拉低480us
    ds18b20_set_output(pin);
    ds18b20_write_pin(pin, 0);
    ds18b20_delay_us(480);
    
    // 释放总线
    ds18b20_set_input(pin);
    ds18b20_delay_us(70);  // 等待70us
    
    // 检测存在脉冲
    if (ds18b20_read_pin(pin) == 0) {
        presence = true;
    }
    
    // 等待存在脉冲结束
    ds18b20_delay_us(410);
    
    return presence;
}

/**
 * @brief 写一个位到DS18B20
 */
static void ds18b20_write_bit(gpio_num_t pin, uint8_t bit)
{
    ds18b20_set_output(pin);
    ds18b20_write_pin(pin, 0);
    
    if (bit) {
        // 写1：拉低1us后释放
        ds18b20_delay_us(1);
        ds18b20_set_input(pin);
        ds18b20_delay_us(59);
    } else {
        // 写0：拉低60us
        ds18b20_delay_us(60);
        ds18b20_set_input(pin);
        ds18b20_delay_us(1);
    }
}

/**
 * @brief 从DS18B20读一个位
 */
static uint8_t ds18b20_read_bit(gpio_num_t pin)
{
    uint8_t bit = 0;
    
    // 启动读时隙：拉低1us
    ds18b20_set_output(pin);
    ds18b20_write_pin(pin, 0);
    ds18b20_delay_us(1);
    
    // 释放总线并采样
    ds18b20_set_input(pin);
    ds18b20_delay_us(15);  // 等待15us后采样
    
    // 读取数据位
    if (ds18b20_read_pin(pin)) {
        bit = 1;
    }
    
    // 等待读时隙结束
    ds18b20_delay_us(45);
    
    return bit;
}

/**
 * @brief 写一个字节到DS18B20
 */
static void ds18b20_write_byte(gpio_num_t pin, uint8_t byte)
{
    for (int i = 0; i < 8; i++) {
        ds18b20_write_bit(pin, (byte >> i) & 0x01);
    }
}

/**
 * @brief 从DS18B20读一个字节
 */
static uint8_t ds18b20_read_byte(gpio_num_t pin)
{
    uint8_t byte = 0;
    
    for (int i = 0; i < 8; i++) {
        if (ds18b20_read_bit(pin)) {
            byte |= (1 << i);
        }
    }
    
    return byte;
}

/**
 * @brief 计算CRC8校验
 */
static uint8_t ds18b20_crc8(uint8_t *data, uint8_t len)
{
    uint8_t crc = 0;
    
    for (uint8_t i = 0; i < len; i++) {
        uint8_t inbyte = data[i];
        for (uint8_t j = 0; j < 8; j++) {
            uint8_t mix = (crc ^ inbyte) & 0x01;
            crc >>= 1;
            if (mix) {
                crc ^= 0x8C;
            }
            inbyte >>= 1;
        }
    }
    
    return crc;
}

esp_err_t ds18b20_init(const ds18b20_config_t *config)
{
    if (!config) {
        ESP_LOGE(TAG, "Config is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (config->data_pin < 0) {
        ESP_LOGE(TAG, "Invalid data pin: %d", config->data_pin);
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Initializing DS18B20 on GPIO%d", config->data_pin);
    
    // 配置GPIO为输入模式，启用上拉电阻
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << config->data_pin),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO configuration failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 设置初始状态
    gpio_set_level(config->data_pin, 1);
    
    ESP_LOGI(TAG, "DS18B20 pin set to GPIO%d, waiting for stabilization...", config->data_pin);
    vTaskDelay(pdMS_TO_TICKS(100));  // 等待100ms稳定
    
    // 测试传感器连接
    ESP_LOGI(TAG, "Starting DS18B20 initialization sequence...");
    if (!ds18b20_reset(config->data_pin)) {
        ESP_LOGE(TAG, "DS18B20 check failed - sensor not responding on GPIO%d", config->data_pin);
        ESP_LOGE(TAG, "Please check: 1) Hardware connection 2) Power supply 3) Sensor functionality");
        return ESP_ERR_NOT_FOUND;
    }
    
    // 保存配置
    memcpy(&g_ds18b20_config, config, sizeof(ds18b20_config_t));
    g_ds18b20_initialized = true;
    
    ESP_LOGI(TAG, "DS18B20 initialized successfully on GPIO%d", config->data_pin);
    return ESP_OK;
}

esp_err_t ds18b20_read(ds18b20_data_t *data)
{
    if (!g_ds18b20_initialized) {
        ESP_LOGE(TAG, "DS18B20 not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!data) {
        ESP_LOGE(TAG, "Data pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    gpio_num_t pin = g_ds18b20_config.data_pin;
    uint8_t scratchpad[9];
    
    // 初始化数据
    data->temperature = 0.0f;
    data->valid = false;
    
    // 复位并检查传感器存在
    if (!ds18b20_reset(pin)) {
        ESP_LOGW(TAG, "DS18B20 not responding during read");
        return ESP_ERR_TIMEOUT;
    }
    
    // 跳过ROM命令
    ds18b20_write_byte(pin, DS18B20_CMD_SKIP_ROM);
    
    // 启动温度转换
    ds18b20_write_byte(pin, DS18B20_CMD_CONVERT_T);
    
    // 等待转换完成
    vTaskDelay(pdMS_TO_TICKS(DS18B20_CONVERSION_TIME_MS));
    
    // 再次复位
    if (!ds18b20_reset(pin)) {
        ESP_LOGW(TAG, "DS18B20 not responding after conversion");
        return ESP_ERR_TIMEOUT;
    }
    
    // 跳过ROM命令
    ds18b20_write_byte(pin, DS18B20_CMD_SKIP_ROM);
    
    // 读取暂存器
    ds18b20_write_byte(pin, DS18B20_CMD_READ_SCRATCHPAD);
    
    // 读取9字节数据
    for (int i = 0; i < 9; i++) {
        scratchpad[i] = ds18b20_read_byte(pin);
    }
    
    // 验证CRC
    uint8_t crc = ds18b20_crc8(scratchpad, 8);
    if (crc != scratchpad[8]) {
        ESP_LOGW(TAG, "DS18B20 CRC check failed: calculated=0x%02X, received=0x%02X", crc, scratchpad[8]);
        return ESP_FAIL;
    }
    
    // 计算温度
    int16_t temp_raw = (scratchpad[1] << 8) | scratchpad[0];
    data->temperature = (float)temp_raw / 16.0f;
    data->valid = true;
    
    ESP_LOGD(TAG, "DS18B20 read: Temperature=%.1f°C", data->temperature);
    
    return ESP_OK;
}

bool ds18b20_is_initialized(void)
{
    return g_ds18b20_initialized;
}

const ds18b20_config_t* ds18b20_get_config(void)
{
    if (!g_ds18b20_initialized) {
        return NULL;
    }
    return &g_ds18b20_config;
}

esp_err_t ds18b20_deinit(void)
{
    if (!g_ds18b20_initialized) {
        return ESP_OK;
    }
    
    // 重置GPIO为默认状态
    gpio_reset_pin(g_ds18b20_config.data_pin);
    
    g_ds18b20_initialized = false;
    memset(&g_ds18b20_config, 0, sizeof(ds18b20_config_t));
    
    ESP_LOGI(TAG, "DS18B20 deinitialized");
    return ESP_OK;
}