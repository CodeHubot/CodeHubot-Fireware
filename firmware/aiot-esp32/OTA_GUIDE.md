# ESP32 OTA 固件更新指南

## 📋 OTA流程概述

```
┌─────────────────────────────────────────────────────────────┐
│  1. 设备启动/定期检查                                         │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ↓
┌─────────────────────────────────────────────────────────────┐
│  2. 请求配置服务                                              │
│     GET /device/info?mac=AA:BB:CC:DD:EE:FF                   │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ↓
┌─────────────────────────────────────────────────────────────┐
│  3. 解析响应，检查 firmware_update 字段                       │
│     {                                                         │
│       "firmware_update": {                                    │
│         "available": true,                                    │
│         "version": "1.1.0",                                   │
│         "download_url": "http://ota.../v1.1.0.bin",          │
│         "file_size": 1048576,                                 │
│         "checksum": "sha256:abc123..."                        │
│       }                                                       │
│     }                                                         │
└────────────────────┬────────────────────────────────────────┘
                     │
         ┌───────────┴───────────┐
         │                       │
    available=true          available=false
         │                       │
         ↓                       ↓
    执行OTA更新              继续正常工作
         │
         ↓
┌─────────────────────────────────────────────────────────────┐
│  4. 下载固件                                                  │
│     从 download_url 下载 .bin 文件                            │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ↓
┌─────────────────────────────────────────────────────────────┐
│  5. 验证固件                                                  │
│     - 检查文件大小                                            │
│     - 验证 SHA256 校验和                                      │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ↓
┌─────────────────────────────────────────────────────────────┐
│  6. 写入固件分区                                              │
│     使用 ESP-IDF OTA API                                     │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ↓
┌─────────────────────────────────────────────────────────────┐
│  7. 设置启动分区并重启                                        │
│     esp_ota_set_boot_partition()                             │
│     esp_restart()                                            │
└────────────────────┬────────────────────────────────────────┘
                     │
                     ↓
┌─────────────────────────────────────────────────────────────┐
│  8. 重启后验证                                                │
│     - 检查新版本是否运行正常                                   │
│     - 如果成功: esp_ota_mark_app_valid_cancel_rollback()     │
│     - 如果失败: 自动回滚到旧版本                              │
└─────────────────────────────────────────────────────────────┘
```

## 📄 ESP32代码实现

### 1. 完整的OTA处理函数

```c
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "cJSON.h"

#define TAG "OTA"
#define OTA_BUFFER_SIZE 1024

/**
 * @brief OTA更新任务
 */
void ota_update_task(void *pvParameter) {
    const char *firmware_url = (const char *)pvParameter;
    
    ESP_LOGI(TAG, "🚀 开始OTA更新");
    ESP_LOGI(TAG, "📥 下载地址: %s", firmware_url);
    
    // 显示更新进度到LCD
    simple_display_show_info("固件更新中...", "请勿断电");
    
    esp_http_client_config_t config = {
        .url = firmware_url,
        .cert_pem = NULL,  // 如果使用HTTPS，需要添加证书
        .timeout_ms = 30000,
        .keep_alive_enable = true,
    };
    
    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };
    
    esp_err_t ret = esp_https_ota(&ota_config);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "✅ OTA更新成功，准备重启...");
        simple_display_show_info("更新成功", "重启中...");
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    } else {
        ESP_LOGE(TAG, "❌ OTA更新失败: %s", esp_err_to_name(ret));
        simple_display_show_info("更新失败", "继续使用旧版本");
    }
    
    vTaskDelete(NULL);
}

/**
 * @brief 处理配置响应中的OTA信息
 */
esp_err_t handle_ota_update(cJSON *firmware_update) {
    if (!firmware_update || cJSON_IsNull(firmware_update)) {
        return ESP_OK;  // 无更新可用
    }
    
    cJSON *available = cJSON_GetObjectItem(firmware_update, "available");
    if (!available || !cJSON_IsTrue(available)) {
        ESP_LOGI(TAG, "✅ 固件已是最新版本");
        return ESP_OK;
    }
    
    // 提取更新信息
    cJSON *version = cJSON_GetObjectItem(firmware_update, "version");
    cJSON *url = cJSON_GetObjectItem(firmware_update, "download_url");
    cJSON *size = cJSON_GetObjectItem(firmware_update, "file_size");
    cJSON *checksum = cJSON_GetObjectItem(firmware_update, "checksum");
    cJSON *changelog = cJSON_GetObjectItem(firmware_update, "changelog");
    
    if (!url || !cJSON_IsString(url)) {
        ESP_LOGE(TAG, "❌ 固件更新URL无效");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "⚠️ 发现固件更新！");
    ESP_LOGI(TAG, "   新版本: %s", version ? version->valuestring : "未知");
    ESP_LOGI(TAG, "   文件大小: %d 字节", size ? size->valueint : 0);
    ESP_LOGI(TAG, "   更新内容: %s", changelog ? changelog->valuestring : "无");
    ESP_LOGI(TAG, "   下载地址: %s", url->valuestring);
    
    // 询问用户或自动更新
    // 方式1: 自动更新（推荐用于无人值守设备）
    ESP_LOGI(TAG, "⏰ 3秒后开始自动更新...");
    vTaskDelay(pdMS_TO_TICKS(3000));
    
    // 方式2: 等待按钮确认
    // ESP_LOGI(TAG, "请按按钮确认更新");
    // wait_for_button_press();
    
    // 启动OTA任务
    char *url_copy = strdup(url->valuestring);
    xTaskCreate(
        ota_update_task,
        "ota_task",
        8192,  // 栈大小
        url_copy,
        5,     // 优先级
        NULL
    );
    
    return ESP_OK;
}

/**
 * @brief 在主程序中调用
 */
void app_main(void) {
    // ... WiFi连接等初始化代码 ...
    
    // 获取配置
    char response[4096];
    char url[256];
    snprintf(url, sizeof(url), "%s/device/info?mac=%s", 
             server_address, mac_address);
    
    if (http_get_request(url, response, sizeof(response)) == ESP_OK) {
        // 解析响应
        cJSON *root = cJSON_Parse(response);
        if (root) {
            // 提取MQTT配置等
            // ...
            
            // 检查并处理OTA
            cJSON *firmware_update = cJSON_GetObjectItem(root, "firmware_update");
            handle_ota_update(firmware_update);
            
            cJSON_Delete(root);
        }
    }
    
    // 继续正常运行
    // ...
}
```

### 2. 带进度显示的高级OTA

```c
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"

typedef struct {
    int total_size;
    int downloaded_size;
    int progress;
} ota_progress_t;

static ota_progress_t ota_progress = {0};

/**
 * @brief HTTP事件处理（显示下载进度）
 */
static esp_err_t ota_http_event_handler(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ON_HEADER:
            if (strcasecmp(evt->header_key, "Content-Length") == 0) {
                ota_progress.total_size = atoi(evt->header_value);
                ESP_LOGI(TAG, "📦 固件大小: %d 字节", ota_progress.total_size);
            }
            break;
            
        case HTTP_EVENT_ON_DATA:
            ota_progress.downloaded_size += evt->data_len;
            if (ota_progress.total_size > 0) {
                int new_progress = (ota_progress.downloaded_size * 100) / ota_progress.total_size;
                if (new_progress != ota_progress.progress) {
                    ota_progress.progress = new_progress;
                    ESP_LOGI(TAG, "📥 下载进度: %d%%", ota_progress.progress);
                    
                    // 更新LCD显示
                    char progress_str[32];
                    snprintf(progress_str, sizeof(progress_str), "下载中 %d%%", ota_progress.progress);
                    simple_display_update_status(progress_str);
                }
            }
            break;
            
        default:
            break;
    }
    return ESP_OK;
}

/**
 * @brief 高级OTA更新（带进度）
 */
esp_err_t ota_update_with_progress(const char *url) {
    memset(&ota_progress, 0, sizeof(ota_progress));
    
    ESP_LOGI(TAG, "🚀 开始OTA更新");
    ESP_LOGI(TAG, "📥 URL: %s", url);
    
    esp_http_client_config_t config = {
        .url = url,
        .event_handler = ota_http_event_handler,
        .timeout_ms = 30000,
        .buffer_size = 1024,
        .buffer_size_tx = 1024,
    };
    
    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };
    
    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ OTA开始失败: %s", esp_err_to_name(err));
        return err;
    }
    
    // 逐块下载并写入
    while (1) {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
    }
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "✅ 下载完成，验证固件...");
        err = esp_https_ota_finish(https_ota_handle);
        
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "✅ OTA更新成功，准备重启...");
            simple_display_show_info("更新成功", "重启中...");
            vTaskDelay(pdMS_TO_TICKS(2000));
            esp_restart();
        } else {
            ESP_LOGE(TAG, "❌ OTA验证失败: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "❌ OTA下载失败: %s", esp_err_to_name(err));
        esp_https_ota_abort(https_ota_handle);
    }
    
    return err;
}
```

### 3. 定期检查更新

```c
/**
 * @brief 定期检查固件更新任务
 */
void firmware_check_task(void *pvParameters) {
    const uint32_t CHECK_INTERVAL_MS = 24 * 60 * 60 * 1000;  // 每天检查一次
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(CHECK_INTERVAL_MS));
        
        ESP_LOGI(TAG, "🔍 定期检查固件更新...");
        
        // 获取配置（包含固件更新信息）
        char response[4096];
        char url[256];
        snprintf(url, sizeof(url), "%s/device/info?mac=%s&firmware_version=%s",
                 server_address, mac_address, FIRMWARE_VERSION);
        
        if (http_get_request(url, response, sizeof(response)) == ESP_OK) {
            cJSON *root = cJSON_Parse(response);
            if (root) {
                cJSON *firmware_update = cJSON_GetObjectItem(root, "firmware_update");
                handle_ota_update(firmware_update);
                cJSON_Delete(root);
            }
        }
    }
}

// 在app_main中启动
xTaskCreate(firmware_check_task, "fw_check", 4096, NULL, 5, NULL);
```

## 🔐 安全建议

### 1. 验证固件签名

```c
// TODO: 添加固件签名验证
// 使用ESP32的Secure Boot功能
```

### 2. HTTPS下载

```c
// 使用HTTPS确保固件传输安全
esp_http_client_config_t config = {
    .url = "https://ota.example.com/firmware/v1.1.0.bin",
    .cert_pem = (char *)server_cert_pem_start,  // 服务器证书
    .timeout_ms = 30000,
};
```

### 3. 校验和验证

```c
#include "mbedtls/sha256.h"

bool verify_firmware_checksum(const uint8_t *data, size_t len, const char *expected_checksum) {
    unsigned char sha256[32];
    mbedtls_sha256_context ctx;
    
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, data, len);
    mbedtls_sha256_finish(&ctx, sha256);
    mbedtls_sha256_free(&ctx);
    
    // 转换为hex字符串并比较
    char hex_str[65];
    for (int i = 0; i < 32; i++) {
        sprintf(&hex_str[i*2], "%02x", sha256[i]);
    }
    
    return (strcmp(hex_str, expected_checksum) == 0);
}
```

## 📊 OTA配置

### partition_table.csv

确保分区表支持OTA：

```csv
# Name,     Type, SubType, Offset,   Size,    Flags
nvs,        data, nvs,     0x9000,   0x6000,
phy_init,   data, phy,     0xf000,   0x1000,
factory,    app,  factory, 0x10000,  0x200000,
ota_0,      app,  ota_0,   0x210000, 0x200000,
ota_1,      app,  ota_1,   0x410000, 0x200000,
ota_data,   data, ota,     0x610000, 0x2000,
```

### menuconfig配置

```bash
idf.py menuconfig

# 导航到:
Component config → 
  ESP HTTPS OTA →
    [*] Enable
    
Security features →
  [*] Enable OTA rollback
  [*] Enable OTA secure version
```

## 🧪 测试

### 1. 本地测试服务器

```bash
# 启动简单HTTP服务器
cd firmware/build
python3 -m http.server 8080

# 固件URL: http://192.168.1.100:8080/aiot-esp32s3-firmware.bin
```

### 2. 模拟固件更新

在配置服务的数据库中添加固件记录：

```sql
INSERT INTO firmware_versions (
    version, 
    product_id, 
    download_url, 
    changelog, 
    file_size, 
    checksum, 
    is_active
) VALUES (
    '1.1.0',
    1,
    'http://192.168.1.100:8080/aiot-esp32s3-firmware.bin',
    '修复LED闪烁bug，优化MQTT连接',
    1048576,
    'sha256:abc123...',
    true
);
```

## 📝 最佳实践

### 1. 版本管理

```c
// 在固件中定义版本号
#define FIRMWARE_VERSION "1.0.0"

// 编译时显示
ESP_LOGI(TAG, "固件版本: %s", FIRMWARE_VERSION);
ESP_LOGI(TAG, "编译时间: %s %s", __DATE__, __TIME__);
```

### 2. 回滚机制

```c
void app_main(void) {
    // 检查是否是OTA更新后首次启动
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGI(TAG, "⚠️ OTA更新后首次启动，验证中...");
            
            // 运行自检
            bool self_test_passed = run_self_test();
            
            if (self_test_passed) {
                ESP_LOGI(TAG, "✅ 自检通过，标记新固件为有效");
                esp_ota_mark_app_valid_cancel_rollback();
            } else {
                ESP_LOGE(TAG, "❌ 自检失败，将回滚到旧版本");
                esp_ota_mark_app_invalid_rollback_and_reboot();
            }
        }
    }
    
    // 继续正常初始化
    // ...
}

bool run_self_test(void) {
    // 测试关键功能
    bool wifi_ok = test_wifi_connection();
    bool mqtt_ok = test_mqtt_connection();
    bool sensors_ok = test_sensors();
    
    return wifi_ok && mqtt_ok && sensors_ok;
}
```

### 3. OTA日志记录

```c
// 记录OTA历史
typedef struct {
    char from_version[32];
    char to_version[32];
    uint32_t timestamp;
    bool success;
} ota_history_t;

void save_ota_history(const char *from, const char *to, bool success) {
    ota_history_t history = {
        .timestamp = (uint32_t)time(NULL),
        .success = success
    };
    strncpy(history.from_version, from, sizeof(history.from_version) - 1);
    strncpy(history.to_version, to, sizeof(history.to_version) - 1);
    
    // 保存到NVS
    nvs_handle_t nvs_handle;
    nvs_open("ota_history", NVS_READWRITE, &nvs_handle);
    nvs_set_blob(nvs_handle, "last_ota", &history, sizeof(history));
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
}
```

## 🚨 故障排查

### 问题1: OTA下载失败

```
错误: ESP_ERR_HTTP_EAGAIN
原因: 网络不稳定
解决: 增加超时时间，添加重试机制
```

### 问题2: 固件验证失败

```
错误: ESP_ERR_OTA_VALIDATE_FAILED
原因: 固件损坏或不兼容
解决: 检查固件完整性，确保架构匹配
```

### 问题3: 分区空间不足

```
错误: ESP_ERR_NO_MEM
原因: OTA分区太小
解决: 修改partition_table.csv，增大ota_0/ota_1分区
```

---

**文档版本**: v1.0.0  
**最后更新**: 2025-11-06  
**维护者**: AIOT团队

