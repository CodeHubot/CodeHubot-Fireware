#ifndef DS18B20_H
#define DS18B20_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief DS18B20传感器配置结构体
 */
typedef struct {
    gpio_num_t data_pin;        ///< 数据引脚
    uint32_t timeout_us;        ///< 超时时间（微秒）
} ds18b20_config_t;

/**
 * @brief DS18B20传感器数据结构体
 */
typedef struct {
    float temperature;          ///< 温度值（摄氏度）
    bool valid;                ///< 数据是否有效
} ds18b20_data_t;

/**
 * @brief 初始化DS18B20传感器
 * 
 * @param config 传感器配置
 * @return esp_err_t 
 *         - ESP_OK: 初始化成功
 *         - ESP_ERR_INVALID_ARG: 参数无效
 *         - ESP_ERR_NOT_FOUND: 传感器未找到
 */
esp_err_t ds18b20_init(const ds18b20_config_t *config);

/**
 * @brief 读取DS18B20传感器数据
 * 
 * @param data 输出数据结构体
 * @return esp_err_t 
 *         - ESP_OK: 读取成功
 *         - ESP_ERR_INVALID_STATE: 传感器未初始化
 *         - ESP_ERR_TIMEOUT: 读取超时
 *         - ESP_FAIL: 读取失败
 */
esp_err_t ds18b20_read(ds18b20_data_t *data);

/**
 * @brief 检查DS18B20传感器是否已初始化
 * 
 * @return true 已初始化
 * @return false 未初始化
 */
bool ds18b20_is_initialized(void);

/**
 * @brief 获取DS18B20传感器配置信息
 * 
 * @return const ds18b20_config_t* 配置信息指针，如果未初始化则返回NULL
 */
const ds18b20_config_t* ds18b20_get_config(void);

/**
 * @brief 反初始化DS18B20传感器
 * 
 * @return esp_err_t 
 *         - ESP_OK: 反初始化成功
 */
esp_err_t ds18b20_deinit(void);

#ifdef __cplusplus
}
#endif

#endif // DS18B20_H