# ESP32设备API接口参考

本文档列出ESP32设备需要访问的所有服务器接口。

## 📋 目录

1. [配置服务接口](#1-配置服务接口-必须)
2. [MQTT通信](#2-mqtt通信-必须)
3. [HTTP API接口](#3-http-api接口-可选)
4. [固件更新](#4-固件更新)
5. [完整启动流程](#5-完整启动流程)

---

## 1. 配置服务接口 (必须)

### 1.1 获取设备配置

**这是设备启动后必须首先调用的接口！**

```http
POST /device/info
Content-Type: application/json
Host: {配置服务器地址}
```

**请求体**:
```json
{
  "mac_address": "AA:BB:CC:DD:EE:FF",
  "firmware_version": "1.0.0",
  "hardware_version": "ESP32-S3"
}
```

**成功响应** (200 OK):
```json
{
  "device_id": "AIOT-ESP32-12345678",
  "device_uuid": "550e8400-e29b-41d4-a716-446655440000",
  "device_secret": "abc123def456...",
  "mac_address": "AA:BB:CC:DD:EE:FF",
  "mqtt_config": {
    "broker": "mqtt.example.com",
    "port": 1883,
    "use_ssl": false,
    "url": "mqtt://mqtt.example.com:1883",
    "username": "AIOT-ESP32-12345678",
    "password": "abc123def456...",
    "client_id": "550e8400-e29b-41d4-a716-446655440000",
    "topics": {
      "data": "devices/550e8400-e29b-41d4-a716-446655440000/data",
      "control": "devices/550e8400-e29b-41d4-a716-446655440000/control",
      "status": "devices/550e8400-e29b-41d4-a716-446655440000/status"
    }
  },
  "api_config": {
    "server": "http://api.example.com",
    "endpoints": {
      "register": "http://api.example.com/api/devices/register",
      "data_upload": "http://api.example.com/api/devices/data/upload",
      "status_update": "http://api.example.com/api/devices/status/update"
    }
  },
  "firmware_update": {
    "available": true,
    "version": "1.1.0",
    "download_url": "http://ota.example.com/firmware/1.1.0.bin",
    "file_size": 1048576,
    "checksum": "sha256:abc123...",
    "changelog": "修复了一些bug"
  },
  "message": "设备配置获取成功",
  "timestamp": "2025-11-06T12:00:00.123456"
}
```

**错误响应**:
- `404 Not Found`: 设备未注册
- `429 Too Many Requests`: 请求过于频繁
- `422 Unprocessable Entity`: 请求格式错误

**ESP32代码示例**:
```c
#include "provisioning/provisioning_client.h"

// 1. 读取配置服务器地址（从NVS）
unified_server_config_t server_config;
server_config_load_from_nvs(&server_config);

// 2. 获取MAC地址
uint8_t mac[6];
esp_efuse_mac_get_default(mac);
char mac_str[18];
snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
         mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

// 3. 请求设备配置
device_config_t device_config;
esp_err_t ret = provisioning_client_get_config(
    server_config.base_address,  // http://provision.example.com
    mac_str,                      // AA:BB:CC:DD:EE:FF
    FIRMWARE_VERSION,             // "1.0.0"
    &device_config
);

if (ret == ESP_OK) {
    // 4. 保存配置到NVS（缓存）
    provisioning_client_save_config(&device_config);
    
    // 5. 使用配置连接MQTT
    // device_config.mqtt.broker
    // device_config.mqtt.port
    // device_config.mqtt.username
    // device_config.mqtt.password
}
```

**调用时机**:
- ✅ 首次启动
- ✅ 配网后重启
- ✅ 从NVS读取配置失败
- ✅ 定期刷新配置（推荐每天一次）

**配置服务器地址来源**:
```
用户在配网页面输入 → 保存到NVS(server_config/base_address) → 设备读取
```

---

### 1.2 检查固件更新

**可选接口，用于定期检查固件更新**

```http
POST /firmware/check
Content-Type: application/json
Host: {配置服务器地址}
```

**请求体**:
```json
{
  "mac_address": "AA:BB:CC:DD:EE:FF",
  "current_version": "1.0.0",
  "product_id": 1
}
```

**成功响应** (200 OK):
```json
{
  "update_available": true,
  "current_version": "1.0.0",
  "latest_version": "1.1.0",
  "download_url": "http://ota.example.com/firmware/1.1.0.bin",
  "file_size": 1048576,
  "checksum": "sha256:abc123...",
  "changelog": "修复了一些bug",
  "message": "有新版本可用"
}
```

**调用时机**:
- 每天检查一次
- 用户手动触发检查

---

## 2. MQTT通信 (必须)

### 2.1 连接配置

从 `/device/info` 接口获取MQTT配置后：

```c
mqtt_config = {
    .broker = device_config.mqtt.broker,        // "mqtt.example.com"
    .port = device_config.mqtt.port,            // 1883
    .username = device_config.mqtt.username,    // "AIOT-ESP32-12345678"
    .password = device_config.mqtt.password,    // "abc123..."
    .client_id = device_config.mqtt.client_id   // "550e8400-..."
};
```

### 2.2 发布数据

**主题**: `devices/{device_uuid}/data`

**QoS**: 1 (至少一次)

**消息格式**:
```json
{
  "sensor": "DHT11",
  "temperature": 25.5,
  "humidity": 60.0,
  "timestamp": "2025-11-06T12:00:00"
}
```

**ESP32代码示例**:
```c
// 构造JSON数据
cJSON *root = cJSON_CreateObject();
cJSON_AddStringToObject(root, "sensor", "DHT11");
cJSON_AddNumberToObject(root, "temperature", 25.5);
cJSON_AddNumberToObject(root, "humidity", 60.0);

char *json_str = cJSON_PrintUnformatted(root);

// 发布到MQTT
esp_mqtt_client_publish(
    mqtt_client,
    device_config.mqtt.topic_data,  // "devices/xxx/data"
    json_str,
    strlen(json_str),
    1,  // QoS 1
    0   // retain
);

cJSON_Delete(root);
free(json_str);
```

**发送频率**:
- 传感器数据: 每10秒一次
- 状态数据: 每分钟一次
- 事件数据: 立即发送

---

### 2.3 接收控制命令

**订阅主题**: `devices/{device_uuid}/control`

**QoS**: 1 (至少一次)

**LED控制**:
```json
{
  "cmd": "led",
  "device_id": 1,
  "action": "on"
}
```

**继电器控制**:
```json
{
  "cmd": "relay",
  "device_id": 1,
  "action": "on"
}
```

**舵机控制**:
```json
{
  "cmd": "servo",
  "device_id": 1,
  "angle": 90
}
```

**ESP32代码示例**:
```c
static void mqtt_event_handler(void *handler_args, 
                               esp_event_base_t base,
                               int32_t event_id, 
                               void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    
    switch (event->event_id) {
        case MQTT_EVENT_DATA:
            // 解析控制命令
            cJSON *root = cJSON_Parse(event->data);
            if (root) {
                cJSON *cmd = cJSON_GetObjectItem(root, "cmd");
                
                if (strcmp(cmd->valuestring, "led") == 0) {
                    // 处理LED控制
                    cJSON *device_id = cJSON_GetObjectItem(root, "device_id");
                    cJSON *action = cJSON_GetObjectItem(root, "action");
                    
                    led_control(device_id->valueint, 
                               strcmp(action->valuestring, "on") == 0);
                }
                
                cJSON_Delete(root);
            }
            break;
    }
}

// 订阅控制主题
esp_mqtt_client_subscribe(
    mqtt_client,
    device_config.mqtt.topic_control,  // "devices/xxx/control"
    1  // QoS 1
);
```

---

### 2.4 发布状态

**主题**: `devices/{device_uuid}/status`

**QoS**: 1

**消息格式**:
```json
{
  "online": true,
  "ip_address": "192.168.1.100",
  "rssi": -65,
  "free_heap": 150000,
  "uptime": 3600
}
```

**发送频率**: 每分钟一次

---

## 3. HTTP API接口 (可选)

以下接口是可选的，通常使用MQTT通信即可，HTTP接口作为备用。

### 3.1 设备数据上传

```http
POST /api/devices/data/upload
Content-Type: application/json
```

**请求体**:
```json
{
  "device_id": "AIOT-ESP32-12345678",
  "device_secret": "abc123...",
  "sensors": {
    "temperature": 25.5,
    "humidity": 60.0
  },
  "status": {
    "ip_address": "192.168.1.100"
  }
}
```

### 3.2 设备状态更新

```http
POST /api/devices/status/update
Content-Type: application/json
```

**请求体**:
```json
{
  "device_id": "AIOT-ESP32-12345678",
  "device_secret": "abc123...",
  "status": {
    "online": true,
    "ip_address": "192.168.1.100",
    "rssi": -65
  }
}
```

---

## 4. 固件更新

### 4.1 下载固件

从 `/device/info` 或 `/firmware/check` 响应中获取下载URL：

```c
if (device_config.firmware_update_available) {
    ESP_LOGI(TAG, "开始OTA更新");
    ESP_LOGI(TAG, "URL: %s", device_config.firmware_url);
    ESP_LOGI(TAG, "大小: %u bytes", device_config.firmware_size);
    
    // 使用ESP-IDF OTA API下载并安装
    esp_http_client_config_t config = {
        .url = device_config.firmware_url,
        .cert_pem = NULL,  // 如果使用HTTPS需要证书
    };
    
    esp_err_t ret = esp_https_ota(&config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA成功，重启...");
        esp_restart();
    }
}
```

---

## 5. 完整启动流程

```
┌─────────────────────────────────────────────────┐
│  1. 设备上电                                      │
└────────────────┬────────────────────────────────┘
                 │
                 ↓
┌─────────────────────────────────────────────────┐
│  2. 初始化硬件                                    │
│     - NVS初始化                                  │
│     - WiFi初始化                                 │
│     - 传感器初始化                               │
└────────────────┬────────────────────────────────┘
                 │
                 ↓
┌─────────────────────────────────────────────────┐
│  3. 检查WiFi配置                                 │
│     从NVS读取WiFi SSID和密码                      │
└────────────────┬────────────────────────────────┘
                 │
         ┌───────┴───────┐
         │               │
    有配置              无配置
         │               │
         ↓               ↓
    连接WiFi        进入配网模式
         │          (AP + Captive Portal)
         │               │
         │               ↓
         │          等待用户配置
         │               │
         │               ↓
         │          保存配置，重启
         │               │
         └───────┬───────┘
                 │
                 ↓
┌─────────────────────────────────────────────────┐
│  4. WiFi连接成功                                 │
│     获取IP地址                                   │
└────────────────┬────────────────────────────────┘
                 │
                 ↓
┌─────────────────────────────────────────────────┐
│  5. 请求设备配置                                 │
│     POST {provision_server}/device/info          │
│     携带: MAC地址、固件版本                       │
└────────────────┬────────────────────────────────┘
                 │
         ┌───────┴───────┐
         │               │
     成功(200)        失败(404/429/500)
         │               │
         │               ↓
         │          尝试从NVS加载缓存配置
         │               │
         │       ┌───────┴───────┐
         │       │               │
         │   有缓存            无缓存
         │       │               │
         │       │               ↓
         │       │          显示错误，等待重试
         │       │          或进入配网模式
         └───────┴───────┐
                 │
                 ↓
┌─────────────────────────────────────────────────┐
│  6. 保存配置到NVS                                │
│     缓存UUID、Secret、MQTT配置                    │
└────────────────┬────────────────────────────────┘
                 │
                 ↓
┌─────────────────────────────────────────────────┐
│  7. 连接MQTT服务器                               │
│     使用配置中的broker、port、username、password  │
└────────────────┬────────────────────────────────┘
                 │
                 ↓
┌─────────────────────────────────────────────────┐
│  8. 订阅控制主题                                 │
│     devices/{uuid}/control                       │
└────────────────┬────────────────────────────────┘
                 │
                 ↓
┌─────────────────────────────────────────────────┐
│  9. 进入工作循环                                 │
│     - 定时采集传感器数据                         │
│     - 通过MQTT发布到data主题                     │
│     - 监听control主题接收命令                    │
│     - 执行控制命令（LED、继电器、舵机）           │
│     - 定期发送状态信息                           │
│     - 定期检查固件更新                           │
└─────────────────────────────────────────────────┘
```

---

## 6. 错误处理

### 6.1 配置服务不可用

```c
esp_err_t ret = provisioning_client_get_config(...);

if (ret != ESP_OK) {
    // 尝试从NVS加载缓存配置
    ret = provisioning_client_load_config(&device_config);
    
    if (ret == ESP_OK && device_config.valid) {
        ESP_LOGW(TAG, "配置服务不可用，使用缓存配置");
        // 使用缓存配置继续工作
    } else {
        ESP_LOGE(TAG, "无可用配置");
        // 选项1: 定期重试
        // 选项2: 进入配网模式
        // 选项3: 显示错误状态
    }
}
```

### 6.2 MQTT连接失败

```c
// 实现重连机制
void mqtt_reconnect_task(void *pvParameters) {
    while (1) {
        if (!mqtt_is_connected()) {
            ESP_LOGW(TAG, "MQTT disconnected, reconnecting...");
            mqtt_client_connect();
        }
        vTaskDelay(pdMS_TO_TICKS(5000));  // 每5秒检查一次
    }
}
```

### 6.3 网络断开

```c
// WiFi事件处理
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi disconnected, reconnecting...");
        esp_wifi_connect();
    }
}
```

---

## 7. 安全建议

1. **使用HTTPS**: 生产环境配置服务应使用HTTPS
2. **证书验证**: 启用TLS证书验证
3. **密钥保护**: Device Secret不要打印到日志
4. **速率限制**: 遵守配置服务的速率限制
5. **错误重试**: 实现指数退避重试策略

---

**文档版本**: v1.0.0  
**最后更新**: 2025-11-06  
**维护者**: AIOT团队

