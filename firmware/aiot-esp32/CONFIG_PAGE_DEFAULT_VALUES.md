# 配网页面默认值修改说明

本文档说明如何修改 ESP32 固件的配网页面默认输入值。

## 📝 已完成的修改

### 1. 板子配置
已切换到 **ESP32-S3-DevKit-Lite** 板子：
- **产品代码**: `ESP32-S3-Lite-01`
- **传感器**: 仅 DHT11 温湿度传感器 (GPIO35)
- **不支持**: DS18B20 和舵机功能

### 2. 配网页面默认值
修改文件：`firmware/aiot-esp32/main/wifi_config/wifi_config.c`

**当前默认值**（已移除，用户需手动输入）：
- **WiFi SSID**: 空（用户需输入）
- **WiFi 密码**: 空（用户需输入）
- **服务器地址**: 空（用户需输入）

## 🔧 如何修改配网页面默认值

### 方法：直接修改源代码

编辑文件：`firmware/aiot-esp32/main/wifi_config/wifi_config.c`

#### 1. 修改默认服务器地址

找到第 431 行左右：

```c
// 默认配网服务器地址（已移除，用户需手动输入）
char saved_server_address[128] = "";
```

修改为您需要的地址：
```c
char saved_server_address[128] = "http://your-server.com:port";
```

#### 2. 修改 WiFi SSID 和密码默认值

找到第 466-473 行左右的 HTML 表单部分：

```c
"<div class='form-group'>"
"<label for='ssid'>WiFi Name (SSID):</label>"
"<input type='text' id='ssid' name='ssid' required placeholder='Enter WiFi name' value='Z01'>"
"</div>"
"<div class='form-group'>"
"<label for='password'>WiFi Password:</label>"
"<input type='password' id='password' name='password' placeholder='Enter WiFi password (optional)' value='Z12345678'>"
"</div>"
```

修改 `value` 属性为您需要的默认值：

**修改 SSID**：
```c
value='YOUR_WIFI_SSID'
```

**修改密码**：
```c
value='YOUR_WIFI_PASSWORD'
```

**示例**：
```c
"<input type='text' id='ssid' name='ssid' required placeholder='Enter WiFi name' value='MyHomeWiFi'>"
"<input type='password' id='password' name='password' placeholder='Enter WiFi password (optional)' value='MyPassword123'>"
```

## 🚀 修改后的编译和烧录流程

修改完成后，需要重新编译并烧录固件：

```bash
# 1. 进入固件目录
cd firmware/aiot-esp32

# 2. 激活 ESP-IDF 环境
. $HOME/esp/esp-idf/export.sh

# 3. 清理旧配置（如果切换了板子）
rm -f sdkconfig

# 4. 编译
idf.py build

# 5. 烧录
idf.py flash

# 6. 监控（可选）
idf.py monitor
```

## 📱 配网页面效果

修改后，当设备进入配网模式时：

1. **连接设备热点**: `AIOT-Config-XXXXXX`
2. **打开浏览器**: 自动跳转或访问 `http://192.168.4.1`
3. **查看配网页面**: 输入框将自动填充默认值：
   - WiFi 名称: （用户需输入）
   - WiFi 密码: （用户需输入）
   - 服务器地址: （用户需输入）
4. **可以直接提交**: 如果默认值正确，无需修改直接点击保存

## 🔍 验证默认值

### 方式一：查看源代码
```bash
cd /Users/zhangqixun/AICodeing/AIOT-Admin-Server/firmware/aiot-esp32
# 检查配网页面代码（已移除默认值）
grep -n "placeholder" main/wifi_config/wifi_config.c
```

### 方式二：实际测试
1. 烧录固件到设备
2. 长按 Boot 键 3 秒进入配网模式
3. 手机连接设备热点
4. 打开浏览器查看配网页面，确认默认值是否正确

## 📝 注意事项

### 1. 安全性考虑
⚠️ **不建议在生产环境中硬编码 WiFi 密码**

- 仅用于开发和测试环境
- 生产环境应该让用户手动输入密码
- 如果必须预填充，考虑使用环境变量或配置文件

### 2. 清空默认值
如果希望不预填充任何值，将 `value` 属性设置为空：

```c
"<input type='text' id='ssid' name='ssid' required placeholder='Enter WiFi name' value=''>"
"<input type='password' id='password' name='password' placeholder='Enter WiFi password (optional)' value=''>"
```

或者直接删除 `value` 属性：

```c
"<input type='text' id='ssid' name='ssid' required placeholder='Enter WiFi name'>"
"<input type='password' id='password' name='password' placeholder='Enter WiFi password (optional)'>"
```

### 3. HTML 字符转义
如果密码或 SSID 包含特殊字符（如引号、&符号等），需要进行 HTML 转义：

- `"` → `&quot;`
- `'` → `&#39;`
- `&` → `&amp;`
- `<` → `&lt;`
- `>` → `&gt;`

**示例**：
```c
// 密码包含引号: My"Pass'word&123
value='My&quot;Pass&#39;word&amp;123'
```

## 🌐 服务器地址格式

服务器地址支持以下格式：

### HTTP 格式
```
http://conf.example.com:8001
http://192.168.1.100:8001
http://example.com
```

### HTTPS 格式
```
https://conf.example.com:8001
https://api.example.com
```

### 端口说明
- 如果使用标准端口（HTTP: 80, HTTPS: 443），可以省略端口号
- 自定义端口必须明确指定

## 📚 相关文件

- **配网页面代码**: `firmware/aiot-esp32/main/wifi_config/wifi_config.c`
- **板子配置**: `firmware/aiot-esp32/sdkconfig.defaults`
- **Captive Portal**: `firmware/aiot-esp32/main/captive_portal/captive_portal.c`
- **服务器配置**: `firmware/aiot-esp32/main/server/server_config.c`

## 🔄 Git 提交记录

```bash
# 查看最近的修改
git log --oneline -5

# 查看具体修改内容
git show HEAD

# 回滚到之前的版本（如果需要）
git revert HEAD
```

## ❓ 常见问题

### Q1: 修改后不生效？
**A**: 确保重新编译并烧录固件。旧的固件不会自动更新。

### Q2: 密码显示为点点，如何确认？
**A**: 
1. 浏览器开发者工具（F12）→ 元素检查
2. 找到密码输入框，查看 `value` 属性
3. 或者直接提交表单，在串口日志中查看

### Q3: 服务器地址无效？
**A**: 确保：
1. 地址格式正确（包含 `http://` 或 `https://`）
2. 端口号正确
3. 服务器可访问
4. 防火墙未阻止

### Q4: 如何为不同设备设置不同默认值？
**A**: 有两种方法：
1. 为不同设备编译不同固件（修改源代码）
2. 使用批量配网工具，通过 API 批量配置

## 🔗 参考链接

- [配网服务 API 文档](http://conf.example.com:8001)
- [ESP32 WiFi 配网指南](../README.md)
- [Captive Portal 实现说明](../main/captive_portal/README.md)
