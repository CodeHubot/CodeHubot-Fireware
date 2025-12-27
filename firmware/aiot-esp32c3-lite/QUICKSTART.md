# ESP32-C3 Lite 快速开始指南

## 📋 概述

这是一个精简版的ESP32-C3 IoT设备固件，适用于4MB Flash、无OTA、无LVGL显示的应用场景。

## ⚡ 5分钟快速开始

### 1. 安装ESP-IDF (如果还没安装)

```bash
# 创建ESP目录
mkdir -p ~/esp
cd ~/esp

# 克隆ESP-IDF
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
git checkout release/v5.4

# 安装ESP32-C3工具链
./install.sh esp32c3

# 激活环境 (每次新终端都需要)
. $HOME/esp/esp-idf/export.sh
```

### 2. 编译固件

```bash
cd /Users/zhangqixun/AICodeing/AIOT-Admin-Server/firmware/aiot-esp32c3-lite

# 激活ESP-IDF环境
. $HOME/esp/esp-idf/export.sh

# 编译
./build.sh build
```

### 3. 烧录固件

```bash
# 查看设备端口
ls /dev/cu.usbserial-* /dev/cu.usbmodem-*

# 烧录 (自动识别端口)
./build.sh flash

# 或指定端口
./build.sh flash /dev/cu.usbserial-1420

# 烧录并监控
./build.sh flash-monitor
```

### 4. 配网使用

#### 首次使用 (自动配网)
1. 设备启动后自动进入配网模式
2. LED常亮
3. 用手机连接WiFi热点：`AIOT-C3-XXXXXX` (无密码)
4. 浏览器会自动打开配网页面，或手动访问 `http://192.168.4.1`
5. 输入WiFi信息和MQTT服务器
6. 保存后设备自动重启并连接WiFi

#### 重新配网
1. 按住Boot按键(GPIO9)
2. 等待3秒直到LED常亮
3. 释放按键
4. 设备进入配网模式
5. 重复上面的配网步骤

## 🔧 编译脚本使用

```bash
# 编译固件
./build.sh build

# 清理构建
./build.sh clean

# 配置选项
./build.sh config

# 烧录固件
./build.sh flash

# 串口监控
./build.sh monitor

# 烧录+监控
./build.sh fm

# 擦除Flash
./build.sh erase

# 分析大小
./build.sh size

# 完整重编译
./build.sh all

# 生成合并固件
./build.sh merge
```

## 📡 MQTT使用

### 主题结构

设备会自动订阅和发布以下主题：

**发布 (设备上报):**
```
devices/{device_id}/data        # 传感器数据
devices/{device_id}/status      # 设备状态
devices/{device_id}/heartbeat   # 心跳 (30秒一次)
```

**订阅 (接收控制):**
```
devices/{device_id}/control     # 控制命令
```

### 控制命令示例

**控制LED:**
```json
{
  "device_id": "C3-LITE-AABBCCDDEE",
  "type": "control",
  "port": "LED1",
  "value": 1
}
```

**控制继电器:**
```json
{
  "device_id": "C3-LITE-AABBCCDDEE",
  "type": "control",
  "port": "RELAY1",
  "value": 1
}
```

### 测试MQTT

使用mosquitto客户端测试：

```bash
# 订阅所有主题
mosquitto_sub -h 192.168.0.85 -p 1883 -t "devices/#" -v

# 发送控制命令
mosquitto_pub -h 192.168.0.85 -p 1883 \
  -t "devices/C3-LITE-AABBCCDDEE/control" \
  -m '{"type":"control","port":"LED1","value":1}'
```

## 📊 固件大小说明

编译后的固件大小约**400-500KB**：

```
- Bootloader:     ~25KB
- 分区表:         ~3KB
- 主应用:         ~400KB
- 总Flash占用:    ~430KB / 4MB (约10%)
```

剩余约**3.5MB**空间可用于：
- SPIFFS文件系统 (512KB)
- 用户数据存储 (192KB)
- 日志存储 (32KB)
- 系统配置 (32KB)

## 🔍 故障排查

### 编译错误

**Q: 找不到ESP-IDF**
```bash
# 确保激活了环境
. $HOME/esp/esp-idf/export.sh
echo $IDF_PATH  # 应该显示ESP-IDF路径
```

**Q: 编译错误 - 找不到头文件**
```bash
# 完全清理重新编译
./build.sh clean
./build.sh build
```

### 烧录问题

**Q: 无法识别设备**
```bash
# 检查设备连接
ls /dev/cu.* /dev/tty.*

# 检查驱动 (macOS可能需要安装CH340驱动)
```

**Q: 烧录失败**
```bash
# 手动进入下载模式：
# 1. 按住Boot按键
# 2. 按一下Reset按键
# 3. 释放Boot按键
# 然后烧录

# 或擦除后重新烧录
./build.sh erase
./build.sh flash
```

### 运行问题

**Q: 无法连接WiFi**
```bash
# 查看串口日志
./build.sh monitor

# 检查WiFi配置是否正确
# 重新配网
```

**Q: MQTT无法连接**
```bash
# 确认MQTT服务器地址和端口
# 检查防火墙设置
# 查看串口日志中的错误信息
```

### 查看日志

```bash
# 实时监控
./build.sh monitor

# 过滤WiFi相关日志
./build.sh monitor | grep WIFI

# 过滤MQTT相关日志
./build.sh monitor | grep MQTT

# 退出监控: Ctrl+]
```

## 🎓 进阶使用

### 修改默认配置

编辑 `main/board_config.h`:
```c
// 修改默认MQTT服务器
#define DEFAULT_MQTT_BROKER "your-server.com"

// 修改GPIO引脚
#define LED1_GPIO_PIN       8
#define RELAY1_GPIO_PIN     2
#define DHT11_GPIO_PIN      5
```

### 添加自定义功能

1. 修改 `main/main.c` 添加新功能
2. 修改 `main/app_config.h` 添加配置
3. 重新编译烧录

### 优化固件大小

编辑 `sdkconfig.defaults`:
```ini
# 禁用蓝牙 (节省~150KB)
CONFIG_BT_ENABLED=n

# 禁用IPv6 (节省~50KB)
CONFIG_LWIP_IPV6=n

# 最小日志等级 (节省~30KB)
CONFIG_LOG_DEFAULT_LEVEL_WARN=y
```

## 📚 相关文档

- [完整README](README.md) - 详细功能说明
- [后端集成](../../backend/README.md) - 服务器端配置
- [ESP32-C3数据手册](https://www.espressif.com/sites/default/files/documentation/esp32-c3_datasheet_cn.pdf)

## ⚙️ 硬件连接

### 基本连接

```
ESP32-C3开发板:
├─ LED1      -> GPIO8  (内置LED)
├─ 继电器1   -> GPIO2  (需要外接继电器模块)
├─ DHT11     -> GPIO5  (温湿度传感器)
├─ Boot按键  -> GPIO9  (内置按键)
└─ USB       -> 连接电脑 (供电+烧录+调试)
```

### DHT11连接

```
DHT11模块:
  VCC  -> 3.3V
  DATA -> GPIO5
  GND  -> GND
```

### 继电器连接

```
继电器模块:
  VCC -> 5V (或3.3V，取决于模块)
  IN  -> GPIO2
  GND -> GND
```

## 🎯 性能指标

- **启动时间**: ~2秒
- **WiFi连接**: ~3-5秒
- **MQTT连接**: ~1-2秒
- **内存占用**: ~150KB / 400KB
- **功耗**: 
  - 正常运行: ~40mA @ 3.3V
  - WiFi省电模式: ~15mA
  - 深度睡眠: ~5μA (需定制)

## 💡 技巧和建议

1. **首次使用建议先擦除Flash**
   ```bash
   ./build.sh erase
   ./build.sh flash
   ```

2. **查看编译后固件大小**
   ```bash
   ./build.sh size
   ```

3. **生成可分发的固件包**
   ```bash
   ./build.sh merge
   # 生成: build/ESP32-C3-Lite-1.0.0.bin
   ```

4. **保存串口日志到文件**
   ```bash
   ./build.sh monitor | tee device.log
   ```

## 📞 获取帮助

- **编译脚本帮助**: `./build.sh`
- **ESP-IDF文档**: https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32c3/
- **项目Issue**: 提交到项目仓库

---

**更新日期**: 2025-12-27  
**固件版本**: 1.0.0  
**适用芯片**: ESP32-C3  
**Flash要求**: 4MB

