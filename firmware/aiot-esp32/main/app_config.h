/**
 * @file app_config.h
 * @brief 应用配置常量定义
 * 
 * 定义应用程序中使用的默认配置值
 */

#ifndef APP_CONFIG_H
#define APP_CONFIG_H

// ============================================================================
// 📝 设备配置（烧录前请修改 DEVICE_CONFIG.h）
// ============================================================================
#include "../DEVICE_CONFIG.h"  // 包含产品ID和固件版本配置

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// WiFi 配置
// ============================================================================
// 注意：这些是默认值，如果NVS中没有保存的配置则使用
// 实际生产环境应该通过配网获取
// 如果board_config.h已定义，则使用board_config.h的定义
#ifndef DEFAULT_WIFI_SSID
#define DEFAULT_WIFI_SSID       "YOUR_WIFI_SSID"      // 替换为您的WiFi名称
#endif

#ifndef DEFAULT_WIFI_PASS
#define DEFAULT_WIFI_PASS       "YOUR_WIFI_PASSWORD"  // 替换为您的WiFi密码
#endif

// ============================================================================
// MQTT 配置
// ============================================================================
// 注意：这些是默认值，设备注册成功后会使用注册获得的凭证
#ifndef DEFAULT_MQTT_BROKER
#define DEFAULT_MQTT_BROKER     ""                     // MQTT Broker地址（通过配网或注册获取）
#endif

#ifndef DEFAULT_MQTT_PORT
#define DEFAULT_MQTT_PORT       1883                  // MQTT Broker端口
#endif

#ifndef DEFAULT_MQTT_USERNAME
#define DEFAULT_MQTT_USERNAME   ""                    // 默认用户名（注册后会被device_uuid替换）
#endif

#ifndef DEFAULT_MQTT_PASSWORD
#define DEFAULT_MQTT_PASSWORD   ""                    // 默认密码（注册后会被device_secret替换）
#endif

// ============================================================================
// 设备配置
// ============================================================================
#ifndef DEFAULT_DEVICE_ID
#define DEFAULT_DEVICE_ID       CONFIG_AIOT_DEVICE_ID // 来自Kconfig
#endif

// ============================================================================
// 服务器配置
// ============================================================================
#define DEFAULT_SERVER_URL      ""                     // 后端服务器地址（通过配网获取）
#define DEFAULT_SERVER_PORT     8000                  // 后端服务器端口

// ============================================================================
// 传感器配置
// ============================================================================
#define DHT11_DATA_PIN          GPIO_NUM_35           // DHT11数据引脚
#define DS18B20_DATA_PIN        GPIO_NUM_39           // DS18B20数据引脚

// ============================================================================
// 产品信息（已从 DEVICE_CONFIG.h 导入）
// ============================================================================
// ⚠️ 注意：产品ID和固件版本已在 DEVICE_CONFIG.h 中定义
// 如需修改，请编辑 DEVICE_CONFIG.h 文件

// 产品代码定义 - 从Kconfig获取板子特定的产品代码
// Kconfig会根据板子类型自动设置：
// - ESP32-S3-DevKit → ESP32-S3-Dev-01
// - ESP32-S3-DevKit-Rain → ESP32-S3-Rain-01
// 注意：DEVICE_CONFIG.h中可能已经定义了PRODUCT_ID，需要先取消定义再重新定义
#ifdef PRODUCT_ID
    #undef PRODUCT_ID
#endif
#ifdef PRODUCT_CODE
    #undef PRODUCT_CODE
#endif

#ifdef CONFIG_AIOT_PRODUCT_CODE
    #define PRODUCT_ID              CONFIG_AIOT_PRODUCT_CODE  // 使用Kconfig配置的产品代码
    #define PRODUCT_CODE            CONFIG_AIOT_PRODUCT_CODE
#else
    #define PRODUCT_ID              "ESP32-S3-Dev-01"  // 默认产品代码（兼容旧版）
    #define PRODUCT_CODE            "ESP32-S3-Dev-01"
#endif

// 其他产品信息
#define PRODUCT_VERSION         "1.0"                 // 产品版本
#define MANUFACTURER            "Espressif"           // 制造商
#define MODEL                   "ESP32-S3-DevKitC"    // 型号

#ifdef __cplusplus
}
#endif

#endif // APP_CONFIG_H

