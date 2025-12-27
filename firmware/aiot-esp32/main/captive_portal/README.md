# Captive Portal 组件

## 简介

这是一个用于 ESP32 的 Captive Portal（强制门户）组件，实现手机连接 Wi-Fi 热点后自动弹出配网页面的功能。

## 功能特性

- ✅ DNS 通配符解析（所有域名 → 192.168.4.1）
- ✅ iOS Captive Portal 检测支持
- ✅ Android Captive Portal 检测支持
- ✅ Windows 网络检测支持
- ✅ HTTP 通配符处理器（捕获所有未匹配请求）
- ✅ 自动重定向到配网页面

## API 说明

### 启动 DNS 服务器

```c
#include "captive_portal/captive_portal.h"

esp_err_t ret = captive_portal_dns_start();
if (ret == ESP_OK) {
    ESP_LOGI(TAG, "DNS 服务器启动成功");
}
```

### 注册 HTTP 处理器

```c
httpd_handle_t server;
// ... 启动 HTTP 服务器 ...

esp_err_t ret = captive_portal_register_handlers(server);
if (ret == ESP_OK) {
    ESP_LOGI(TAG, "Captive Portal 处理器注册成功");
}
```

### 停止 DNS 服务器

```c
captive_portal_dns_stop();
```

## 集成示例

```c
// 在配网模式启动时
esp_err_t wifi_config_start(void) {
    // 1. 启动 AP 模式
    esp_wifi_set_mode(WIFI_MODE_AP);
    esp_wifi_start();
    
    // 2. 启动 HTTP 服务器
    httpd_handle_t server;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;  // 确保足够的处理器数量
    httpd_start(&server, &config);
    
    // 3. 注册配网页面处理器
    httpd_register_uri_handler(server, &config_get);
    httpd_register_uri_handler(server, &config_post);
    
    // 4. 注册 Captive Portal 处理器
    captive_portal_register_handlers(server);
    
    // 5. 启动 DNS 服务器
    captive_portal_dns_start();
    
    return ESP_OK;
}
```

## 技术原理

### DNS 服务器

- 监听 UDP 53 端口
- 接收所有 DNS 查询
- 返回固定 IP：192.168.4.1
- 触发操作系统的 Captive Portal 检测

### HTTP 处理器

**检测路径（高优先级）：**
- `/hotspot-detect.html` - iOS
- `/library/test/success.html` - iOS
- `/generate_204` - Android
- `/gen_204` - Android
- `/connecttest.txt` - Windows
- `/ncsi.txt` - Windows

**通配符路径（低优先级）：**
- `/*` - 捕获所有其他请求
- 重定向到 `http://192.168.4.1/`

## 依赖项

- `esp_http_server` - HTTP 服务器
- `lwip` - TCP/IP 协议栈（DNS socket）
- `freertos` - 实时操作系统（DNS 任务）

## 资源占用

- **任务栈大小：** 4096 字节
- **任务优先级：** 5
- **UDP 缓冲区：** 256 字节
- **HTTP 处理器：** 8 个

## 故障排查

### DNS 启动失败

**症状：** 日志显示 "创建DNS socket失败" 或 "绑定DNS端口失败"

**原因：**
- 端口 53 已被占用
- 网络接口未就绪

**解决：**
- 确保 AP 模式已启动
- 检查是否有其他 DNS 服务运行

### 手机未自动弹出

**症状：** 连接热点后没有自动弹出配网页面

**原因：**
- DNS 服务器未启动
- 手机系统缓存

**解决：**
1. 查看日志确认 DNS 启动成功
2. 断开并重新连接热点
3. 手动访问 http://192.168.4.1

## 测试方法

### 1. 查看日志

```bash
idf.py monitor
```

预期日志：
```
I captive_portal: ✅ Captive Portal DNS服务器启动成功，端口: 53
I captive_portal: ✅ Captive Portal HTTP处理器注册成功
```

### 2. DNS 测试

```bash
# 从手机或电脑测试 DNS 解析
nslookup example.com 192.168.4.1
```

预期结果：所有域名都解析到 `192.168.4.1`

### 3. HTTP 测试

```bash
# 测试 iOS 检测
curl -i http://192.168.4.1/hotspot-detect.html

# 测试 Android 检测
curl -i http://192.168.4.1/generate_204

# 测试通配符
curl -i http://192.168.4.1/any-path
```

## 兼容性

- ✅ ESP32
- ✅ ESP32-S3
- ✅ ESP32-C3
- ✅ ESP32-S2
- ✅ iOS 10+
- ✅ Android 5.0+
- ✅ Windows 10+

## 许可证

与 AIOT-Admin-Server 项目保持一致

## 作者

AIOT 项目组

## 参考

- [ESP-IDF HTTP Server](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_http_server.html)
- [RFC 1035 - Domain Names](https://www.rfc-editor.org/rfc/rfc1035)
- [RFC 8910 - Captive-Portal Identification](https://www.rfc-editor.org/rfc/rfc8910.html)

