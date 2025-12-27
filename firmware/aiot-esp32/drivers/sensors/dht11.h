/** 
 **************************************************************************************************** 
 * @file        dht11.h 
 * @author      AIOT Project Team
 * @version     V1.0 
 * @date        2024-01-01 
 * @brief       DHT11温湿度传感器驱动代码 
 **************************************************************************************************** 
 */ 

#ifndef __DHT11_H
#define __DHT11_H

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "rom/ets_sys.h"

#ifdef __cplusplus
extern "C" {
#endif

// DHT11 GPIO宏定义 - 动态配置引脚
extern gpio_num_t g_dht11_pin;
#define DHT11_DQ_GPIO_PIN   g_dht11_pin                 /* DHT11数据引脚 - 动态配置 */

/* DHT11引脚高低电平枚举 */ 
typedef enum  
{ 
    DHT11_PIN_RESET = 0u, 
    DHT11_PIN_SET 
}DHT11_GPIO_PinState; 

/* IO操作 */ 
#define DHT11_DQ_IN     gpio_get_level(g_dht11_pin)   /* 数据端口输入 */ 

/* DHT11端口定义 */ 
#define DHT11_DQ_OUT(x) do{ x ? \
                            gpio_set_level(g_dht11_pin, DHT11_PIN_SET) : \
                            gpio_set_level(g_dht11_pin, DHT11_PIN_RESET); \
                        }while(0)

/* 普中科技原始函数声明 */ 
void dht11_reset(void);                                 /* 复位DHT11 */ 
uint8_t dht11_init(void);                               /* 初始化DHT11 */ 
uint8_t dht11_check(void);                              /* 等待DHT11的回应 */ 
uint8_t dht11_read_data(short *temp, short *humi);   /* 读取温湿度 */ 

/**
 * @brief DHT11传感器数据结构
 */
typedef struct {
    float temperature;  // 温度值 (°C)
    float humidity;     // 湿度值 (%)
    bool valid;         // 数据是否有效
} dht11_data_t;

/**
 * @brief DHT11传感器配置结构
 */
typedef struct {
    gpio_num_t data_pin;    // 数据引脚
    uint32_t timeout_us;    // 超时时间(微秒)
} dht11_config_t;

/* 适配器函数声明 - 保持与原有接口兼容 */

/**
 * @brief 初始化DHT11传感器（适配器函数）
 * 
 * @param config DHT11配置
 * @return esp_err_t 
 */
esp_err_t dht11_init_adapter(const dht11_config_t *config);

/**
 * @brief 读取DHT11传感器数据（适配器函数）
 * 
 * @param data 输出数据结构
 * @return esp_err_t 
 */
esp_err_t dht11_read_adapter(dht11_data_t *data);

/**
 * @brief 获取温度值
 * 
 * @return float 温度值，失败返回-999.0
 */
float dht11_get_temperature(void);

/**
 * @brief 获取湿度值
 * 
 * @return float 湿度值，失败返回-999.0
 */
float dht11_get_humidity(void);

/**
 * @brief 检查DHT11是否就绪
 * 
 * @return true 就绪
 * @return false 未就绪
 */
bool dht11_is_ready(void);

#ifdef __cplusplus
}
#endif

#endif // __DHT11_H