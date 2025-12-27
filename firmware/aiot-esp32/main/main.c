/**
 * @file main.c
 * @brief AIOT ESP32-S3 主程序
 * 
 * 使用BSP架构的ESP32-S3设备管理系统主程序
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef ESP_PLATFORM
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#else
// 非ESP-IDF环境的兼容性定义
#define ESP_LOGI(tag, format, ...) printf("[%s] " format "\n", tag, ##__VA_ARGS__)
#define ESP_ERROR_CHECK(x) (x)
#endif

// BSP架构头文件（先包含board_config.h以避免重定义）
#include "bsp/bsp_interface.h"

// 根据Kconfig配置选择板子
#ifdef CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_RAIN
    // Rain板子配置
    #include "../boards/esp32-s3-devkit-rain/bsp_esp32_s3_devkit_rain.h"
    #include "../boards/esp32-s3-devkit-rain/board_config.h"
    #define BOARD_IS_RAIN 1
    #define BOARD_IS_LITE 0
#elif defined(CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_LITE)
    // Lite板子配置
    #include "../boards/esp32-s3-devkit-lite/bsp_esp32_s3_devkit_lite.h"
    #include "../boards/esp32-s3-devkit-lite/board_config.h"
    #define BOARD_IS_RAIN 0
    #define BOARD_IS_LITE 1
#else
    // 标准板子配置（默认）
    #include "../boards/esp32-s3-devkit/bsp_esp32_s3_devkit.h"
    #include "../boards/esp32-s3-devkit/board_config.h"
    #define BOARD_IS_RAIN 0
    #define BOARD_IS_LITE 0
#endif

// 应用配置（在board_config.h之后包含，避免重定义）
#include "app_config.h"

// 功能模块头文件
// #include "bluetooth/bt_provision.h"  // 临时禁用
// #include "wechat_ble/wechat_ble.h"  // 临时禁用
#include "mqtt/aiot_mqtt_client.h"
#include "ota/ota_manager.h"
#include "wifi_config/wifi_config.h"
#include "button/button_handler.h"
#include "device/device_registration.h"
#include "server/server_config.h"  // 统一服务器配置
#include "startup/startup_manager.h"  // 统一启动管理器
#include "system/module_init.h"  // 模块初始化管理（旧，保留兼容）
#include "device/device_control.h"  // 设备控制模块
#include "device/preset_control.h"  // 预设控制模块

// 驱动层头文件
#include "lcd_st7789.h"    // 显示驱动
#include "dht11.h"         // 传感器驱动
#if !defined(CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_RAIN) && !defined(CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_LITE)
#include "ds18b20.h"       // 传感器驱动（仅标准板子）
#endif
#ifdef CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_RAIN
#include "rain_sensor.h"   // 雨水传感器驱动（仅Rain板子）
#endif

// 组件层头文件
#include "lvgl_display.h"  // 显示组件
#include "simple_display.h" // 显示组件
#include "lvgl_ui_demo.h"  // UI组件

#ifdef ESP_PLATFORM
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_timer.h"
#endif

static const char *TAG = "AIOT_MAIN";

// 设备ID和UUID（UUID用于MQTT主题）
static char g_device_id[128] = {0};  // 增加到128字符以支持长Device ID（用于client_id）
static char g_device_uuid[128] = {0};  // 设备UUID（用于MQTT主题，与device_uuid_info_t中的长度一致）

// 服务器地址缓存（避免在事件处理器中读取NVS导致栈溢出）
static char g_server_address[64] = {0};  // 服务器地址缓存
static char g_mqtt_command_topic[256] = {0};  // 相应增加MQTT主题长度
static char g_mqtt_sensor_topic[256] = {0};
static char g_mqtt_status_topic[256] = {0};
static char g_mqtt_heartbeat_topic[256] = {0};

// WiFi连接状态定义
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// 配网相关变量
static bool s_config_mode = false;
static wifi_config_data_t s_wifi_config = {0};

/**
 * @brief 更新设备ID、UUID和MQTT主题（内部函数）
 * @param device_uuid 设备UUID（用于构建MQTT主题）
 */
static void update_device_id_and_topics(const char *device_uuid)
{
    if (device_uuid && strlen(device_uuid) > 0) {
        strncpy(g_device_uuid, device_uuid, sizeof(g_device_uuid) - 1);
        g_device_uuid[sizeof(g_device_uuid) - 1] = '\0';
        
        // 构建MQTT主题（使用device_uuid，仅在获取设备标识后构建）
        snprintf(g_mqtt_command_topic, sizeof(g_mqtt_command_topic), "devices/%s/control", g_device_uuid);
        snprintf(g_mqtt_sensor_topic, sizeof(g_mqtt_sensor_topic), "devices/%s/data", g_device_uuid);
        snprintf(g_mqtt_status_topic, sizeof(g_mqtt_status_topic), "devices/%s/status", g_device_uuid);
        snprintf(g_mqtt_heartbeat_topic, sizeof(g_mqtt_heartbeat_topic), "devices/%s/heartbeat", g_device_uuid);
        
        ESP_LOGI(TAG, "Device UUID: %s", g_device_uuid);
        ESP_LOGI(TAG, "MQTT主题已构建: control=%s, data=%s, heartbeat=%s", 
                 g_mqtt_command_topic, g_mqtt_sensor_topic, g_mqtt_heartbeat_topic);
    }
}

// 全局状态变量（需要在函数之前定义）
static bool g_wifi_connected = false;
static bool g_mqtt_connected = false;
static bool g_ble_connected = false;

// DHT11传感器相关
static bool g_dht11_initialized = false;
static dht11_data_t g_sensor_data = {0};

// DS18B20传感器相关（仅标准板子）
#if !defined(CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_RAIN) && !defined(CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_LITE)
static bool g_ds18b20_initialized = false;
static ds18b20_data_t g_ds18b20_data = {0};
#endif

// 雨水传感器相关（仅Rain板子）
#ifdef CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_RAIN
static bool g_rain_sensor_initialized = false;
static rain_sensor_data_t g_rain_sensor_data = {0};
#endif

// 系统运行时间
static uint32_t g_system_start_time = 0;

// 设备注册状态
static bool g_device_registered = false;

// 已移除未使用的LCD句柄变量：g_lcd_handle

// 注释掉未使用的LVGL变量
// static lvgl_display_handle_t g_lvgl_display_handle = {0};
// static lvgl_ui_demo_handle_t g_lvgl_ui_demo_handle = {0};

// Simple Display句柄
static simple_display_t *g_simple_display = NULL;

/**
 * @brief 按键事件处理函数
 */
/**
 * @brief 设备注册事件回调函数
 */
static void __attribute__((unused)) device_registration_event_callback(device_registration_event_t event, 
                                               const device_registration_info_t *info)
{
    switch (event) {
        case DEVICE_REG_EVENT_STARTED:
            ESP_LOGI(TAG, "Device registration started");
            break;
            
        case DEVICE_REG_EVENT_SUCCESS:
            ESP_LOGI(TAG, "Device registration successful");
            if (info) {
                ESP_LOGI(TAG, "Device ID: %s", info->device_id);
                ESP_LOGI(TAG, "Device UUID: %s", info->device_uuid);
                ESP_LOGI(TAG, "MAC Address: %s", info->mac_address);
            }
            g_device_registered = true;
            
            // 更新设备ID和主题，使用注册获得的UUID
            update_device_id_and_topics(info->device_uuid);
            
            // 更新MQTT客户端配置，使用设备注册获得的凭证进行认证
            // 关键: client_id = device_id, username = device_uuid, password = device_secret
            mqtt_config_t updated_mqtt_config = {
                .port = DEFAULT_MQTT_PORT,
                .use_ssl = false,
                .clean_session = true,
                .keepalive = 60,
                .reconnect_timeout = 5000
            };
            
            // 复制字符串字段
            strncpy(updated_mqtt_config.broker_url, DEFAULT_MQTT_BROKER, sizeof(updated_mqtt_config.broker_url) - 1);
            
            // 🔑 关键认证配置：使用设备注册获得的凭证
            // Username = device_uuid, Password = device_secret
            strncpy(updated_mqtt_config.username, info->device_uuid, sizeof(updated_mqtt_config.username) - 1);
            strncpy(updated_mqtt_config.password, info->device_secret, sizeof(updated_mqtt_config.password) - 1);
            strncpy(updated_mqtt_config.client_id, info->device_id, sizeof(updated_mqtt_config.client_id) - 1);
            
            ESP_LOGI(TAG, "🔐 MQTT认证配置:");
            ESP_LOGI(TAG, "   Client ID: %s", updated_mqtt_config.client_id);
            ESP_LOGI(TAG, "   Username:  %s", updated_mqtt_config.username);
            ESP_LOGI(TAG, "   Password:  %s", updated_mqtt_config.password);
            
            // 确保字符串以null结尾
            updated_mqtt_config.broker_url[sizeof(updated_mqtt_config.broker_url) - 1] = '\0';
            updated_mqtt_config.username[sizeof(updated_mqtt_config.username) - 1] = '\0';
            updated_mqtt_config.password[sizeof(updated_mqtt_config.password) - 1] = '\0';
            updated_mqtt_config.client_id[sizeof(updated_mqtt_config.client_id) - 1] = '\0';
            
            esp_err_t update_ret = mqtt_client_update_config(&updated_mqtt_config);
            if (update_ret == ESP_OK) {
                ESP_LOGI(TAG, "MQTT client config updated with new device UUID: %s", g_device_id);
    } else {
                ESP_LOGE(TAG, "Failed to update MQTT client config: %s", esp_err_to_name(update_ret));
            }
            
            // 如果WiFi已连接，重新连接MQTT
            if (g_wifi_connected && !g_mqtt_connected) {
                ESP_LOGI(TAG, "Reconnecting MQTT with new device UUID");
                mqtt_client_connect();
            }
            break;
            
        case DEVICE_REG_EVENT_FAILED:
            ESP_LOGE(TAG, "Device registration failed");
            g_device_registered = false;
            break;
            
        case DEVICE_REG_EVENT_TIMEOUT:
            ESP_LOGE(TAG, "Device registration timeout");
            g_device_registered = false;
            break;
            
        default:
            ESP_LOGW(TAG, "Unknown device registration event: %d", event);
            break;
    }
}

/**
 * @brief 配网重启任务（避免在定时器上下文中执行复杂操作导致栈溢出）
 */
static void provision_restart_task(void *pvParameters) {
    ESP_LOGI(TAG, "⏳ 准备进入配网模式...");
    
    // 设置强制配网标志（NVS写入操作）
    wifi_config_set_force_flag();
    ESP_LOGI(TAG, "✅ 配网标志已设置");
    
    // 停止按钮处理，避免冲突
    button_handler_deinit();
    ESP_LOGI(TAG, "✅ 按钮处理已停止");
    
    // ⚠️ 重要：在重启前停止WiFi和MQTT，避免在重启过程中触发事件导致崩溃
    ESP_LOGI(TAG, "🛑 停止MQTT客户端...");
    if (g_mqtt_connected) {
        mqtt_client_disconnect();
        vTaskDelay(pdMS_TO_TICKS(200));  // 等待断开完成
    }
    
    ESP_LOGI(TAG, "🛑 停止WiFi连接...");
    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(200));  // 等待WiFi断开事件处理完成
    
    ESP_LOGI(TAG, "🛑 停止WiFi驱动...");
    esp_wifi_stop();
    vTaskDelay(pdMS_TO_TICKS(200));  // 等待WiFi完全停止
    
    ESP_LOGI(TAG, "✅ 所有服务已停止");
    
    // 延迟1秒，确保日志输出完成和事件处理完成
    vTaskDelay(pdMS_TO_TICKS(500));
    
    ESP_LOGI(TAG, "🔄 设备重启中...");
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // 重启设备
    esp_restart();
    
    // 永远不会执行到这里
    vTaskDelete(NULL);
}

/**
 * @brief 按键事件处理函数
 */
static void button_event_handler(button_event_t event) {
    switch (event) {
        case BUTTON_EVENT_CLICK:
            ESP_LOGI(TAG, "Boot按键短按 - 预留功能");
            // 可以添加其他功能，如状态指示等
            break;
            
        case BUTTON_EVENT_LONG_PRESS:
            ESP_LOGI(TAG, "🔔 Boot按键长按检测 - 启动配网流程");
            
            // 创建一个独立任务来处理重启操作，避免在定时器上下文中执行复杂操作导致栈溢出
            BaseType_t ret = xTaskCreate(
                provision_restart_task,
                "provision_restart",
                4096,  // 4KB栈空间，足够执行NVS操作和重启
                NULL,
                5,     // 优先级
                NULL
            );
            
            if (ret != pdPASS) {
                ESP_LOGE(TAG, "❌ 创建配网重启任务失败");
            }
            break;
            
        default:
            break;
    }
}

/**
 * @brief WiFi配网事件处理函数
 */
static void __attribute__((unused)) wifi_config_event_handler(wifi_config_event_t event, void *data) {
    switch (event) {
        case WIFI_CONFIG_EVENT_AP_STARTED:
            ESP_LOGI(TAG, "配网AP模式启动成功");
            ESP_LOGI(TAG, "请连接WiFi热点: %s", wifi_config_get_ap_ssid());
            ESP_LOGI(TAG, "然后访问: %s", wifi_config_get_web_url());
            break;
            
        case WIFI_CONFIG_EVENT_CLIENT_CONNECTED:
            ESP_LOGI(TAG, "客户端连接到配网热点");
            break;
            
        case WIFI_CONFIG_EVENT_CONFIG_RECEIVED:
            ESP_LOGI(TAG, "收到WiFi配置，设备即将重启");
            break;
            
        case WIFI_CONFIG_EVENT_WIFI_CONNECTED:
            ESP_LOGI(TAG, "WiFi连接成功");
            break;
            
        case WIFI_CONFIG_EVENT_WIFI_FAILED:
            ESP_LOGI(TAG, "WiFi连接失败");
            break;
            
        default:
            break;
    }
}

/**
 * @brief WiFi事件处理器（用于处理WiFi连接状态）
 * 注意：此函数必须在module_init.c中注册，不能是static
 */
void wifi_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "[WiFi DEBUG] 📡 WiFi Station启动成功");
        ESP_LOGI(TAG, "[WiFi DEBUG] 开始连接WiFi...");
        esp_wifi_connect();
        ESP_LOGI(TAG, "[WiFi DEBUG] ✅ 已调用esp_wifi_connect()");
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t* disconnected = (wifi_event_sta_disconnected_t*) event_data;
        ESP_LOGW(TAG, "[WiFi DEBUG] ⚠️ WiFi断开连接");
        ESP_LOGW(TAG, "[WiFi DEBUG]    断开原因: %d", disconnected->reason);
        ESP_LOGW(TAG, "[WiFi DEBUG]    SSID: %s", disconnected->ssid);
        ESP_LOGW(TAG, "[WiFi DEBUG]    BSSID: " MACSTR, MAC2STR(disconnected->bssid));
        
        g_wifi_connected = false;
        
        // 更新Simple Display的WiFi状态
        if (g_simple_display) {
            // 获取之前的WiFi SSID（如果可用）
            wifi_config_t wifi_config;
            esp_err_t ret = esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
            if (ret == ESP_OK && strlen((char*)wifi_config.sta.ssid) > 0) {
                simple_display_update_wifi_status(g_simple_display, (char*)wifi_config.sta.ssid, "Reconnecting...");
            } else {
                simple_display_update_wifi_status(g_simple_display, "WiFi", "Reconnecting");
            }
        }
        
        // WiFi断开时，MQTT也应该断开
        if (g_mqtt_connected) {
            ESP_LOGI(TAG, "[WiFi DEBUG] WiFi断开，断开MQTT连接");
            mqtt_client_disconnect();
        }
        
        // 重连策略：根据断开原因决定是否立即重连
        // 某些原因（如密码错误）不应该立即重连，但大部分情况应该重连
        bool should_reconnect = true;
        const char* reason_desc = "";
        
        switch (disconnected->reason) {
            case WIFI_REASON_AUTH_EXPIRE:
            case WIFI_REASON_AUTH_FAIL:
                reason_desc = "认证失败";
                // 认证失败时也尝试重连（可能只是临时问题）
                should_reconnect = true;
                break;
            case WIFI_REASON_ASSOC_LEAVE:
                reason_desc = "主动断开";
                // 主动断开时，等待一下再重连
                should_reconnect = true;
                break;
            case WIFI_REASON_BEACON_TIMEOUT:
            case WIFI_REASON_NO_AP_FOUND:
                reason_desc = "信号丢失";
                // 信号丢失，应该重连
                should_reconnect = true;
                break;
            default:
                reason_desc = "其他原因";
                should_reconnect = true;
                break;
        }
        
        if (should_reconnect) {
            // 添加短暂延迟后重连，避免频繁重连
            static uint32_t last_reconnect_time = 0;
            uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            uint32_t time_since_last = current_time - last_reconnect_time;
            
            // 如果距离上次重连不足2秒，延迟2秒再重连
            if (time_since_last < 2000) {
                ESP_LOGI(TAG, "[WiFi DEBUG] ⏳ 距离上次重连仅%lu毫秒，延迟2秒后重连...", time_since_last);
                vTaskDelay(pdMS_TO_TICKS(2000));
            }
            
            ESP_LOGI(TAG, "[WiFi DEBUG] 🔄 尝试重新连接WiFi (原因: %s, 代码: %d)...", reason_desc, disconnected->reason);
            esp_err_t ret = esp_wifi_connect();
            if (ret == ESP_OK) {
                ESP_LOGI(TAG, "[WiFi DEBUG] ✅ 已调用esp_wifi_connect()重新连接");
                last_reconnect_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            } else {
                ESP_LOGE(TAG, "[WiFi DEBUG] ❌ 调用esp_wifi_connect()失败: %s", esp_err_to_name(ret));
            }
        } else {
            ESP_LOGW(TAG, "[WiFi DEBUG] ⚠️ 根据断开原因，暂不自动重连");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "[WiFi DEBUG] ✅ WiFi连接成功！");
        ESP_LOGI(TAG, "[WiFi DEBUG] 📋 IP地址信息:");
        ESP_LOGI(TAG, "[WiFi DEBUG]    IP地址: " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "[WiFi DEBUG]    子网掩码: " IPSTR, IP2STR(&event->ip_info.netmask));
        ESP_LOGI(TAG, "[WiFi DEBUG]    网关: " IPSTR, IP2STR(&event->ip_info.gw));
        g_wifi_connected = true;
        
        // 更新Simple Display的WiFi状态
        if (g_simple_display) {
            // 获取MAC地址
            uint8_t mac[6];
            char mac_str[18];
            esp_wifi_get_mac(WIFI_IF_STA, mac);
            snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            
            // 获取WiFi SSID
            wifi_config_t wifi_config;
            esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
            
            // 更新WiFi状态（合并显示）
            simple_display_update_wifi_status(g_simple_display, (char*)wifi_config.sta.ssid, "Connected");
            
            // 使用缓存的服务器地址（避免在事件处理器中读取NVS导致栈溢出）
            const char *server_addr = (strlen(g_server_address) > 0) ? g_server_address : "Loading...";
            
            // 显示详细信息（使用配置的产品ID）
            simple_display_show_detailed_info(g_simple_display,
                                            PRODUCT_ID,           // device (使用产品ID)
                                            PRODUCT_ID,           // product (使用产品ID)
                                            (char*)wifi_config.sta.ssid,  // wifi_id
                                            "Connected",          // wifi_status
                                            g_mqtt_connected ? "Connected" : "Connecting...", // mqtt_status
                                            mac_str,              // mac
                                            strlen(g_device_uuid) > 0 ? g_device_uuid : "Loading...", // uuid
                                            server_addr);         // server_address
        }
    }
}

/**
 * @brief MQTT事件回调函数
 */
static void mqtt_event_callback(const mqtt_event_data_t *event_data)
{
    if (!event_data) {
        ESP_LOGW(TAG, "MQTT event data is NULL");
        return;
    }
    
    if (event_data->event == MQTT_EVENT_CONNECTED) {
        ESP_LOGI(TAG, "MQTT Connected");
        g_mqtt_connected = true;
        
        // 订阅控制主题
        mqtt_client_subscribe(g_mqtt_command_topic, MQTT_QOS_0);
        ESP_LOGI(TAG, "Subscribed to %s", g_mqtt_command_topic);
        
        // 更新Simple Display的MQTT状态
        if (g_simple_display) {
            simple_display_update_status(g_simple_display, "MQTT: Connected");
            
            // 如果WiFi已连接，重新显示详细信息以更新MQTT状态
            if (g_wifi_connected) {
                // 获取MAC地址
                uint8_t mac[6];
                char mac_str[18];
                esp_wifi_get_mac(WIFI_IF_STA, mac);
                snprintf(mac_str, sizeof(mac_str), "%02x:%02x:%02x:%02x:%02x:%02x",
                         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                
                // 获取WiFi SSID
                wifi_config_t wifi_config;
                esp_wifi_get_config(WIFI_IF_STA, &wifi_config);
                
                // 使用缓存的服务器地址（避免在事件处理器中读取NVS导致栈溢出）
                const char *server_addr = (strlen(g_server_address) > 0) ? g_server_address : "Loading...";
                
                // 显示详细信息（使用配置的产品ID）
                simple_display_show_detailed_info(g_simple_display,
                                                PRODUCT_ID,           // device (使用产品ID)
                                                PRODUCT_ID,           // product (使用产品ID)
                                                (char*)wifi_config.sta.ssid,  // wifi_id
                                                "Connected",          // wifi_status
                                                "Connected",          // mqtt_status
                                                mac_str,              // mac
                                                strlen(g_device_uuid) > 0 ? g_device_uuid : "Loading...", // uuid
                                                server_addr);         // server_address
            }
        }
    } else if (event_data->event == MQTT_EVENT_DISCONNECTED) {
        ESP_LOGW(TAG, "🔌 MQTT连接断开");
        g_mqtt_connected = false;
        
        // 检查WiFi状态，如果WiFi已连接，ESP-IDF会自动重连MQTT
        if (g_wifi_connected) {
            ESP_LOGI(TAG, "🔄 WiFi已连接，ESP-IDF MQTT客户端将自动重连（已启用自动重连）");
            ESP_LOGI(TAG, "   重连间隔: 5秒");
            ESP_LOGI(TAG, "   保持连接: 60秒");
    } else {
            ESP_LOGW(TAG, "⚠️ WiFi未连接，等待WiFi连接后再重连MQTT");
        }
        
        // 更新Simple Display的MQTT状态
        if (g_simple_display) {
            if (g_wifi_connected) {
                simple_display_update_status(g_simple_display, "MQTT: Reconnecting...");
            } else {
                simple_display_update_status(g_simple_display, "MQTT: Disconnected");
            }
        }
    } else if (event_data->event == MQTT_EVENT_ERROR) {
        ESP_LOGE(TAG, "🚨 MQTT错误事件");
        g_mqtt_connected = false;
        
        // 检查错误类型
        if (event_data->error_code == ESP_ERR_WIFI_NOT_CONNECT) {
            ESP_LOGW(TAG, "⚠️ WiFi未连接，MQTT无法连接");
        } else {
            ESP_LOGE(TAG, "❌ MQTT错误代码: %s", esp_err_to_name(event_data->error_code));
            ESP_LOGI(TAG, "🔄 ESP-IDF MQTT客户端将自动重连");
        }
        
        // 更新Simple Display的MQTT状态
        if (g_simple_display) {
            simple_display_update_status(g_simple_display, "MQTT: Error");
        }
    } else if (event_data->event == AIOT_MQTT_EVENT_MESSAGE_RECEIVED) {
        ESP_LOGI(TAG, "MQTT Message received on topic: %s", 
                 event_data->message ? event_data->message->topic : "unknown");
        
        // 处理控制命令（使用设备控制模块和预设控制模块）
        if (event_data->message && 
            strncmp(event_data->message->topic, g_mqtt_command_topic, strlen(g_mqtt_command_topic)) == 0) {
            ESP_LOGI(TAG, "Processing control command: %.*s", 
                     event_data->message->payload_len, 
                     (char*)event_data->message->payload);
            
            // 解析JSON命令
            char *payload = malloc(event_data->message->payload_len + 1);
            if (payload) {
                memcpy(payload, event_data->message->payload, event_data->message->payload_len);
                payload[event_data->message->payload_len] = '\0';
                
                // 先解析JSON检查cmd字段，确定是预设命令还是设备控制命令
                cJSON *json = cJSON_Parse(payload);
                if (!json) {
                    ESP_LOGE(TAG, "Failed to parse JSON: %s", cJSON_GetErrorPtr());
                    free(payload);
                    return;
                }
                
                // 检查cmd字段
                cJSON *cmd_item = cJSON_GetObjectItem(json, "cmd");
                const char *cmd_str = NULL;
                if (cmd_item && cJSON_IsString(cmd_item)) {
                    cmd_str = cmd_item->valuestring;
                    ESP_LOGI(TAG, "Parsed cmd field: '%s'", cmd_str);
    } else {
                    ESP_LOGW(TAG, "Missing or invalid 'cmd' field in JSON");
                }
                
                bool is_preset = (cmd_str && strcmp(cmd_str, "preset") == 0);
                ESP_LOGI(TAG, "Command type check: is_preset=%d, cmd_str='%s'", is_preset, cmd_str ? cmd_str : "NULL");
                cJSON_Delete(json);  // 释放临时JSON对象
                
                if (is_preset) {
                    ESP_LOGI(TAG, "Found preset command: %s", cmd_str);
                    
                    // 使用预设控制模块解析和执行
                    preset_control_command_t preset_cmd;
                    esp_err_t ret = preset_control_parse_json_command(payload, &preset_cmd);
                    if (ret == ESP_OK) {
                        preset_control_result_t preset_result;
                        ret = preset_control_execute(&preset_cmd, &preset_result);
                        if (ret == ESP_OK && preset_result.success) {
                            ESP_LOGI(TAG, "✅ Preset command executed successfully");
                        } else {
                            ESP_LOGE(TAG, "❌ Preset command failed: %s", 
                                    preset_result.error_msg ? preset_result.error_msg : esp_err_to_name(ret));
                        }
                        preset_control_free_command(&preset_cmd);
                    } else {
                        ESP_LOGE(TAG, "Failed to parse preset command: %s", esp_err_to_name(ret));
                    }
                } else {
                    ESP_LOGI(TAG, "Found device control command: %s", cmd_str ? cmd_str : "unknown");
                    
                    // 使用设备控制模块解析和执行
                    device_control_command_t device_cmd;
                    esp_err_t ret = device_control_parse_json_command(payload, &device_cmd);
                    if (ret == ESP_OK) {
                        device_control_result_t device_result;
                        ret = device_control_execute(&device_cmd, &device_result);
                        if (ret == ESP_OK && device_result.success) {
                            ESP_LOGI(TAG, "✅ Device control command executed successfully");
                        } else {
                            ESP_LOGE(TAG, "❌ Device control command failed: %s", 
                                    device_result.error_msg ? device_result.error_msg : esp_err_to_name(ret));
                        }
                    } else {
                        ESP_LOGE(TAG, "Failed to parse device control command: %s", esp_err_to_name(ret));
                    }
                }
                
                free(payload);
            }
        }
    } else if (event_data->event == MQTT_EVENT_ERROR) {
        ESP_LOGI(TAG, "MQTT Error: %d", event_data->error_code);
    }
}

/**
 * @brief 微信小程序蓝牙事件回调函数
 */
/*
static void wechat_ble_event_callback(wechat_ble_event_t *event)
{
    switch (event->event_type) {
        case WECHAT_BLE_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WeChat BLE Connected");
            g_ble_connected = true;
            break;
        case WECHAT_BLE_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "WeChat BLE Disconnected");
            g_ble_connected = false;
            break;
        case WECHAT_BLE_EVENT_WIFI_CONFIG_RECEIVED:
            ESP_LOGI(TAG, "WiFi config received via WeChat BLE");
            break;
        case WECHAT_BLE_EVENT_MQTT_CONFIG_RECEIVED:
            ESP_LOGI(TAG, "MQTT config received via WeChat BLE");
            break;
        default:
            break;
    }
}
*/

// 已移除OTA事件回调函数（已由startup_manager统一处理）
/*
static void ota_event_callback(ota_event_type_t event, const ota_progress_t *progress)
{
    switch (event) {
        case OTA_EVENT_START:
            ESP_LOGI(TAG, "OTA Update started");
            break;
        case OTA_EVENT_PROGRESS:
            ESP_LOGI(TAG, "OTA Progress: %d%%", progress->progress_percent);
            break;
        case OTA_EVENT_COMPLETE:
            ESP_LOGI(TAG, "OTA Update completed successfully");
            break;
        case OTA_EVENT_ERROR:
            ESP_LOGI(TAG, "OTA Update failed");
            break;
        default:
            break;
    }
}
*/

/**
 * @brief 蓝牙配网事件回调函数
 */
/*
static void bt_provision_event_callback(bt_provision_state_t state, bt_provision_err_t error, const char* message)
{
    ESP_LOGI(TAG, "BT Provision State: %s, Error: %d, Message: %s", 
             bt_provision_get_state_string(state), error, message ? message : "");
    
    if (state == BT_PROVISION_STATE_SUCCESS) {
        g_wifi_connected = true;
        ESP_LOGI(TAG, "WiFi provisioning completed successfully");
    }
}
*/

// 已移除未使用的LED和L1控制函数：led1_set_state, l1_set_state

/**
 * @brief 开机L1测试
 * 在系统启动时立即测试L1控制功能
 */
// 已移除开机测试函数：startup_l1_test, startup_led1_test, led_blink_demo

// 已移除未使用的LCD初始化和测试函数：init_lcd_display

// 已移除未使用的函数: init_all_modules (已由 startup_manager_run() 替代)


/**
 * @brief 系统状态监控任务
 */
static void system_monitor_task(void *pvParameters)
{
#ifdef ESP_PLATFORM
    static uint32_t heartbeat_sequence = 0;
    static uint32_t last_heartbeat_time = 0;
    static uint32_t last_sensor_report_time = 0;
    static uint32_t last_status_report_time = 0;
    
    // 心跳间隔已移到心跳发送代码中，使用CONFIG_MQTT_HEARTBEAT_INTERVAL_MS（默认30秒）
    const uint32_t SENSOR_REPORT_INTERVAL = 10;  // 传感器数据上报间隔：10秒
    const uint32_t STATUS_REPORT_INTERVAL = 30;  // 系统状态上报间隔：30秒
    
    while (1) {
        // 获取系统信息
        uint32_t free_heap = esp_get_free_heap_size();
        uint32_t uptime = (esp_timer_get_time() / 1000000) - g_system_start_time;
        
        // 更新Simple Display运行时间和连接状态
        if (g_simple_display) {
            simple_display_update_uptime(g_simple_display, uptime);
            
            // 定期更新MQTT状态显示（每10秒检查一次）
            static uint32_t last_mqtt_status_update = 0;
            static bool last_mqtt_status = false;
            if (uptime - last_mqtt_status_update >= 10 || last_mqtt_status != g_mqtt_connected) {
                const char *mqtt_status_str = g_mqtt_connected ? "Connected" : "Disconnected";
                simple_display_update_mqtt_status(g_simple_display, mqtt_status_str);
                last_mqtt_status = g_mqtt_connected;
                last_mqtt_status_update = uptime;
                
                if (last_mqtt_status != g_mqtt_connected) {
                    ESP_LOGI(TAG, "📺 LCD MQTT状态已更新: %s", mqtt_status_str);
                }
            }
        }
        
        // === MQTT心跳发送（按照FIRMWARE_MANUAL.md要求） ===
        // 心跳格式：{"sequence": <uint>, "timestamp": <ms>, "status": <0/1/2>}
        // 发布间隔：30秒（可配置，默认MQTT_HEARTBEAT_INTERVAL_MS）
        // 发布条件：仅当UUID已获取且MQTT已连接；断开时暂停发布
        // QoS：1
        #ifndef CONFIG_MQTT_HEARTBEAT_INTERVAL_MS
        #define CONFIG_MQTT_HEARTBEAT_INTERVAL_MS 30000  // 默认30秒
        #endif
        const uint32_t HEARTBEAT_INTERVAL_MS = CONFIG_MQTT_HEARTBEAT_INTERVAL_MS;
        const uint32_t HEARTBEAT_INTERVAL_SEC = HEARTBEAT_INTERVAL_MS / 1000;
        
        if (uptime - last_heartbeat_time >= HEARTBEAT_INTERVAL_SEC) {
            // 仅当UUID已获取（g_device_id不是临时MAC地址）且MQTT已连接时发送心跳
            if (g_mqtt_connected && strlen(g_device_id) > 0 && 
                strncmp(g_device_id, "AIOT_", 5) != 0) {  // 检查不是临时MAC地址格式
                
                heartbeat_sequence++;
                uint64_t timestamp_ms = esp_timer_get_time() / 1000;  // 毫秒时间戳
                
                // 状态：0=离线，1=在线，2=错误（这里使用1表示在线）
                uint8_t status = 1;
                
                // 构建心跳消息（符合文档要求格式）
                char heartbeat_json[128];
                snprintf(heartbeat_json, sizeof(heartbeat_json),
                    "{\"sequence\":%lu,\"timestamp\":%llu,\"status\":%d}",
                    heartbeat_sequence, timestamp_ms, status);
                
                // 显示心跳数据包
                ESP_LOGI(TAG, "📤 Publishing heartbeat to topic: %s", g_mqtt_heartbeat_topic);
                ESP_LOGI(TAG, "📦 Payload: %s", heartbeat_json);
                
                // 发布心跳（QoS=1，符合文档要求）
                esp_err_t pub_ret = mqtt_client_publish(g_mqtt_heartbeat_topic, heartbeat_json, 
                                                       strlen(heartbeat_json), MQTT_QOS_1, false);
                if (pub_ret == ESP_OK) {
                    ESP_LOGI(TAG, "💓 Heartbeat #%lu sent successfully (status=%d, timestamp=%llu ms)", 
                             heartbeat_sequence, status, timestamp_ms);
                } else {
                    ESP_LOGW(TAG, "Heartbeat publish failed: %s", esp_err_to_name(pub_ret));
                }
            }
            last_heartbeat_time = uptime;
        }
        
        // === 传感器数据上报 (每10秒) ===
        if (uptime - last_sensor_report_time >= SENSOR_REPORT_INTERVAL) {
            bool sensor_data_updated = false;
            
            // 读取DHT11传感器数据（带重试：最多尝试3次）
            if (g_dht11_initialized) {
                esp_err_t dht11_ret = ESP_FAIL;
                int dht11_retry_count = 0;
                const int dht11_max_retries = 3;
                
                // 重试读取（最多3次）
                while (dht11_retry_count < dht11_max_retries) {
                    dht11_ret = dht11_read_adapter(&g_sensor_data);
                    if (dht11_ret == ESP_OK && g_sensor_data.valid) {
                        // 读取成功
                        break;
                    }
                    dht11_retry_count++;
                    if (dht11_retry_count < dht11_max_retries) {
                        ESP_LOGW(TAG, "DHT11读取失败，重试 %d/%d...", dht11_retry_count, dht11_max_retries - 1);
                        vTaskDelay(pdMS_TO_TICKS(100));  // 重试前延迟100ms
                    }
                }
                
                if (g_sensor_data.valid) {
                    ESP_LOGI(TAG, "🌡️ DHT11数据 - 温度: %.1f°C, 湿度: %.1f%% (尝试次数: %d)", 
                             g_sensor_data.temperature, g_sensor_data.humidity, dht11_retry_count + 1);
                    
                    // 更新动态传感器UI - DHT11（传感器索引0）
                    if (g_simple_display) {
                        char dht11_value[32];
                        snprintf(dht11_value, sizeof(dht11_value), "%.1fC / %.1f%%", 
                                g_sensor_data.temperature, g_sensor_data.humidity);
                        simple_display_update_sensor_value(g_simple_display, 0, dht11_value);
                    }
                    
                    // 上传DHT11传感器数据到MQTT
                    if (g_mqtt_connected) {
                        char sensor_json[256];
                        snprintf(sensor_json, sizeof(sensor_json),
                            "{\"device_id\":\"%s\",\"sensor\":\"DHT11\",\"temperature\":%.1f,\"humidity\":%.1f,\"timestamp\":%lu}",
                            g_device_id, g_sensor_data.temperature, g_sensor_data.humidity, uptime);
                        
                        ESP_LOGI(TAG, "📤 Publishing DHT11 data to topic: %s", g_mqtt_sensor_topic);
                        ESP_LOGI(TAG, "📦 Payload: %s", sensor_json);
                        
                        int msg_id = mqtt_client_publish(g_mqtt_sensor_topic, sensor_json, strlen(sensor_json), MQTT_QOS_1, false);
                        if (msg_id >= 0) {
                            ESP_LOGI(TAG, "✅ DHT11 data published successfully (msg_id=%d)", msg_id);
                        } else {
                            ESP_LOGE(TAG, "❌ DHT11 data publish failed (msg_id=%d)", msg_id);
                        }
                    } else {
                        ESP_LOGW(TAG, "⚠️ MQTT not connected, DHT11 data not sent");
                    }
                    sensor_data_updated = true;
                } else {
                    ESP_LOGW(TAG, "DHT11读取失败（已重试%d次）", dht11_max_retries);
                }
            }
            
            // 读取DS18B20传感器数据（仅标准板子，Rain和Lite板子不使用DS18B20）
#if !defined(CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_RAIN) && !defined(CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_LITE)
            if (g_ds18b20_initialized) {
                esp_err_t ds18b20_ret = ESP_FAIL;
                int ds18b20_retry_count = 0;
                const int ds18b20_max_retries = 3;
                
                // 重试读取（最多3次）
                while (ds18b20_retry_count < ds18b20_max_retries) {
                    ds18b20_ret = ds18b20_read(&g_ds18b20_data);
                    if (ds18b20_ret == ESP_OK && g_ds18b20_data.valid) {
                        // 读取成功
                        break;
                    }
                    ds18b20_retry_count++;
                    if (ds18b20_retry_count < ds18b20_max_retries) {
                        ESP_LOGW(TAG, "DS18B20读取失败，重试 %d/%d...", ds18b20_retry_count, ds18b20_max_retries - 1);
                        vTaskDelay(pdMS_TO_TICKS(100));  // 重试前延迟100ms
                    }
                }
                
                if (g_ds18b20_data.valid) {
                    ESP_LOGI(TAG, "🌡️ DS18B20数据 - 温度: %.1f°C (尝试次数: %d)", 
                             g_ds18b20_data.temperature, ds18b20_retry_count + 1);
                    
                    // 更新动态传感器UI - DS18B20（传感器索引1，仅标准板）
                        if (g_simple_display) {
                        char ds18b20_value[32];
                        snprintf(ds18b20_value, sizeof(ds18b20_value), "%.1fC", g_ds18b20_data.temperature);
                        simple_display_update_sensor_value(g_simple_display, 1, ds18b20_value);
                    }
                    
                    // 上传DS18B20传感器数据到MQTT
                    if (g_mqtt_connected) {
                        char sensor_json[256];
                        snprintf(sensor_json, sizeof(sensor_json),
                            "{\"device_id\":\"%s\",\"sensor\":\"DS18B20\",\"temperature\":%.1f,\"timestamp\":%lu}",
                            g_device_id, g_ds18b20_data.temperature, uptime);
                        
                        ESP_LOGI(TAG, "📤 Publishing DS18B20 data to topic: %s", g_mqtt_sensor_topic);
                        ESP_LOGI(TAG, "📦 Payload: %s", sensor_json);
                        
                        int msg_id = mqtt_client_publish(g_mqtt_sensor_topic, sensor_json, strlen(sensor_json), MQTT_QOS_1, false);
                        if (msg_id >= 0) {
                            ESP_LOGI(TAG, "✅ DS18B20 data published successfully (msg_id=%d)", msg_id);
                        } else {
                            ESP_LOGE(TAG, "❌ DS18B20 data publish failed (msg_id=%d)", msg_id);
                        }
                    } else {
                        ESP_LOGW(TAG, "⚠️ MQTT not connected, DS18B20 data not sent");
                    }
                    sensor_data_updated = true;
                } else {
                    ESP_LOGW(TAG, "DS18B20读取失败（已重试%d次）", ds18b20_max_retries);
                }
            }
#endif  // CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_RAIN
            
            // 读取雨水传感器数据（仅Rain板子）
#ifdef CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_RAIN
            if (g_rain_sensor_initialized) {
                esp_err_t rain_ret = rain_sensor_read(&g_rain_sensor_data);
                if (rain_ret == ESP_OK && g_rain_sensor_data.valid) {
                    ESP_LOGI(TAG, "🌧️ 雨水传感器数据 - 是否下雨: %s, 电平: %d", 
                             g_rain_sensor_data.is_raining ? "是" : "否", g_rain_sensor_data.level);
                    
                    // 更新动态传感器UI - 雨水传感器（传感器索引1，仅Rain板）
                    if (g_simple_display) {
                        const char *rain_status = g_rain_sensor_data.is_raining ? "Raining" : "Dry";
                        simple_display_update_sensor_value(g_simple_display, 1, rain_status);
                    }
                    
                    // 上传雨水传感器数据到MQTT
                    if (g_mqtt_connected) {
                        char sensor_json[256];
                        snprintf(sensor_json, sizeof(sensor_json),
                            "{\"device_id\":\"%s\",\"sensor\":\"RAIN_SENSOR\",\"is_raining\":%s,\"level\":%d,\"timestamp\":%lu}",
                            g_device_id, 
                            g_rain_sensor_data.is_raining ? "true" : "false",
                            g_rain_sensor_data.level,
                            uptime);
                        
                        ESP_LOGI(TAG, "📤 Publishing RAIN_SENSOR data to topic: %s", g_mqtt_sensor_topic);
                        ESP_LOGI(TAG, "📦 Payload: %s", sensor_json);
                        
                        int msg_id = mqtt_client_publish(g_mqtt_sensor_topic, sensor_json, strlen(sensor_json), MQTT_QOS_1, false);
                        if (msg_id >= 0) {
                            ESP_LOGI(TAG, "✅ RAIN_SENSOR data published successfully (msg_id=%d)", msg_id);
                        } else {
                            ESP_LOGE(TAG, "❌ RAIN_SENSOR data publish failed (msg_id=%d)", msg_id);
                        }
                    } else {
                        ESP_LOGW(TAG, "⚠️ MQTT not connected, RAIN_SENSOR data not sent");
                    }
                    sensor_data_updated = true;
                } else {
                    ESP_LOGW(TAG, "⚠️ 雨水传感器读取失败: %s", esp_err_to_name(rain_ret));
                }
            }
#endif
            
            if (sensor_data_updated) {
                ESP_LOGI(TAG, "📊 传感器数据已上报");
            }
            last_sensor_report_time = uptime;
        }
        
        // === 系统状态上报 (每30秒) ===
        if (uptime - last_status_report_time >= STATUS_REPORT_INTERVAL) {
            // 同步MQTT实际连接状态（使用MQTT客户端的实际状态）
            bool mqtt_actually_connected = mqtt_client_is_connected();
            if (mqtt_actually_connected != g_mqtt_connected) {
                ESP_LOGW(TAG, "⚠️ MQTT状态不同步：g_mqtt_connected=%d, 实际状态=%d，同步中...", 
                         g_mqtt_connected, mqtt_actually_connected);
                g_mqtt_connected = mqtt_actually_connected;
                
                // 如果状态变为已连接，更新LCD显示
                if (g_mqtt_connected && g_simple_display) {
                    simple_display_update_status(g_simple_display, "MQTT: Connected");
                }
            }
            
            // MQTT断线重连监控：如果WiFi已连接但MQTT断开，检查是否需要主动触发重连
            if (g_wifi_connected && !mqtt_actually_connected) {
                mqtt_connection_state_t mqtt_state = mqtt_client_get_state();
                if (mqtt_state == MQTT_STATE_DISCONNECTED || mqtt_state == MQTT_STATE_ERROR) {
                    // ESP-IDF应该自动重连，但我们可以记录状态
                    static uint32_t last_reconnect_check = 0;
                    uint32_t current_time = uptime;
                    // 每30秒检查一次，避免频繁日志
                    if (current_time - last_reconnect_check >= 30) {
                        ESP_LOGI(TAG, "🔄 MQTT监控: WiFi已连接，MQTT未连接，ESP-IDF自动重连中...");
                        ESP_LOGI(TAG, "   MQTT状态: %s", mqtt_client_get_state_string(mqtt_state));
                        last_reconnect_check = current_time;
                    }
                }
            }
            
            ESP_LOGI(TAG, "=== System Status ===");
            ESP_LOGI(TAG, "Uptime: %lu seconds", uptime);
            ESP_LOGI(TAG, "Free heap: %lu bytes", free_heap);
            ESP_LOGI(TAG, "WiFi: %s", g_wifi_connected ? "Connected" : "Disconnected");
            ESP_LOGI(TAG, "MQTT: %s (实际状态: %s)", 
                     g_mqtt_connected ? "Connected" : "Disconnected",
                     mqtt_client_get_state_string(mqtt_client_get_state()));
            ESP_LOGI(TAG, "BLE: %s", g_ble_connected ? "Connected" : "Disconnected");
            
            // 上传系统状态数据到MQTT
            if (g_mqtt_connected) {
                char status_json[512];
                snprintf(status_json, sizeof(status_json),
                    "{\"device_id\":\"%s\",\"uptime\":%lu,\"free_heap\":%lu,\"wifi_connected\":%s,\"mqtt_connected\":%s,\"ble_connected\":%s,\"timestamp\":%lu}",
                    g_device_id, uptime, free_heap, 
                    g_wifi_connected ? "true" : "false",
                    g_mqtt_connected ? "true" : "false", 
                    g_ble_connected ? "true" : "false",
                    uptime);
                
                ESP_LOGI(TAG, "📤 Publishing status to topic: %s", g_mqtt_status_topic);
                ESP_LOGI(TAG, "📦 Payload: %s", status_json);
                
                int msg_id = mqtt_client_publish(g_mqtt_status_topic, status_json, strlen(status_json), MQTT_QOS_1, false);
                if (msg_id >= 0) {
                    ESP_LOGI(TAG, "✅ System status published successfully (msg_id=%d)", msg_id);
                } else {
                    ESP_LOGE(TAG, "❌ System status publish failed (msg_id=%d)", msg_id);
                }
            } else {
                ESP_LOGW(TAG, "⚠️ MQTT not connected, system status not sent");
            }
            last_status_report_time = uptime;
        }
        
        // 简化的MQTT连接管理 - 依赖ESP-IDF自动重连，但需要设备先注册
        static bool mqtt_start_attempted = false;
        if (g_wifi_connected && !g_mqtt_connected && !mqtt_start_attempted && g_device_registered) {
            ESP_LOGI(TAG, "🔄 设备已注册，启动MQTT客户端（ESP-IDF将自动处理重连）...");
            mqtt_client_connect();
            mqtt_start_attempted = true;
        } else if (!g_wifi_connected || !g_device_registered) {
            // WiFi断开或设备未注册时重置MQTT启动标志
            if (mqtt_start_attempted) {
                if (!g_wifi_connected) {
                    ESP_LOGW(TAG, "⚠️ WiFi断开，重置MQTT启动标志");
                } else if (!g_device_registered) {
                    ESP_LOGW(TAG, "⚠️ 设备未注册，重置MQTT启动标志");
                }
                mqtt_start_attempted = false;
            }
            // 同步MQTT实际状态（不强制设置，让MQTT客户端自己管理状态）
            bool mqtt_actually_connected = mqtt_client_is_connected();
            if (mqtt_actually_connected != g_mqtt_connected) {
                ESP_LOGW(TAG, "⚠️ MQTT状态同步：%s -> %s", 
                         g_mqtt_connected ? "Connected" : "Disconnected",
                         mqtt_actually_connected ? "Connected" : "Disconnected");
                g_mqtt_connected = mqtt_actually_connected;
            }
        } else if (g_wifi_connected && !g_device_registered) {
            // WiFi已连接但设备未注册，显示等待注册状态
            static uint32_t last_reg_log_time = 0;
            if (uptime - last_reg_log_time > 10000) { // 每10秒打印一次
                ESP_LOGI(TAG, "⏳ WiFi已连接，等待设备注册完成...");
                last_reg_log_time = uptime;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(5000)); // 5秒检查一次，提高响应性
    }
#endif
}



/**
 * @brief ESP32应用程序入口
 */
void app_main(void)
{
    ESP_LOGI(TAG, "=== AIOT ESP32-S3 Advanced System ===");
    ESP_LOGI(TAG, "ESP-IDF Version: %s", esp_get_idf_version());
    
#ifdef ESP_PLATFORM
    // 记录系统启动时间
    g_system_start_time = esp_timer_get_time() / 1000000;
    
    // =====================================
    // 初始化NVS（需要在GPIO配置之前）
    // =====================================
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized");
    
    // =====================================
    // 🔘 配置Boot按键GPIO（准备后续检测）
    // =====================================
    gpio_config_t boot_io_conf = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ULL << BOOT_BUTTON_GPIO),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    gpio_config(&boot_io_conf);
    ESP_LOGI(TAG, "Boot按键GPIO配置完成");
#endif
    
    // 初始化BSP（根据Kconfig配置选择板子）
    ESP_LOGI(TAG, "初始化BSP...");
    
#ifdef CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_RAIN
    // Rain板子
    bsp_esp32_s3_devkit_rain_register();
    const bsp_interface_t* bsp = bsp_get_interface();
    if (bsp && bsp->init) {
        bsp->init();
        ESP_LOGI(TAG, "BSP初始化完成 (Rain板子)");
    }
    bsp_esp32_s3_devkit_rain_print_config();
#elif defined(CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_LITE)
    // Lite板子
    bsp_esp32_s3_devkit_lite_register();
    const bsp_interface_t* bsp = bsp_get_interface();
    if (bsp && bsp->init) {
        bsp->init();
        ESP_LOGI(TAG, "BSP初始化完成 (Lite板子)");
    }
    bsp_esp32_s3_devkit_lite_print_config();
#else
    // 标准板子
    bsp_esp32_s3_devkit_register();
    const bsp_interface_t* bsp = bsp_get_interface();
    if (bsp && bsp->init) {
        bsp->init();
        ESP_LOGI(TAG, "BSP初始化完成 (标准板子)");
    }
    bsp_esp32_s3_devkit_print_config();
#endif
    
    // =====================================
    // 初始化LCD显示系统
    // =====================================
    ESP_LOGI(TAG, "🖥️ 初始化LCD显示系统...");
    
    // 1. 初始化LCD硬件（ST7789驱动）
    lcd_handle_t lcd_handle = {0};
    esp_err_t lcd_ret = lcd_init(&lcd_handle);
    if (lcd_ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ LCD硬件初始化失败: %s", esp_err_to_name(lcd_ret));
        // LCD失败不应阻止系统启动，继续运行
    } else {
        ESP_LOGI(TAG, "✅ LCD硬件初始化成功 (ST7789, 240x240)");
        
        // 2. 初始化Simple Display（LVGL显示系统）
        g_simple_display = simple_display_init(
            lcd_handle.panel_io,
                                               lcd_handle.panel, 
            LCD_BACKLIGHT_PIN,
            LCD_BACKLIGHT_OUTPUT_INVERT,
            LCD_WIDTH,
            LCD_HEIGHT,
            LCD_MIRROR_X,
            LCD_MIRROR_Y,
            LCD_SWAP_XY
        );
        
        if (g_simple_display) {
            ESP_LOGI(TAG, "✅ Simple Display初始化成功");
            ESP_LOGI(TAG, "📺 LCD启动UI已启用 - 将显示详细启动过程");
            
            // 清屏准备显示启动UI
            simple_display_clear_for_startup(g_simple_display);
            
            // 设置背光亮度为80%（柔和不刺眼）
            simple_display_set_backlight(g_simple_display, 80);
        } else {
            ESP_LOGE(TAG, "❌ Simple Display初始化失败");
        }
    }
    
    // =====================================
    // 🔘 启动Boot按键检测窗口（带倒计时提示）
    // =====================================
    ESP_LOGI(TAG, "🔘 启动Boot按键检测窗口...");
    bool boot_key_detected = false;
    const int detection_window_ms = 3000;  // 3秒检测窗口
    const int sample_interval_ms = 100;    // 100ms采样一次
    const int samples_needed = 3;          // 需要连续3次检测到按下才确认
    int pressed_count = 0;
    
    for (int elapsed_ms = 0; elapsed_ms < detection_window_ms; elapsed_ms += sample_interval_ms) {
        // 计算剩余时间（秒）
        int remaining_sec = (detection_window_ms - elapsed_ms) / 1000 + 1;
        
        // 在LCD上显示倒计时提示
        if (g_simple_display) {
            char countdown_msg[64];
            snprintf(countdown_msg, sizeof(countdown_msg), "Boot key -> Config (%ds)", remaining_sec);
            simple_display_show_startup_step(g_simple_display, "Detect", countdown_msg);
        }
        
        // 读取Boot按键状态
        int boot_level = gpio_get_level(BOOT_BUTTON_GPIO);
        if (boot_level == 0) {
            // 检测到按键按下
            pressed_count++;
            ESP_LOGI(TAG, "🔘 Boot按键按下检测 (%d/%d)", pressed_count, samples_needed);
            
            if (pressed_count >= samples_needed) {
                // 连续多次检测到按下，确认按键有效
                boot_key_detected = true;
                ESP_LOGW(TAG, "✅ Boot按键长按确认！将进入配网模式");
                
                // 在LCD上显示确认信息
                if (g_simple_display) {
                    simple_display_show_startup_step(g_simple_display, "Boot Key", "Enter Config Mode!");
                    vTaskDelay(pdMS_TO_TICKS(1000));  // 显示1秒
                }
                break;
            }
        } else {
            // 按键未按下，重置计数
            pressed_count = 0;
        }
        
        vTaskDelay(pdMS_TO_TICKS(sample_interval_ms));
    }
    
    // 如果检测到Boot按键，设置配网标志
    if (boot_key_detected) {
        ESP_LOGW(TAG, "🔘 设置强制配网标志（启动时检测到Boot按键）");
        wifi_config_set_force_flag();
        ESP_LOGW(TAG, "✅ 配网标志已设置，系统将进入配网模式");
    } else {
        ESP_LOGI(TAG, "🔘 Boot按键未检测到，正常启动");
        // 清除检测提示，准备显示正常启动流程
        if (g_simple_display) {
            simple_display_clear_for_startup(g_simple_display);
        }
    }
    
    // =====================================
    // 使用统一启动管理器初始化所有功能模块
    // =====================================
    ESP_LOGI(TAG, "启动系统管理器...");
    // 传入g_simple_display以启用LCD启动UI显示
    esp_err_t init_ret = startup_manager_run(g_simple_display, NULL, button_event_handler);
    if (init_ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ 系统启动完成");
        
        // ✅ 关键修复：从startup_manager获取device UUID并更新MQTT主题
        const char *device_uuid = startup_manager_get_device_uuid();
        const char *device_id = startup_manager_get_device_id();
        
        if (device_uuid && strlen(device_uuid) > 0) {
            // 更新device UUID和MQTT主题
            update_device_id_and_topics(device_uuid);
            ESP_LOGI(TAG, "✅ 已从startup_manager设置Device UUID和MQTT主题");
        } else {
            ESP_LOGW(TAG, "⚠️ 未能从startup_manager获取Device UUID");
        }
        
        if (device_id && strlen(device_id) > 0) {
            strncpy(g_device_id, device_id, sizeof(g_device_id) - 1);
            g_device_id[sizeof(g_device_id) - 1] = '\0';
            ESP_LOGI(TAG, "✅ 已设置Device ID: %s", g_device_id);
        }
        
        // ✅ WiFi已在startup_manager中连接，更新状态
        g_wifi_connected = true;
        ESP_LOGI(TAG, "✅ WiFi状态已同步");
        
        // ✅ 初始化传感器（在系统启动成功后）
        // 每个传感器独立初始化，互不影响
        ESP_LOGI(TAG, "📊 初始化传感器...");
        
        // 初始化DHT11传感器（独立初始化）
        dht11_config_t dht11_config = {
            .data_pin = DHT11_GPIO_PIN
        };
        esp_err_t dht11_ret = dht11_init_adapter(&dht11_config);
        if (dht11_ret == ESP_OK) {
            g_dht11_initialized = true;
            ESP_LOGI(TAG, "✅ DHT11传感器初始化成功 - GPIO%d已就绪", DHT11_GPIO_PIN);
        } else {
            g_dht11_initialized = false;
            ESP_LOGW(TAG, "⚠️ DHT11传感器初始化失败: %s - 将继续运行，DHT11数据不可用", esp_err_to_name(dht11_ret));
        }
        
        // 初始化DS18B20传感器（仅标准板子，Rain和Lite板子不使用DS18B20）
#if !defined(CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_RAIN) && !defined(CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_LITE)
        ds18b20_config_t ds18b20_config = {
            .data_pin = DS18B20_GPIO_PIN
        };
        esp_err_t ds18b20_ret = ds18b20_init(&ds18b20_config);
        if (ds18b20_ret == ESP_OK) {
            g_ds18b20_initialized = true;
            ESP_LOGI(TAG, "✅ DS18B20传感器初始化成功 - GPIO%d已就绪", DS18B20_GPIO_PIN);
        } else {
            g_ds18b20_initialized = false;
            ESP_LOGW(TAG, "⚠️ DS18B20传感器初始化失败: %s - 将继续运行，DS18B20数据不可用", esp_err_to_name(ds18b20_ret));
        }
#else
        // Rain和Lite板子不使用DS18B20
        // g_ds18b20_initialized 在Rain和Lite板子时未定义，不需要设置
#if defined(CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_RAIN)
        ESP_LOGI(TAG, "ℹ️ Rain板子：DS18B20已禁用，GPIO39用于雨水传感器");
#elif defined(CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_LITE)
        ESP_LOGI(TAG, "ℹ️ Lite板子：DS18B20已禁用，仅支持DHT11传感器");
#endif
#endif
        
        // ✅ 初始化雨水传感器（仅Rain板子，独立初始化，不影响其他传感器）
#ifdef CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_RAIN
        ESP_LOGI(TAG, "🌧️ 初始化雨水传感器...");
        rain_sensor_config_t rain_config = {
            .data_pin = RAIN_SENSOR_GPIO_PIN,  // GPIO39（原DS18B20管脚）
            .pull_up_enable = true,  // 启用内部上拉
            .debounce_ms = 50         // 50ms防抖
        };
        esp_err_t rain_ret = rain_sensor_init(&rain_config);
        if (rain_ret == ESP_OK) {
            g_rain_sensor_initialized = true;
            ESP_LOGI(TAG, "✅ 雨水传感器初始化成功 - GPIO%d已就绪", RAIN_SENSOR_GPIO_PIN);
        } else {
            g_rain_sensor_initialized = false;  // 明确设置为false，确保读取逻辑不会尝试读取
            ESP_LOGW(TAG, "⚠️ 雨水传感器初始化失败: %s - 将继续运行，雨水传感器数据不可用（不影响其他传感器）", esp_err_to_name(rain_ret));
        }
#endif
        
        // 总结所有传感器初始化结果
#ifdef CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_RAIN
        // Rain板子：DHT11 + 雨水传感器（GPIO39）
        if (g_dht11_initialized || g_rain_sensor_initialized) {
            ESP_LOGI(TAG, "📊 所有传感器初始化完成 - DHT11: %s, 雨水传感器(GPIO39): %s", 
                     g_dht11_initialized ? "✅" : "❌",
                     g_rain_sensor_initialized ? "✅" : "❌");
        } else {
            ESP_LOGW(TAG, "⚠️ 所有传感器初始化失败 - 系统将继续运行，但传感器数据不可用");
        }
#else
        // 标准板子：DHT11 + DS18B20
#if !defined(CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_RAIN) && !defined(CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_LITE)
        if (g_dht11_initialized || g_ds18b20_initialized) {
            ESP_LOGI(TAG, "📊 传感器初始化完成 - DHT11: %s, DS18B20: %s", 
                     g_dht11_initialized ? "✅" : "❌",
                     g_ds18b20_initialized ? "✅" : "❌");
        } else {
            ESP_LOGW(TAG, "⚠️ DHT11和DS18B20传感器初始化失败 - 系统将继续运行，但这两个传感器数据不可用");
        }
#elif defined(CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_LITE)
        // Lite板子：仅DHT11
        if (g_dht11_initialized) {
            ESP_LOGI(TAG, "📊 传感器初始化完成 - DHT11: ✅");
        } else {
            ESP_LOGW(TAG, "⚠️ DHT11传感器初始化失败 - 系统将继续运行，但传感器数据不可用");
        }
#endif
#endif
        
        // ✅ 启动完成后，切换LCD到运行时主界面
        if (g_simple_display) {
            ESP_LOGI(TAG, "📺 切换LCD到运行时主界面...");
            
            // 准备显示数据
            const char *product_str = PRODUCT_ID;
            const char *wifi_status_str = g_wifi_connected ? "Connected" : "Disconnected";
            const char *mqtt_status_str = g_mqtt_connected ? "Connected" : "Connecting...";
            const char *uuid_str = (device_uuid && strlen(device_uuid) > 0) ? device_uuid : "Loading...";
            
            // 初始温湿度显示为占位符（传感器数据会在后续更新）
            float init_temp = 0.0f;
            float init_hum = 0.0f;
            uint32_t init_uptime = 0;
            
            // 1️⃣ 先显示运行时主界面（这会清空屏幕）
            simple_display_show_runtime_main(g_simple_display,
                                           product_str,
                                           wifi_status_str,
                                           mqtt_status_str,
                                           uuid_str,
                                           init_temp,
                                           init_hum,
                                           init_uptime);
            
            ESP_LOGI(TAG, "✅ LCD运行时主界面已显示");
            ESP_LOGI(TAG, "   Product: %s", product_str);
            ESP_LOGI(TAG, "   WiFi: %s", wifi_status_str);
            ESP_LOGI(TAG, "   MQTT: %s", mqtt_status_str);
            ESP_LOGI(TAG, "   UUID: %s", uuid_str);
            
            // 2️⃣ 然后初始化传感器动态UI（在主界面基础上添加传感器显示）
            ESP_LOGI(TAG, "🎨 初始化传感器动态UI...");
            const bsp_board_info_t *board_info = bsp_get_board_info();
            if (board_info && board_info->sensor_display_count > 0) {
                // 构建传感器配置（需要类型转换，因为结构体布局相同）
                board_sensor_config_t sensor_config = {
                    .sensor_list = (const sensor_display_info_t *)board_info->sensor_display_list,
                    .sensor_count = board_info->sensor_display_count
                };
                
                // 初始化传感器UI（在主界面下方显示）
                simple_display_init_sensor_ui(g_simple_display, &sensor_config);
                
                ESP_LOGI(TAG, "✅ 传感器动态UI初始化完成");
                ESP_LOGI(TAG, "   板子: %s", board_info->board_name);
                ESP_LOGI(TAG, "   传感器数量: %d", sensor_config.sensor_count);
                for (int i = 0; i < sensor_config.sensor_count; i++) {
                    ESP_LOGI(TAG, "   传感器%d: %s (GPIO%d) %s", 
                             i + 1,
                             sensor_config.sensor_list[i].name,
                             sensor_config.sensor_list[i].gpio_pin,
                             sensor_config.sensor_list[i].unit);
                }
            } else {
                ESP_LOGW(TAG, "⚠️ 未找到传感器配置信息，跳过传感器UI初始化");
            }
        }
        
    } else {
        ESP_LOGE(TAG, "❌ 系统启动失败: %s", esp_err_to_name(init_ret));
        
        // 检查是否是设备未注册（WiFi已连接但设备未在后端注册）
        bool device_not_registered = startup_manager_is_device_not_registered();
        
        if (device_not_registered) {
            // WiFi已连接，但设备未注册 - 显示提示信息，不要进入配网模式
            ESP_LOGE(TAG, "❌ 设备未注册（WiFi已连接，但设备未在后端注册）");
            ESP_LOGE(TAG, "   请先在管理页面注册设备");
            
            // 获取MAC地址用于显示
            uint8_t mac[6];
            esp_err_t mac_ret = esp_read_mac(mac, ESP_MAC_WIFI_STA);
            if (mac_ret == ESP_OK) {
                ESP_LOGE(TAG, "   MAC地址: %02X:%02X:%02X:%02X:%02X:%02X",
                         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            }
            
            // 在LCD上显示设备未注册提示
            if (g_simple_display) {
                char mac_str[18] = {0};
                if (mac_ret == ESP_OK) {
                    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
                }
                simple_display_show_not_registered_info(g_simple_display, mac_str);
                ESP_LOGI(TAG, "✅ LCD设备未注册提示已显示");
            }
            
            // 设备未注册时，系统继续运行（保持WiFi连接），等待用户注册
            // 不进入配网模式，因为WiFi已经连接成功
            ESP_LOGW(TAG, "⚠️ 系统将继续运行，等待用户在后端注册设备");
            ESP_LOGW(TAG, "   用户可以长按Boot按键进入配网模式（如果需要重新配置WiFi）");
        } else if (init_ret == ESP_ERR_NOT_FOUND) {
            // WiFi配置不存在或连接失败 - 需要配网
            ESP_LOGI(TAG, "🔧 检测到需要配网（WiFi配置不存在或连接失败），启动WiFi AP配网模式");
            
            // 初始化WiFi配网模块
            esp_err_t config_ret = wifi_config_init(wifi_config_event_handler);
            if (config_ret == ESP_OK) {
                ESP_LOGI(TAG, "✅ WiFi配网模块初始化成功");
                
                // 启动AP配网模式
                config_ret = wifi_config_start();
                if (config_ret == ESP_OK) {
                    ESP_LOGI(TAG, "✅ WiFi AP配网模式已启动");
                    ESP_LOGI(TAG, "📱 请连接WiFi热点: %s", wifi_config_get_ap_ssid());
                    ESP_LOGI(TAG, "🌐 打开浏览器访问: %s", wifi_config_get_web_url());
                    
                    // 📺 在LCD上显示配网信息
                    if (g_simple_display) {
                        const char *ap_ssid = wifi_config_get_ap_ssid();
                        const char *web_url = wifi_config_get_web_url();
                        ESP_LOGI(TAG, "📺 正在LCD上显示配网引导信息...");
                        simple_display_show_provisioning_info(g_simple_display, ap_ssid, web_url);
                        ESP_LOGI(TAG, "✅ LCD配网引导信息已显示");
                    }
                } else {
                    ESP_LOGE(TAG, "❌ WiFi AP配网模式启动失败: %s", esp_err_to_name(config_ret));
                }
            } else {
                ESP_LOGE(TAG, "❌ WiFi配网模块初始化失败: %s", esp_err_to_name(config_ret));
            }
        } else {
            // 其他错误
            ESP_LOGE(TAG, "❌ 系统启动失败，错误码: %s", esp_err_to_name(init_ret));
        }
    }
    
#ifdef ESP_PLATFORM
    // 创建系统监控任务
    ESP_LOGI(TAG, "=== System Monitor Task Creation ===");
    xTaskCreate(system_monitor_task, "system_monitor", 4096, NULL, 5, NULL);
#endif
    
    ESP_LOGI(TAG, "=== System Startup Completed ===");
    ESP_LOGI(TAG, "All modules initialized and running");
    ESP_LOGI(TAG, "Available features:");
    ESP_LOGI(TAG, "  - Bluetooth Provisioning");
    ESP_LOGI(TAG, "  - WeChat Mini Program BLE");
    ESP_LOGI(TAG, "  - MQTT Communication");
    ESP_LOGI(TAG, "  - OTA Updates");
    ESP_LOGI(TAG, "  - System Monitoring");
    
    // 主循环 - 保持系统运行
    while (1) {
#ifdef ESP_PLATFORM
        vTaskDelay(pdMS_TO_TICKS(10000));
        
        // 定期检查系统状态
        static uint32_t last_status_check = 0;
        uint32_t current_time = esp_timer_get_time() / 1000000;
        
        if (current_time - last_status_check > 60) { // 每分钟检查一次
            ESP_LOGI(TAG, "System heartbeat - Uptime: %lu seconds", 
                     current_time - g_system_start_time);
            last_status_check = current_time;
        }
#else
        ESP_LOGI(TAG, "System simulation completed");
        break;
#endif
    }
}

#ifndef ESP_PLATFORM
/**
 * @brief 标准C程序入口（仅用于非ESP-IDF环境的演示编译）
 */
int main(void)
{
    printf("=== ESP32-S3 LED Test Program (Simulation) ===\n");
    app_main();
    return 0;
}
#endif