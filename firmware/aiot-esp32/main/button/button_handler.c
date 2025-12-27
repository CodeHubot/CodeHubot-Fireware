/**
 * @file button_handler.c
 * @brief 按键处理模块实现
 */

#include "button_handler.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "../../boards/esp32-s3-devkit/board_config.h"

static const char *TAG = "button_handler";

// 按键配置
#define BUTTON_DEBOUNCE_TIME_MS     50      // 防抖时间
#define BUTTON_LONG_PRESS_TIME_MS   3000    // 长按时间
#define BUTTON_TASK_STACK_SIZE      4096    // 任务栈大小（增加到4096以支持调试日志）
#define BUTTON_TASK_PRIORITY        5       // 任务优先级

// 按键状态
typedef enum {
    BUTTON_STATE_IDLE = 0,      // 空闲状态
    BUTTON_STATE_PRESSED,       // 按下状态
    BUTTON_STATE_DEBOUNCE,      // 防抖状态
    BUTTON_STATE_LONG_PRESS,    // 长按状态
} button_state_t;

// 全局变量
static button_event_cb_t s_event_cb = NULL;
static TaskHandle_t s_button_task_handle = NULL;
static TimerHandle_t s_debounce_timer = NULL;
static TimerHandle_t s_long_press_timer = NULL;
static button_state_t s_button_state = BUTTON_STATE_IDLE;
static bool s_button_pressed = false;
static bool s_long_press_triggered = false;

// 前向声明
static void button_task(void *pvParameters);
static void debounce_timer_callback(TimerHandle_t xTimer);
static void long_press_timer_callback(TimerHandle_t xTimer);
static void button_isr_handler(void *arg);

/**
 * @brief 按键中断处理函数
 */
static void IRAM_ATTR button_isr_handler(void *arg) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    
    // 通知按键任务处理
    if (s_button_task_handle) {
        vTaskNotifyGiveFromISR(s_button_task_handle, &xHigherPriorityTaskWoken);
        portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
    }
    
    // 添加简单的调试标记（在ISR中不能使用ESP_LOG）
    static volatile uint32_t isr_count = 0;
    isr_count++;
}

/**
 * @brief 防抖定时器回调
 */
static void debounce_timer_callback(TimerHandle_t xTimer) {
    // 读取当前按键状态
    int level = gpio_get_level(BOOT_BUTTON_GPIO);
    bool pressed = (level == 0);  // Boot按键低电平有效
    
    if (pressed && s_button_state == BUTTON_STATE_DEBOUNCE) {
        // 确认按键按下
        s_button_state = BUTTON_STATE_PRESSED;
        s_button_pressed = true;
        s_long_press_triggered = false;
        
        // 启动长按定时器
        xTimerStart(s_long_press_timer, 0);
        
        ESP_LOGD(TAG, "按键按下确认");
    } else if (!pressed && s_button_state == BUTTON_STATE_PRESSED) {
        // 按键释放
        s_button_state = BUTTON_STATE_IDLE;
        s_button_pressed = false;
        
        // 停止长按定时器
        xTimerStop(s_long_press_timer, 0);
        
        // 如果没有触发长按，则触发短按事件
        if (!s_long_press_triggered && s_event_cb) {
            ESP_LOGI(TAG, "检测到短按事件");
            s_event_cb(BUTTON_EVENT_CLICK);
        }
        
        ESP_LOGD(TAG, "按键释放");
    }
}

/**
 * @brief 长按定时器回调
 */
static void long_press_timer_callback(TimerHandle_t xTimer) {
    if (s_button_state == BUTTON_STATE_PRESSED && !s_long_press_triggered) {
        s_long_press_triggered = true;
        
        if (s_event_cb) {
            ESP_LOGI(TAG, "检测到长按事件");
            s_event_cb(BUTTON_EVENT_LONG_PRESS);
        }
    }
}

/**
 * @brief 按键处理任务
 */
static void button_task(void *pvParameters) {
    ESP_LOGI(TAG, "按键处理任务启动");
    ESP_LOGI(TAG, "Boot按键GPIO: %d", BOOT_BUTTON_GPIO);
    
    while (1) {
        // 等待中断通知
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        
        // 读取当前按键状态
        int level = gpio_get_level(BOOT_BUTTON_GPIO);
        bool pressed = (level == 0);  // Boot按键低电平有效
        
        ESP_LOGI(TAG, "🔘 按键中断触发 - GPIO电平: %d, 当前状态: %d", level, s_button_state);
        
        if (pressed && s_button_state == BUTTON_STATE_IDLE) {
            // 按键按下，进入防抖状态
            s_button_state = BUTTON_STATE_DEBOUNCE;
            xTimerStart(s_debounce_timer, 0);
            ESP_LOGI(TAG, "✅ 按键按下检测，开始防抖");
        } else if (!pressed && (s_button_state == BUTTON_STATE_PRESSED || s_button_state == BUTTON_STATE_DEBOUNCE)) {
            // 按键释放，启动防抖定时器处理
            xTimerStart(s_debounce_timer, 0);
            ESP_LOGI(TAG, "✅ 按键释放检测，开始防抖");
        }
    }
}

/**
 * @brief 初始化按键处理模块
 */
esp_err_t button_handler_init(button_event_cb_t event_cb) {
    esp_err_t ret = ESP_OK;
    
    ESP_LOGI(TAG, "初始化按键处理模块");
    
    s_event_cb = event_cb;
    s_button_state = BUTTON_STATE_IDLE;
    s_button_pressed = false;
    s_long_press_triggered = false;
    
    // 配置Boot按键GPIO
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_ANYEDGE,     // 任意边沿触发
        .mode = GPIO_MODE_INPUT,            // 输入模式
        .pin_bit_mask = (1ULL << BOOT_BUTTON_GPIO),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,   // 使能上拉
    };
    
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "配置Boot按键GPIO失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 创建防抖定时器
    s_debounce_timer = xTimerCreate(
        "debounce_timer",
        pdMS_TO_TICKS(BUTTON_DEBOUNCE_TIME_MS),
        pdFALSE,  // 单次触发
        NULL,
        debounce_timer_callback
    );
    
    if (!s_debounce_timer) {
        ESP_LOGE(TAG, "创建防抖定时器失败");
        return ESP_FAIL;
    }
    
    // 创建长按定时器
    s_long_press_timer = xTimerCreate(
        "long_press_timer",
        pdMS_TO_TICKS(BUTTON_LONG_PRESS_TIME_MS),
        pdFALSE,  // 单次触发
        NULL,
        long_press_timer_callback
    );
    
    if (!s_long_press_timer) {
        ESP_LOGE(TAG, "创建长按定时器失败");
        xTimerDelete(s_debounce_timer, 0);
        return ESP_FAIL;
    }
    
    // 创建按键处理任务
    BaseType_t task_ret = xTaskCreate(
        button_task,
        "button_task",
        BUTTON_TASK_STACK_SIZE,
        NULL,
        BUTTON_TASK_PRIORITY,
        &s_button_task_handle
    );
    
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "创建按键处理任务失败");
        xTimerDelete(s_debounce_timer, 0);
        xTimerDelete(s_long_press_timer, 0);
        return ESP_FAIL;
    }
    
    // 安装GPIO中断服务
    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "安装GPIO中断服务失败: %s", esp_err_to_name(ret));
        goto cleanup;
    }
    
    // 添加GPIO中断处理函数
    ret = gpio_isr_handler_add(BOOT_BUTTON_GPIO, button_isr_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "添加GPIO中断处理函数失败: %s", esp_err_to_name(ret));
        goto cleanup;
    }
    
    // 读取并显示当前GPIO状态
    int current_level = gpio_get_level(BOOT_BUTTON_GPIO);
    ESP_LOGI(TAG, "✅ 按键处理模块初始化成功");
    ESP_LOGI(TAG, "   Boot按键GPIO: %d", BOOT_BUTTON_GPIO);
    ESP_LOGI(TAG, "   当前GPIO电平: %d (%s)", current_level, current_level == 0 ? "按下" : "释放");
    ESP_LOGI(TAG, "   长按触发时间: %d ms", BUTTON_LONG_PRESS_TIME_MS);
    return ESP_OK;
    
cleanup:
    if (s_button_task_handle) {
        vTaskDelete(s_button_task_handle);
        s_button_task_handle = NULL;
    }
    if (s_debounce_timer) {
        xTimerDelete(s_debounce_timer, 0);
        s_debounce_timer = NULL;
    }
    if (s_long_press_timer) {
        xTimerDelete(s_long_press_timer, 0);
        s_long_press_timer = NULL;
    }
    return ret;
}

/**
 * @brief 反初始化按键处理模块
 */
esp_err_t button_handler_deinit(void) {
    ESP_LOGI(TAG, "反初始化按键处理模块");
    
    // 移除GPIO中断处理函数
    gpio_isr_handler_remove(BOOT_BUTTON_GPIO);
    
    // 删除任务
    if (s_button_task_handle) {
        vTaskDelete(s_button_task_handle);
        s_button_task_handle = NULL;
    }
    
    // 删除定时器
    if (s_debounce_timer) {
        xTimerStop(s_debounce_timer, 0);
        xTimerDelete(s_debounce_timer, 0);
        s_debounce_timer = NULL;
    }
    
    if (s_long_press_timer) {
        xTimerStop(s_long_press_timer, 0);
        xTimerDelete(s_long_press_timer, 0);
        s_long_press_timer = NULL;
    }
    
    s_event_cb = NULL;
    s_button_state = BUTTON_STATE_IDLE;
    
    return ESP_OK;
}

/**
 * @brief WiFi初始化后重新启用按键中断
 */
esp_err_t button_handler_reinit_after_wifi(void) {
    ESP_LOGI(TAG, "WiFi初始化后重新启用按键中断");
    
    if (!s_button_task_handle) {
        ESP_LOGW(TAG, "按键任务未运行，跳过重新初始化");
        return ESP_ERR_INVALID_STATE;
    }
    
    // 移除旧的GPIO中断处理函数（如果存在）
    gpio_isr_handler_remove(BOOT_BUTTON_GPIO);
    
    // 重新配置GPIO（WiFi可能改变了GPIO配置）
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_ANYEDGE,     // 任意边沿触发
        .mode = GPIO_MODE_INPUT,            // 输入模式
        .pin_bit_mask = (1ULL << BOOT_BUTTON_GPIO),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,   // 使能上拉
    };
    
    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "重新配置GPIO失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 重新安装GPIO ISR服务（如果需要）
    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "重新安装GPIO ISR服务返回: %s", esp_err_to_name(ret));
    }
    
    // 重新添加GPIO中断处理函数
    ret = gpio_isr_handler_add(BOOT_BUTTON_GPIO, button_isr_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "重新添加GPIO中断处理函数失败: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // 读取并显示当前GPIO状态
    int current_level = gpio_get_level(BOOT_BUTTON_GPIO);
    ESP_LOGI(TAG, "✅ 按键中断重新启用成功");
    ESP_LOGI(TAG, "   当前GPIO电平: %d (%s)", current_level, current_level == 0 ? "按下" : "释放");
    
    return ESP_OK;
}

/**
 * @brief 获取Boot按键当前状态
 */
bool button_handler_get_boot_state(void) {
    int level = gpio_get_level(BOOT_BUTTON_GPIO);
    return (level == 0);  // Boot按键低电平有效
}