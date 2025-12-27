/** 
 **************************************************************************************************** 
 * @file        dht11.c 
 * @author      AIOT Project Team
 * @version     V1.0 
 * @date        2024-01-01 
 * @brief       DHT11温湿度传感器驱动代码 
 **************************************************************************************************** 
 */ 
  
#include "dht11.h" 
#include "esp_log.h"
#include <string.h>

static const char *TAG = "DHT11";

// 全局配置和状态
static dht11_config_t g_dht11_config;
static dht11_data_t g_last_data = {0};
static bool g_initialized = false;
static int64_t g_last_read_time = 0;

// 全局引脚变量，用于GPIO宏操作
gpio_num_t g_dht11_pin = GPIO_NUM_NC;

// DHT11时序参数
#define DHT11_READ_INTERVAL_MS          2000    // 读取间隔(毫秒)

/** 
 * @brief       复位DHT11 
 * @param       无 
 * @retval      无 
 */ 
void dht11_reset(void) 
{ 
    DHT11_DQ_OUT(0);        /* 拉低DQ */ 
    vTaskDelay(pdMS_TO_TICKS(20));  /* 拉低至少18ms */ 
    DHT11_DQ_OUT(1);        /* DQ=1 */ 
    esp_rom_delay_us(30);   /* 主机拉高20~40us */ 
} 
  
/** 
 * @brief       等待DHT11的回应 
 * @param       无 
 * @retval      0, DHT11正常 
 *              1, DHT11异常/不存在 
 */ 
uint8_t dht11_check(void) 
{ 
    uint8_t retry = 0; 
    uint8_t rval = 0; 
  
    while (DHT11_DQ_IN && retry < 100)      /* DHT11会拉低40~80us */ 
    { 
        retry++; 
        esp_rom_delay_us(1); 
    } 
  
    if (retry >= 100) 
    { 
        rval = 1; 
    } 
    else 
    { 
        retry = 0; 
  
        while (!DHT11_DQ_IN && retry < 100) /* DHT11拉低后会再次拉高87us */ 
        { 
            retry++; 
            esp_rom_delay_us(1); 
        } 
         
        if (retry >= 100) 
        { 
            rval = 1; 
        } 
    } 
     
    return rval; 
} 
  
/** 
 * @brief       读取DHT11的一位数据 
 * @param       无 
 * @retval      0 / 1, 读取到的数据 
 */ 
uint8_t dht11_read_bit(void) 
{ 
    uint8_t retry = 0; 
  
    while (DHT11_DQ_IN && retry < 100)      /* 等待变为低电平 */ 
    { 
        retry++; 
        esp_rom_delay_us(1); 
    } 
  
    retry = 0; 
  
    while (!DHT11_DQ_IN && retry < 100)     /* 等待变高电平 */ 
    { 
        retry++; 
        esp_rom_delay_us(1); 
    } 
  
    esp_rom_delay_us(40);                   /* 等待40us */ 
  
    if (DHT11_DQ_IN) 
    { 
        return 1; 
    } 
    else  
    { 
        return 0; 
    } 
} 
  
/** 
 * @brief       读取DHT11的一个字节数据 
 * @param       无 
 * @retval      读取到的数据 
 */ 
uint8_t dht11_read_byte(void) 
{ 
    uint8_t i, dat; 
    dat = 0; 
  
    for (i = 0; i < 8; i++) 
    { 
        dat <<= 1; 
        dat |= dht11_read_bit(); 
    } 
  
    return dat; 
} 
  
/** 
 * @brief       读取DHT11的温度和湿度数据 
 * @param       temp: 温度值(范围:-20~60°)(放大10倍) 
 * @param       humi: 湿度值(范围:5%~95%)(放大10倍) 
 * @retval      0, 正常; 1, 失败 
 */ 
uint8_t dht11_read_data(short *temp, short *humi) 
{ 
    uint8_t buf[5]; 
    uint8_t i; 
    short raw_temp = 0; 
    short raw_humi = 0; 
    
    dht11_reset(); 
  
    if (dht11_check() == 0) 
    { 
        for (i = 0; i < 5; i++)             /* 读取40位数据 */ 
        { 
            buf[i] = dht11_read_byte(); 
        } 
  
        if ((buf[0] + buf[1] + buf[2] + buf[3]) == buf[4]) 
        { 
            raw_humi = buf[0] * 10 + buf[1];    /* 获取湿度数据 */ 

            if (buf[3] & 0x80)                  /* 温度为负值 */ 
            { 
                raw_temp = buf[2] * 10 + (buf[3] & 0x7F); 
                raw_temp = -raw_temp; 
            } 
            else 
            { 
                raw_temp = buf[2] * 10 + buf[3];  /* 温度数据 */ 
            } 

            *humi = raw_humi; 
            *temp = raw_temp; 
        } 
        else 
        { 
            return 1;  /* 校验失败 */ 
        } 
    } 
    else 
    { 
        return 1; 
    } 
     
    return 0; 
} 
  
/** 
 * @brief       初始化DHT11 
 * @param       无 
 * @retval      0, 正常; 1, 失败 
 */ 
uint8_t dht11_init(void) 
{ 
    gpio_config_t gpio_init_struct; 
  
    gpio_init_struct.intr_type = GPIO_INTR_DISABLE;             /* 失能引脚中断 */ 
    gpio_init_struct.mode = GPIO_MODE_INPUT_OUTPUT_OD;          /* 开漏模式的输入和输出 */ 
    gpio_init_struct.pull_up_en = GPIO_PULLUP_ENABLE;           /* 使能上拉 */ 
    gpio_init_struct.pull_down_en = GPIO_PULLDOWN_DISABLE;      /* 失能下拉 */ 
    gpio_init_struct.pin_bit_mask = 1ull << g_dht11_pin;        /* 设置的引脚的位掩码 */ 
    gpio_config(&gpio_init_struct);                             /* 配置DHT11引脚 */ 
  
    dht11_reset(); 
    return dht11_check(); 
}

// ==================== 适配器函数实现 ====================

esp_err_t dht11_init_adapter(const dht11_config_t *config) {
    if (!config) {
        ESP_LOGE(TAG, "Config is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Initializing DHT11 on GPIO%d", config->data_pin);
    
    // 保存配置
    memcpy(&g_dht11_config, config, sizeof(dht11_config_t));
    
    // 设置全局引脚变量 - 必须在调用dht11_init之前设置
    g_dht11_pin = config->data_pin;
    
    ESP_LOGI(TAG, "DHT11 pin set to GPIO%d, waiting for stabilization...", g_dht11_pin);
    
    // 等待更长时间让GPIO和传感器稳定
    vTaskDelay(pdMS_TO_TICKS(500));
    
    ESP_LOGI(TAG, "Starting DHT11 initialization sequence...");
    
    // 调用原始初始化函数
    uint8_t result = dht11_init();
    
    if (result == 0) {
        g_initialized = true;
        g_last_read_time = 0;
        ESP_LOGI(TAG, "DHT11 initialized successfully on GPIO%d", config->data_pin);
        return ESP_OK;
    } else {
        ESP_LOGE(TAG, "DHT11 check failed - sensor not responding on GPIO%d", config->data_pin);
        ESP_LOGE(TAG, "Please check: 1) Hardware connection 2) Power supply 3) Sensor functionality");
        return ESP_ERR_NOT_FOUND;
    }
}

esp_err_t dht11_read_adapter(dht11_data_t *data) {
    if (!g_initialized) {
        ESP_LOGE(TAG, "DHT11 not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!data) {
        ESP_LOGE(TAG, "Data pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    // 检查读取间隔
    int64_t current_time = esp_timer_get_time();
    if ((current_time - g_last_read_time) < (DHT11_READ_INTERVAL_MS * 1000)) {
        // 返回上次的数据
        memcpy(data, &g_last_data, sizeof(dht11_data_t));
        return ESP_OK;
    }
    
    short temp, humi;
    
    if (dht11_read_data(&temp, &humi) == 0) {
        data->temperature = (float)temp / 10.0f;  // 转换为实际温度值
        data->humidity = (float)humi / 10.0f;     // 转换为实际湿度值
        data->valid = true;
        
        // 保存数据和时间
        memcpy(&g_last_data, data, sizeof(dht11_data_t));
        g_last_read_time = current_time;
        
        ESP_LOGI(TAG, "DHT11 read: Temperature=%.1f°C, Humidity=%.1f%%", 
                 data->temperature, data->humidity);
        
        return ESP_OK;
    } else {
        data->valid = false;
        return ESP_ERR_TIMEOUT;
    }
}

float dht11_get_temperature(void) {
    dht11_data_t data;
    esp_err_t ret = dht11_read_adapter(&data);
    
    if (ret == ESP_OK && data.valid) {
        return data.temperature;
    }
    
    return -999.0f;  // 错误值
}

float dht11_get_humidity(void) {
    dht11_data_t data;
    esp_err_t ret = dht11_read_adapter(&data);
    
    if (ret == ESP_OK && data.valid) {
        return data.humidity;
    }
    
    return -999.0f;  // 错误值
}

bool dht11_is_ready(void) {
    return g_initialized;
}