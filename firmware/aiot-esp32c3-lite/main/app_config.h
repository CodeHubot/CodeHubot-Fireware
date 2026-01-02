/**
 * @file app_config.h
 * @brief ESP32-C3 Lite 应用配置文件
 * 
 * 定义应用程序的配置参数，包括MQTT、WiFi、传感器等
 * 
 * @author AIOT Team
 * @date 2025-12-27
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include "board_config.h"

#ifdef __cplusplus
extern "C" {
#endif

// ==================== 版本信息 ====================
#define FIRMWARE_VERSION        "1.0.0"
#define FIRMWARE_BUILD_DATE     __DATE__
#define FIRMWARE_BUILD_TIME     __TIME__

// ==================== MQTT配置 ====================
#define MQTT_MAX_RETRY_COUNT            5
#define MQTT_RETRY_INTERVAL_MS          5000
#define MQTT_KEEPALIVE_S                60
#define MQTT_QOS_DEFAULT                1
#define MQTT_CLEAN_SESSION              1

// MQTT主题配置
#define MQTT_TOPIC_PREFIX               "devices"
#define MQTT_TOPIC_DATA                 "data"
#define MQTT_TOPIC_STATUS               "status"
#define MQTT_TOPIC_HEARTBEAT            "heartbeat"
#define MQTT_TOPIC_CONTROL              "control"

// MQTT心跳和数据上报间隔
#define MQTT_HEARTBEAT_INTERVAL_S       30      // 30秒心跳
#define SENSOR_REPORT_INTERVAL_S        10      // 10秒上报传感器数据
#define STATUS_REPORT_INTERVAL_S        60      // 60秒上报系统状态

// ==================== WiFi配置 ====================
#define WIFI_MAX_RETRY_COUNT            10
#define WIFI_RETRY_INTERVAL_MS          5000
#define WIFI_CONNECT_TIMEOUT_MS         30000   // 30秒连接超时

// WiFi配网AP配置
#define WIFI_CONFIG_AP_SSID_PREFIX      "AIOT-C3-"
#define WIFI_CONFIG_AP_PASSWORD         ""      // 无密码
#define WIFI_CONFIG_AP_CHANNEL          1
#define WIFI_CONFIG_AP_MAX_CONN         4
#define WIFI_CONFIG_WEB_PORT            80

// ==================== 传感器配置 ====================
// DHT11传感器
#define DHT11_READ_INTERVAL_MS          2000    // 2秒读取一次
#define DHT11_MAX_RETRY                 3       // 最多重试3次
#define DHT11_RETRY_DELAY_MS            100     // 重试间隔100ms

// 传感器数据有效性检查
#define TEMP_MIN_VALID                  -40.0f  // 最低温度
#define TEMP_MAX_VALID                  80.0f   // 最高温度
#define HUMI_MIN_VALID                  0.0f    // 最低湿度
#define HUMI_MAX_VALID                  100.0f  // 最高湿度

// ==================== 按键配置 ====================
#define BUTTON_LONG_PRESS_MS            3000    // 长按3秒进入配网
#define BUTTON_DEBOUNCE_MS              50      // 50ms防抖

// ==================== 系统配置 ====================
#define SYSTEM_MONITOR_INTERVAL_MS      30000   // 系统监控间隔30秒
#define DEVICE_ID_MAX_LEN               64
#define DEVICE_UUID_MAX_LEN             64

// ==================== 日志配置 ====================
#define LOG_TAG_MAIN                    "MAIN"
#define LOG_TAG_WIFI                    "WIFI"
#define LOG_TAG_MQTT                    "MQTT"
#define LOG_TAG_SENSOR                  "SENSOR"
#define LOG_TAG_CONTROL                 "CONTROL"

// ==================== NVS存储键名 ====================
#define NVS_NAMESPACE                   "aiot_c3"
#define NVS_KEY_WIFI_SSID               "wifi_ssid"
#define NVS_KEY_WIFI_PASS               "wifi_pass"
#define NVS_KEY_CONFIG_SERVER           "config_srv"   // 配置服务器地址（HTTP API，端口8001）
#define NVS_KEY_MQTT_BROKER             "mqtt_broker"
#define NVS_KEY_MQTT_PORT               "mqtt_port"
#define NVS_KEY_MQTT_USER               "mqtt_user"
#define NVS_KEY_MQTT_PASS               "mqtt_pass"
#define NVS_KEY_DEVICE_ID               "device_id"
#define NVS_KEY_DEVICE_UUID             "device_uuid"
#define NVS_KEY_CONFIG_DONE             "config_done"

// ==================== 内存优化配置 ====================
#define MAX_JSON_BUFFER_SIZE            512     // JSON缓冲区大小
#define MAX_HTTP_RESPONSE_SIZE          1024    // HTTP响应大小
#define MAX_MQTT_MESSAGE_SIZE           512     // MQTT消息大小

// 任务栈大小
#define TASK_STACK_SIZE_SMALL           2048
#define TASK_STACK_SIZE_MEDIUM          3072
#define TASK_STACK_SIZE_LARGE           4096

// ==================== 功耗管理配置 ====================
#define ENABLE_LIGHT_SLEEP              0       // 是否启用轻度睡眠
#define LIGHT_SLEEP_DURATION_MS         100     // 轻度睡眠时长

// WiFi省电模式
#define WIFI_PS_NONE                    0       // 无省电
#define WIFI_PS_MIN_MODEM               1       // 最小modem省电
#define WIFI_PS_MAX_MODEM               2       // 最大modem省电
#define WIFI_PS_MODE_DEFAULT            WIFI_PS_MIN_MODEM

// ==================== 调试配置 ====================
// 注意：DEBUG_ENABLED在board_config.h中已定义，这里不重复定义
#ifndef NDEBUG
#define DEBUG_PRINT_SENSOR_DATA         1
#define DEBUG_PRINT_MQTT_MESSAGE        1
#define DEBUG_PRINT_WIFI_INFO           1
#else
#define DEBUG_PRINT_SENSOR_DATA         0
#define DEBUG_PRINT_MQTT_MESSAGE        0
#define DEBUG_PRINT_WIFI_INFO           0
#endif

// ==================== 特性开关 ====================
#define FEATURE_MQTT_ENABLED            1       // MQTT功能
#define FEATURE_SENSOR_ENABLED          1       // 传感器功能
#define FEATURE_CONTROL_ENABLED         1       // 设备控制功能
#define FEATURE_HEARTBEAT_ENABLED       1       // 心跳功能
#define FEATURE_CONFIG_WEB_ENABLED      1       // Web配网功能

// 精简版禁用的特性
#define FEATURE_OTA_ENABLED             0       // ✅ OTA升级
#define FEATURE_DISPLAY_ENABLED         0       // ✅ 显示功能
#define FEATURE_SERVO_ENABLED           0       // ✅ 舵机控制
#define FEATURE_PWM_ADVANCED_ENABLED    0       // ✅ 高级PWM控制
#define FEATURE_BLUETOOTH_ENABLED       0       // ✅ 蓝牙配网(可选)

#ifdef __cplusplus
}
#endif

#endif // APP_CONFIG_H

