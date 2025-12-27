# OTA管理器集成指南

## 📌 概述

基于xiaozhi-esp32项目的OTA实现，我们创建了简化的C语言版本OTA管理器。

**参考项目**: https://github.com/78/xiaozhi-esp32

## 🎯 核心特性

✅ **参考xiaozhi的核心逻辑**:
- `CheckVersion()` - 检查版本并获取固件信息
- `StartUpgrade()` - 流式下载和写入固件
- `MarkCurrentVersionValid()` - 标记新版本有效
- `IsNewVersionAvailable()` - 版本号对比

✅ **简化实现**:
- 纯C语言实现（xiaozhi用C++）
- 使用GET请求（更简单）
- 支持进度回调
- 自动回滚机制

## 📁 文件结构

```
firmware/aiot-esp32/main/
├── ota/
│   ├── ota_manager.h      # OTA管理器头文件
│   └── ota_manager.c      # OTA管理器实现（参考xiaozhi）
```

## 🔌 在main.c中集成

### 1. 完整示例

```c
#include "ota/ota_manager.h"
#include "esp_log.h"
#include "esp_system.h"

#define TAG "MAIN"
#define FIRMWARE_VERSION "1.0.0"  // 定义当前固件版本

// OTA进度回调（可选）
void ota_progress_handler(int progress, size_t speed) {
    ESP_LOGI(TAG, "OTA进度: %d%%, 速度: %u B/s", progress, speed);
    
    // 可以更新LCD显示
    char status[64];
    snprintf(status, sizeof(status), "更新中 %d%%", progress);
    simple_display_update_status(status);
}

void app_main(void) {
    ESP_LOGI(TAG, "========== 系统启动 ==========");
    ESP_LOGI(TAG, "固件版本: %s", FIRMWARE_VERSION);
    
    // 1. 初始化OTA管理器
    ota_manager_init();
    
    // 2. 标记当前固件为有效（参考xiaozhi的MarkCurrentVersionValid）
    //    防止OTA更新后自动回滚
    ota_manager_mark_valid();
    
    // 3. 初始化其他模块
    // ... WiFi, NVS, LCD, 传感器等 ...
    
    // 4. WiFi连接成功后，检查固件更新
    if (wifi_is_connected()) {
        // 获取配置服务器地址
        unified_server_config_t server_config;
        if (server_config_load_from_nvs(&server_config) == ESP_OK) {
            // 获取MAC地址
            uint8_t mac[6];
            esp_efuse_mac_get_default(mac);
            char mac_str[18];
            snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                     mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
            
            // 检查固件更新（参考xiaozhi的CheckVersion）
            firmware_info_t fw_info;
            esp_err_t ret = ota_manager_check_version(
                server_config.base_address,  // http://provision.example.com
                mac_str,                      // AA:BB:CC:DD:EE:FF
                FIRMWARE_VERSION,             // 1.0.0
                &fw_info
            );
            
            if (ret == ESP_OK && fw_info.available) {
                ESP_LOGI(TAG, "⚠️ 发现固件更新: %s -> %s", 
                         FIRMWARE_VERSION, fw_info.version);
                ESP_LOGI(TAG, "   更新内容: %s", fw_info.changelog);
                
                // 决定是否更新
                // 选项1: 立即更新
                ESP_LOGI(TAG, "⏰ 3秒后开始自动更新...");
                vTaskDelay(pdMS_TO_TICKS(3000));
                
                // 选项2: 凌晨更新
                // if (current_hour == 3) { ... }
                
                // 选项3: 用户确认
                // if (wait_button_press(60000)) { ... }
                
                // 开始OTA升级（参考xiaozhi的StartUpgrade）
                simple_display_show_info("固件更新中", "请勿断电");
                
                ret = ota_manager_start_upgrade(
                    fw_info.download_url,
                    ota_progress_handler  // 进度回调
                );
                
                if (ret == ESP_OK) {
                    ESP_LOGI(TAG, "✅ OTA升级成功，准备重启...");
                    simple_display_show_info("更新成功", "重启中...");
                    vTaskDelay(pdMS_TO_TICKS(2000));
                    esp_restart();
                } else {
                    ESP_LOGE(TAG, "❌ OTA升级失败");
                    simple_display_show_info("更新失败", "继续运行");
                }
            } else if (ret == ESP_OK && !fw_info.available) {
                ESP_LOGI(TAG, "✅ 固件已是最新版本");
            } else {
                ESP_LOGW(TAG, "⚠️ 固件检查失败");
            }
        }
    }
    
    // 5. 继续正常运行
    // ... 启动MQTT、传感器任务等 ...
    
    ESP_LOGI(TAG, "========== 系统启动完成 ==========");
}
```

### 2. 定期检查更新

```c
/**
 * @brief 定期检查固件更新任务
 * 参考xiaozhi，每24小时检查一次
 */
void firmware_check_task(void *pvParameters) {
    const uint32_t CHECK_INTERVAL_MS = 24 * 60 * 60 * 1000;  // 24小时
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(CHECK_INTERVAL_MS));
        
        ESP_LOGI(TAG, "🔍 定期检查固件更新...");
        
        // 获取配置服务器地址
        unified_server_config_t server_config;
        if (server_config_load_from_nvs(&server_config) != ESP_OK) {
            continue;
        }
        
        // 获取MAC地址
        uint8_t mac[6];
        esp_efuse_mac_get_default(mac);
        char mac_str[18];
        snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        
        // 检查更新
        firmware_info_t fw_info;
        if (ota_manager_check_version(server_config.base_address, mac_str,
                                      FIRMWARE_VERSION, &fw_info) == ESP_OK) {
            if (fw_info.available) {
                ESP_LOGI(TAG, "发现新版本: %s", fw_info.version);
                // 执行更新...
            }
        }
    }
}

// 在app_main中启动任务
xTaskCreate(firmware_check_task, "fw_check", 4096, NULL, 5, NULL);
```

## 🔄 OTA流程（参考xiaozhi）

```
1. CheckVersion (检查版本)
   ↓
   GET /device/info?mac=AA:BB:CC&firmware_version=1.0.0
   ↓
   响应包含 firmware_update 字段
   ↓
2. IsNewVersionAvailable (比较版本)
   ↓
   1.1.0 > 1.0.0 ?
   ↓
3. StartUpgrade (开始升级)
   ↓
   流式下载固件 (逐块读取，边下边写)
   ↓
   验证固件头
   ↓
   写入OTA分区
   ↓
   设置启动分区
   ↓
4. esp_restart() (重启)
   ↓
5. MarkCurrentVersionValid (首次启动标记有效)
   ↓
   运行自检
   ↓
   标记为有效 (防止回滚)
```

## 📊 版本号规则（参考xiaozhi）

版本号格式: `主版本.次版本.修订号`

```
1.0.0 < 1.0.1  (修订号增加)
1.0.9 < 1.1.0  (次版本增加)
1.9.9 < 2.0.0  (主版本增加)
```

比较规则（参考xiaozhi的ParseVersion和IsNewVersionAvailable）:
1. 先比较主版本
2. 再比较次版本
3. 最后比较修订号

## 🎨 LCD显示集成

```c
void ota_progress_handler(int progress, size_t speed) {
    // 更新LCD显示
    char line1[32];
    char line2[32];
    
    snprintf(line1, sizeof(line1), "固件更新中");
    snprintf(line2, sizeof(line2), "进度: %d%% (%uKB/s)", progress, speed / 1024);
    
    simple_display_show_info(line1, line2);
}
```

## 🔐 安全建议

### 1. 使用HTTPS（生产环境）

```c
esp_http_client_config_t config = {
    .url = firmware_url,
    .cert_pem = (char *)server_cert_pem_start,  // 服务器证书
    .timeout_ms = 30000,
};
```

### 2. 自检机制（参考xiaozhi）

```c
bool run_self_test(void) {
    // 测试关键功能
    bool wifi_ok = test_wifi_connection();
    bool mqtt_ok = test_mqtt_connection();
    bool sensors_ok = test_sensors();
    
    return wifi_ok && mqtt_ok && sensors_ok;
}

void app_main(void) {
    // OTA更新后首次启动
    ota_manager_mark_valid();  // 如果自检失败会自动回滚
    
    if (!run_self_test()) {
        ESP_LOGE(TAG, "❌ 自检失败，系统将回滚到旧版本");
        esp_ota_mark_app_invalid_rollback_and_reboot();
    }
}
```

### 3. 校验和验证

```c
// TODO: 添加SHA256校验和验证
// 参考xiaozhi的固件验证逻辑
```

## 📝 CMakeLists.txt配置

在 `main/CMakeLists.txt` 中添加OTA管理器：

```cmake
set(COMPONENT_SRCS 
    # ... 其他文件 ...
    "ota/ota_manager.c"
)

set(COMPONENT_ADD_INCLUDEDIRS 
    # ... 其他目录 ...
    "ota"
)

idf_component_register(
    SRCS ${COMPONENT_SRCS}
    INCLUDE_DIRS ${COMPONENT_ADD_INCLUDEDIRS}
    REQUIRES 
        esp_http_client
        esp_https_ota
        json
        # ... 其他依赖 ...
)
```

## 🧪 测试流程

### 1. 编译新版本固件

```bash
# 修改版本号
vim main/main.c
# #define FIRMWARE_VERSION "1.1.0"

# 编译
cd firmware/aiot-esp32
idf.py build

# 固件位置
# build/aiot-esp32s3-firmware.bin
```

### 2. 上传固件到服务器

```bash
# 计算SHA256
sha256sum build/aiot-esp32s3-firmware.bin

# 上传到OTA服务器
scp build/aiot-esp32s3-firmware.bin user@server:/var/www/ota/v1.1.0.bin
```

### 3. 在数据库添加固件记录

```sql
INSERT INTO firmware_versions (
    version,
    product_id,
    download_url,
    file_size,
    checksum,
    changelog,
    is_active
) VALUES (
    '1.1.0',
    1,
    'http://ota.example.com/firmware/v1.1.0.bin',
    1048576,
    'sha256:e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855',
    '修复LED闪烁bug，优化MQTT连接',
    true
);
```

### 4. 设备测试

- 设备重启
- 连接WiFi
- 自动检查更新
- 显示更新进度
- 下载并安装
- 重启验证

## 🔍 调试日志示例

```
I (5123) MAIN: ========== 系统启动 ==========
I (5124) MAIN: 固件版本: 1.0.0
I (5125) OTA_MANAGER: OTA管理器初始化
I (5130) OTA_MANAGER: 当前运行分区: ota_0
I (5135) OTA_MANAGER: ✅ 标记新固件为有效
I (8456) OTA_MANAGER: 🔍 检查固件版本: http://provision.example.com/device/info?mac=...
I (8789) OTA_MANAGER: HTTP状态码: 200
I (8790) OTA_MANAGER: ⚠️ 发现固件更新:
I (8791) OTA_MANAGER:    版本: 1.1.0
I (8792) OTA_MANAGER:    大小: 1048576 字节
I (8793) OTA_MANAGER:    URL: http://ota.example.com/firmware/v1.1.0.bin
I (11234) MAIN: ⏰ 3秒后开始自动更新...
I (14234) OTA_MANAGER: 🚀 开始OTA升级
I (14235) OTA_MANAGER: 📥 固件URL: http://ota.example.com/firmware/v1.1.0.bin
I (14567) OTA_MANAGER: 写入分区: ota_1 (地址: 0x410000)
I (14890) OTA_MANAGER: 📦 固件大小: 1048576 字节
I (15234) OTA_MANAGER: 开始OTA写入...
I (16234) OTA_MANAGER: 📥 进度: 10% (102400/1048576), 速度: 102400B/s
I (17234) OTA_MANAGER: 📥 进度: 20% (204800/1048576), 速度: 102400B/s
...
I (24234) OTA_MANAGER: 📥 进度: 100% (1048576/1048576), 速度: 102400B/s
I (24567) OTA_MANAGER: 📥 下载完成，总共: 1048576 字节
I (24789) OTA_MANAGER: ✅ OTA升级成功！
I (24890) MAIN: ✅ OTA升级成功，准备重启...
```

## 🆚 xiaozhi vs 我们的实现

| 特性 | xiaozhi | 我们的实现 |
|------|---------|-----------|
| 语言 | C++ | C |
| 网络库 | 自定义Http类 | esp_http_client |
| 版本检查 | POST/GET | GET（更简单） |
| 进度显示 | 回调函数 | 回调函数 |
| 自动回滚 | ✅ | ✅ |
| 流式下载 | ✅ | ✅ |
| MQTT配置 | ✅ 自动保存 | ⚠️ 手动处理 |
| 激活机制 | ✅ HMAC验证 | ❌ 不支持 |

## 📚 相关文档

- [xiaozhi-esp32 OTA实现](https://github.com/78/xiaozhi-esp32/blob/main/main/ota.cc)
- [ESP-IDF OTA文档](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/api-reference/system/ota.html)
- [配置服务快速开始](../../provisioning-service/QUICK_START.md)

---

**文档版本**: v1.0.0  
**参考项目**: xiaozhi-esp32  
**最后更新**: 2025-11-06  
**维护者**: AIOT团队

