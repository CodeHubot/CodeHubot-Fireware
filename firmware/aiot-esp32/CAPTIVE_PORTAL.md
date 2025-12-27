# Captive Portal（强制门户）实现说明

## ✅ 最新状态（2025-11-06）

**Captive Portal 功能已成功实现并通过编译！**

通过学习 [xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) 项目的架构，我们成功解决了之前的编译问题。现在 Captive Portal 功能已完全可用！

详细的修复过程和经验总结请参考：**[CAPTIVE_PORTAL_XIAOZHI_FIX.md](./CAPTIVE_PORTAL_XIAOZHI_FIX.md)** 📝

---

## 📱 功能概述

Captive Portal（强制门户）是一种网络技术，用于在用户连接到 Wi-Fi 热点后，**自动弹出配网页面**，无需用户手动输入 IP 地址。

### 实现效果

1. 用户打开手机 Wi-Fi 设置
2. 找到设备热点（如 `AIOT-Config-XXXX`）并连接
3. **手机自动弹出配网页面** 📲
4. 用户在页面中输入 Wi-Fi 信息和服务器地址
5. 设备自动连接到目标 Wi-Fi 并启动

## 🔧 技术实现

### 1. DNS 服务器

**作用：** 将所有域名解析到 `192.168.4.1`（ESP32 的 AP IP 地址）

**原理：**
- 监听 UDP 53 端口
- 接收所有 DNS 查询请求
- 对所有查询都返回 `192.168.4.1`

**为什么需要：**
- 手机系统会尝试访问特定的检测域名（如 `captive.apple.com`）
- 如果无法正常解析，系统会认为需要认证
- 通过 DNS 将所有域名指向配网页面，触发强制门户机制

### 2. HTTP Captive Portal 检测

**支持的操作系统检测：**

#### iOS / macOS
- 检测 URL: `http://captive.apple.com/hotspot-detect.html`
- 期望响应: 包含 "Success" 的 HTML 页面
- 状态码: 200 OK

#### Android
- 检测 URL: `http://connectivitycheck.gstatic.com/generate_204`
- 期望响应: 空内容
- 状态码: 204 No Content 或 302 重定向

#### Windows
- 检测 URL: `http://www.msftconnecttest.com/connecttest.txt`
- 期望响应: 文本或重定向
- 状态码: 200 OK 或 302 重定向

### 3. HTTP 通配符处理器

**作用：** 捕获所有未匹配的 HTTP 请求，重定向到配网页面

**实现：**
```c
// 注册通配符路由
httpd_uri_t wildcard_uri = {
    .uri = "/*",
    .method = HTTP_GET,
    .handler = wildcard_handler,
    .user_ctx = NULL
};
```

**优先级：**
1. 配网页面处理器（`/`, `/config`, `/config/current`）
2. Captive Portal 检测处理器（`/hotspot-detect.html`, `/generate_204` 等）
3. 通配符处理器（所有其他请求）

## 📂 代码结构

### 新增组件

```
firmware/aiot-esp32/components/captive_portal/
├── captive_portal.h          # 头文件：API 定义
├── captive_portal.c          # 实现文件：DNS 和 HTTP 处理
└── CMakeLists.txt            # 构建配置
```

### 集成位置

**修改文件：** `main/wifi_config/wifi_config.c`

```c
#include "captive_portal/captive_portal.h"

// 启动 Web 服务器后
start_webserver();

// 注册 Captive Portal HTTP 处理器
captive_portal_register_handlers(s_server);

// 启动 DNS 服务器
captive_portal_dns_start();
```

## 🚀 使用方法

### 编译固件

```bash
cd firmware/aiot-esp32
idf.py build
```

### 烧录固件

```bash
idf.py flash monitor
```

### 测试流程

1. **启动配网模式：**
   - 首次启动（未配置 Wi-Fi）
   - 或按下 Boot 按钮 3 秒以上

2. **手机连接：**
   - 打开 Wi-Fi 设置
   - 连接热点 `AIOT-Config-XXXX`
   - **等待自动弹出配网页面** ✨

3. **配置设备：**
   - 输入 Wi-Fi 名称和密码
   - 输入服务器地址（如 `http://192.168.1.100`）
   - 点击"保存配置"

4. **设备连接：**
   - 设备重启并连接到目标 Wi-Fi
   - 自动向服务器注册

## 🐛 故障排查

### 问题 1: 手机没有自动弹出配网页面

**可能原因：**
- DNS 服务器启动失败
- 手机系统缓存了之前的连接状态

**解决方法：**
1. 查看串口日志，确认 DNS 服务器启动成功：
   ```
   I (xxx) captive_portal: ✅ Captive Portal DNS服务器启动成功，端口: 53
   ```

2. 手机端操作：
   - 完全断开热点连接
   - 关闭 Wi-Fi 后重新打开
   - 重新连接热点

3. 手动访问：
   - 打开浏览器
   - 访问 `http://192.168.4.1`

### 问题 2: iOS 设备显示"无互联网连接"

**正常现象**，这是触发 Captive Portal 的标志。iOS 检测到无法访问互联网，会自动弹出配网页面。

### 问题 3: Android 设备提示"登录网络"

**正常现象**，点击"登录网络"通知即可打开配网页面。

### 问题 4: 配网页面加载缓慢

**可能原因：**
- ESP32 性能限制
- HTTP 处理器数量不足

**优化措施：**
- 已将 `max_uri_handlers` 从 8 增加到 16
- 简化配网页面的 HTML/CSS/JS

## 📊 日志分析

### 正常启动日志

```
I (xxx) wifi_config: 启动WiFi配网模式
I (xxx) wifi_config: 启动HTTP服务器，端口: 80
I (xxx) captive_portal: 注册Captive Portal HTTP处理器...
I (xxx) captive_portal: ✅ Captive Portal HTTP处理器注册成功
I (xxx) captive_portal: 启动Captive Portal DNS服务器...
I (xxx) captive_portal: DNS服务器任务启动
I (xxx) captive_portal: ✅ Captive Portal DNS服务器启动成功，端口: 53
I (xxx) wifi_config: 配网模式启动成功
I (xxx) wifi_config: 请连接WiFi热点: AIOT-Config-XXXX
I (xxx) wifi_config: 📱 手机连接热点后会自动弹出配网页面
```

### DNS 查询日志

```
D (xxx) captive_portal: DNS响应已发送: 45字节
D (xxx) captive_portal: DNS响应已发送: 52字节
```

### HTTP 请求日志

```
I (xxx) captive_portal: 收到Captive Portal检测请求: /hotspot-detect.html
I (xxx) captive_portal: 收到Captive Portal检测请求: /generate_204
I (xxx) captive_portal: 捕获未匹配请求: /favicon.ico
```

## 🔗 参考资料

- [RFC 8910 - Captive-Portal Identification in DHCP and Router Advertisements](https://www.rfc-editor.org/rfc/rfc8910.html)
- [Apple - Captive Network Support](https://developer.apple.com/library/archive/technotes/tn2291/_index.html)
- [Android - Captive Portal Login](https://source.android.com/devices/tech/connect/wifi-network-connection#captive-portal-login)
- [ESP-IDF - HTTP Server](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_http_server.html)

## ✨ 技术亮点

1. **DNS 通配符解析** - 所有域名都指向配网页面
2. **多平台检测支持** - iOS、Android、Windows 全覆盖
3. **优雅降级** - DNS 失败时仍可手动访问
4. **资源高效** - 独立 DNS 任务，内存占用小
5. **用户友好** - 无需手动输入 IP，自动弹出配网页面

## 🎯 优势对比

### 传统方式
❌ 用户需要手动输入 `192.168.4.1`  
❌ 容易输错或不知道如何操作  
❌ 用户体验差  

### Captive Portal
✅ 连接热点后自动弹出  
✅ 无需任何手动操作  
✅ 符合用户使用习惯  
✅ 专业级用户体验  

## 📝 后续优化

1. **DHCP 选项 114** - 通过 DHCP 通告 Captive Portal URL
2. **HTTPS 支持** - 处理现代浏览器的 HTTPS 检测请求
3. **多语言支持** - 根据 HTTP 请求头自动切换语言
4. **二维码配网** - 生成二维码，扫码直达配网页面

---

**实现时间：** 2025-11-06  
**技术栈：** ESP-IDF, lwIP, HTTP Server, DNS  
**支持设备：** ESP32, ESP32-S3, ESP32-C3 等

