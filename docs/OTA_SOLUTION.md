# AIOT系统 - OTA固件更新完整方案

## 📌 概述

基于xiaozhi-esp32项目的OTA实现，结合我们的配置服务，实现了完整的OTA固件更新解决方案。

**参考项目**: https://github.com/78/xiaozhi-esp32

## 🏗️ 架构图

```
┌─────────────────────────────────────────────────────────────┐
│                    配置服务 (Provisioning Service)            │
│                  http://provision.example.com                │
│                                                               │
│  GET /device/info?mac=AA:BB:CC&firmware_version=1.0.0       │
│                                                               │
│  响应:                                                         │
│  {                                                            │
│    "device_uuid": "...",                                      │
│    "mqtt_config": {...},                                      │
│    "firmware_update": {           ⭐ OTA信息                  │
│      "available": true,                                       │
│      "version": "1.1.0",                                      │
│      "download_url": "http://ota.../v1.1.0.bin",            │
│      "file_size": 1048576,                                    │
│      "checksum": "sha256:...",                                │
│      "changelog": "修复bug"                                    │
│    }                                                          │
│  }                                                            │
└───────────────────┬──────────────────────────────────────────┘
                    │
                    │ ① 设备启动，请求配置
                    │
                    ↓
┌─────────────────────────────────────────────────────────────┐
│                ESP32设备 (固件v1.0.0)                         │
│                                                               │
│  main/                                                        │
│  ├── ota/ota_manager.c     (参考xiaozhi实现)                 │
│  └── main.c                                                   │
│                                                               │
│  ② 检查 firmware_update.available                            │
│  ③ 如果有更新，调用 ota_manager_start_upgrade()              │
│  ④ 流式下载固件，显示进度                                     │
│  ⑤ 写入OTA分区，验证                                          │
│  ⑥ 设置启动分区，重启                                         │
└───────────────────┬──────────────────────────────────────────┘
                    │
                    │ ⑦ 下载固件
                    ↓
┌─────────────────────────────────────────────────────────────┐
│                  OTA服务器                                    │
│            http://ota.example.com                             │
│                                                               │
│  /firmware/                                                   │
│  ├── v1.0.0.bin                                              │
│  ├── v1.1.0.bin        ⭐ 新固件                             │
│  └── v1.2.0.bin                                              │
└─────────────────────────────────────────────────────────────┘
```

## 🎯 核心组件

### 1. 配置服务 (Provisioning Service)

**位置**: `provisioning-service/main.py`

**功能**:
- ✅ 提供设备配置（MQTT、UUID等）
- ✅ **自动检测固件更新**
- ✅ 返回固件下载URL和版本信息
- ✅ 支持GET/POST两种方式

**文档**: 
- [QUICK_START.md](../provisioning-service/QUICK_START.md)
- [GET_REQUEST_SUMMARY.md](../provisioning-service/GET_REQUEST_SUMMARY.md)
- [OTA_SUMMARY.md](../provisioning-service/OTA_SUMMARY.md)

### 2. OTA管理器 (固件端)

**位置**: `firmware/aiot-esp32/main/ota/`

**功能**:
- ✅ 版本检查（参考xiaozhi的CheckVersion）
- ✅ 流式下载和写入（参考xiaozhi的Upgrade）
- ✅ 版本号对比（参考xiaozhi的IsNewVersionAvailable）
- ✅ 自动标记有效（参考xiaozhi的MarkCurrentVersionValid）
- ✅ 进度回调

**文档**: 
- [OTA_INTEGRATION.md](../firmware/aiot-esp32/OTA_INTEGRATION.md)
- [OTA_GUIDE.md](../firmware/aiot-esp32/OTA_GUIDE.md)

### 3. 数据库 (固件版本管理)

**表**: `firmware_versions`

**字段**:
```sql
CREATE TABLE firmware_versions (
    id INT PRIMARY KEY AUTO_INCREMENT,
    version VARCHAR(32) NOT NULL,           -- 版本号 (如: 1.1.0)
    product_id INT NOT NULL,                -- 产品ID
    download_url VARCHAR(512) NOT NULL,     -- 下载URL
    file_size INT,                          -- 文件大小
    checksum VARCHAR(255),                  -- SHA256校验和
    changelog TEXT,                         -- 更新日志
    is_active BOOLEAN DEFAULT true,         -- 是否激活
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```

## 🔄 完整流程

### 阶段1: 设备启动

```c
void app_main(void) {
    ESP_LOGI(TAG, "固件版本: %s", FIRMWARE_VERSION);
    
    // 1. 初始化OTA管理器
    ota_manager_init();
    
    // 2. 标记当前固件有效（参考xiaozhi）
    //    防止OTA更新后回滚
    ota_manager_mark_valid();
    
    // 3. 其他初始化...
}
```

### 阶段2: 检查更新

```c
// WiFi连接成功后
firmware_info_t fw_info;
esp_err_t ret = ota_manager_check_version(
    "http://provision.example.com",  // 配置服务
    "AA:BB:CC:DD:EE:FF",             // MAC地址
    FIRMWARE_VERSION,                // 当前版本
    &fw_info                         // 输出
);

if (ret == ESP_OK && fw_info.available) {
    ESP_LOGI(TAG, "发现新版本: %s", fw_info.version);
    ESP_LOGI(TAG, "更新内容: %s", fw_info.changelog);
}
```

### 阶段3: 执行更新

```c
void ota_progress_handler(int progress, size_t speed) {
    ESP_LOGI(TAG, "OTA进度: %d%%, 速度: %uKB/s", progress, speed/1024);
    simple_display_update_status(...);
}

// 开始升级（参考xiaozhi的StartUpgrade）
ret = ota_manager_start_upgrade(
    fw_info.download_url,
    ota_progress_handler
);

if (ret == ESP_OK) {
    ESP_LOGI(TAG, "升级成功，重启...");
    esp_restart();
}
```

### 阶段4: 重启验证

```c
void app_main(void) {
    // 重启后首次启动
    ota_manager_mark_valid();  // 标记为有效
    
    // 运行自检
    if (!run_self_test()) {
        // 自检失败，回滚
        esp_ota_mark_app_invalid_rollback_and_reboot();
    }
}
```

## 🎨 用户体验

### LCD显示流程

```
设备启动
  ↓
检查更新... ━━━━━━┓
  ↓              ↓
已是最新版本   发现新版本 v1.1.0
  ↓              ↓
正常运行       固件更新中
                 ↓
               下载中 10% ━━━━━━━━━━
               下载中 20% ━━━━━━━━━━━━━━
               ...
               下载中 100% ━━━━━━━━━━━━━━━━━━━━
                 ↓
               安装中...
                 ↓
               更新成功
                 ↓
               重启中...
                 ↓
               固件版本: v1.1.0
```

## 📊 性能特点

### 参考xiaozhi的优化策略

| 特性 | xiaozhi | 我们的实现 |
|------|---------|-----------|
| **流式下载** | ✅ 512B缓冲 | ✅ 1KB缓冲 |
| **内存占用** | 低（边下边写） | 低（边下边写） |
| **进度显示** | 每秒更新 | 每秒更新 |
| **速度计算** | ✅ 实时计算 | ✅ 实时计算 |
| **固件验证** | ✅ 头部检查 | ✅ 头部检查 |
| **自动回滚** | ✅ ESP-IDF机制 | ✅ ESP-IDF机制 |

### 内存优化

```c
// ❌ 不好：一次性加载整个固件
char *firmware_data = malloc(1048576);  // 1MB
http_read_all(firmware_data);
esp_ota_write(handle, firmware_data, 1048576);

// ✅ 好：流式处理（参考xiaozhi）
char buffer[1024];  // 只需1KB缓冲
while (http_read(buffer, 1024) > 0) {
    esp_ota_write(handle, buffer, 1024);
}
```

## 🔧 管理端操作

### 1. 上传新固件

```bash
# 编译固件
cd firmware/aiot-esp32
vim main/main.c  # 修改版本号为 1.1.0
idf.py build

# 计算校验和
sha256sum build/aiot-esp32s3-firmware.bin

# 上传到OTA服务器
scp build/aiot-esp32s3-firmware.bin \
    user@server:/var/www/ota/v1.1.0.bin
```

### 2. 添加固件记录

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
    1,  -- ESP32-S3开发板
    'http://ota.example.com/firmware/v1.1.0.bin',
    1048576,
    'sha256:e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855',
    '修复LED闪烁bug，优化MQTT连接，提升稳定性',
    true
);
```

### 3. 触发更新

设备会在以下时机检查更新：
- ✅ 启动时
- ✅ 定期检查（每24小时）
- ✅ 手动触发（按钮/命令）

## 🔒 安全措施

### 1. 传输安全

```c
// 使用HTTPS下载固件
esp_http_client_config_t config = {
    .url = "https://ota.example.com/firmware/v1.1.0.bin",
    .cert_pem = server_cert,  // 服务器证书
};
```

### 2. 固件验证

```
下载 → 验证头部 → 验证校验和 → 写入分区 → 启动验证 → 自检
                                                ↓
                                           失败自动回滚
```

### 3. 回滚机制（ESP-IDF内置）

```c
// OTA更新后首次启动
const esp_partition_t *partition = esp_ota_get_running_partition();
esp_ota_img_states_t state;
esp_ota_get_state_partition(partition, &state);

if (state == ESP_OTA_IMG_PENDING_VERIFY) {
    // 运行自检
    if (run_self_test()) {
        esp_ota_mark_app_valid_cancel_rollback();  // 标记有效
    } else {
        // 自动回滚到旧版本
        esp_ota_mark_app_invalid_rollback_and_reboot();
    }
}
```

## 📈 更新策略

### 策略1: 立即更新（推荐无人值守设备）

```c
if (fw_info.available) {
    ESP_LOGI(TAG, "3秒后开始更新...");
    vTaskDelay(pdMS_TO_TICKS(3000));
    ota_manager_start_upgrade(fw_info.download_url, callback);
}
```

### 策略2: 定时更新（推荐生产设备）

```c
// 凌晨3点更新，避免影响使用
if (fw_info.available && current_hour == 3) {
    ota_manager_start_upgrade(fw_info.download_url, callback);
}
```

### 策略3: 用户确认（推荐有界面设备）

```c
if (fw_info.available) {
    display_message("发现新版本，按按钮更新");
    if (wait_button_press(60000)) {  // 60秒超时
        ota_manager_start_upgrade(fw_info.download_url, callback);
    }
}
```

### 策略4: 强制更新

```sql
-- 数据库中设置 force_update
UPDATE firmware_versions 
SET force_update = true 
WHERE version = '1.1.0';
```

```c
// 固件端检查 force_update 标志
if (fw_info.force_update) {
    // 立即更新，不管版本号
    ota_manager_start_upgrade(fw_info.download_url, callback);
}
```

## 🧪 测试清单

- [ ] 正常更新流程（1.0.0 → 1.1.0）
- [ ] 版本号对比（1.0.0 < 1.0.1 < 1.1.0 < 2.0.0）
- [ ] 进度显示（0% → 100%）
- [ ] 速度计算（B/s, KB/s）
- [ ] 固件验证（校验和）
- [ ] 断网重试
- [ ] 下载失败回滚
- [ ] 安装失败回滚
- [ ] 自检失败自动回滚
- [ ] LCD显示更新
- [ ] 定期检查更新
- [ ] 强制更新
- [ ] 取消更新（is_active=false）

## 📚 相关文档

### 固件端
- [OTA管理器集成指南](../firmware/aiot-esp32/OTA_INTEGRATION.md) ⭐
- [OTA详细实现指南](../firmware/aiot-esp32/OTA_GUIDE.md)

### 服务端
- [配置服务快速开始](../provisioning-service/QUICK_START.md) ⭐
- [GET请求总结](../provisioning-service/GET_REQUEST_SUMMARY.md)
- [OTA总结](../provisioning-service/OTA_SUMMARY.md)

### 参考项目
- [xiaozhi-esp32 (GitHub)](https://github.com/78/xiaozhi-esp32) 🙏

## ❓ 常见问题

### Q: 如何取消一个固件版本？

A: 设置 `is_active = false`:
```sql
UPDATE firmware_versions SET is_active = false WHERE version = '1.1.0';
```

### Q: 可以跨版本更新吗？

A: 可以！比如 1.0.0 直接更新到 1.2.0。

### Q: 更新失败会怎样？

A: ESP-IDF有自动回滚机制，会回到旧版本。

### Q: 如何强制所有设备更新？

A: 设置数据库中的 `is_active = true`，设备下次检查时会自动更新。

### Q: 支持增量更新吗？

A: 当前是完整固件更新。增量更新需要额外实现。

### Q: 如何验证固件完整性？

A: 使用SHA256校验和，服务器返回，设备端验证。

---

**文档版本**: v1.0.0  
**最后更新**: 2025-11-06  
**维护者**: AIOT团队  
**鸣谢**: xiaozhi-esp32项目 🙏

