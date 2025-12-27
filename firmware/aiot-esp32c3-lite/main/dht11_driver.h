/**
 * @file dht11_driver.h
 * @brief DHT11温湿度传感器驱动
 * 
 * DHT11单总线协议实现
 * 
 * @author AIOT Team
 * @date 2025-12-27
 */

#ifndef DHT11_DRIVER_H
#define DHT11_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"
#include "board_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief DHT11数据结构
 */
typedef struct {
    float temperature;      // 温度 (摄氏度)
    float humidity;         // 湿度 (%)
    bool valid;             // 数据有效性
    uint32_t timestamp;     // 读取时间戳
} dht11_data_t;

/**
 * @brief DHT11初始化
 * @param gpio_num GPIO引脚号
 * @return ESP_OK 成功，其他失败
 */
esp_err_t dht11_init(gpio_num_t gpio_num);

/**
 * @brief 读取DHT11数据
 * @param data 输出数据
 * @return ESP_OK 成功，其他失败
 */
esp_err_t dht11_read(dht11_data_t *data);

/**
 * @brief 读取DHT11温度
 * @param temperature 输出温度
 * @return ESP_OK 成功，其他失败
 */
esp_err_t dht11_read_temperature(float *temperature);

/**
 * @brief 读取DHT11湿度
 * @param humidity 输出湿度
 * @return ESP_OK 成功，其他失败
 */
esp_err_t dht11_read_humidity(float *humidity);

/**
 * @brief 检查DHT11是否可用
 * @return true可用，false不可用
 */
bool dht11_is_available(void);

/**
 * @brief DHT11测试
 */
void dht11_test(void);

#ifdef __cplusplus
}
#endif

#endif // DHT11_DRIVER_H

