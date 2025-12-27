# AIOT ESP32 固件

基于 ESP32 系列芯片的物联网设备管理系统固件，支持多种传感器、控制设备和网络配置功能。

## 📋 目录

- [项目特性](#项目特性)
- [支持的硬件](#支持的硬件)
- [GPIO 引脚映射](#gpio-引脚映射)
- [项目结构](#项目结构)
- [快速开始](#快速开始)
- [功能说明](#功能说明)
- [MQTT 通信协议](#mqtt-通信协议)
- [开发指南](#开发指南)
- [故障排查](#故障排查)
- [相关文档](#相关文档)

## 项目特性

### 核心功能
- ✅ **多板支持**: ESP32-S3、ESP32-C3、ESP32-P4 等
- ✅ **WiFi 配网**: Boot 按键触发的 Web 配网功能 + Captive Portal
- ✅ **MQTT 通信**: 设备与服务器之间的双向通信
- ✅ **传感器支持**: DHT11（温湿度）、DS18B20（温度）
- ✅ **控制设备**: LED、继电器、舵机、PWM输出
- ✅ **预设控制**: LED(闪烁、波浪)、舵机(摆动、旋转)、继电器(定时开关)、PWM(渐变、呼吸灯、步进、脉冲、固定输出)等预设指令
- ✅ **OTA 更新**: 支持远程固件升级
- ✅ **LCD 显示**: 设备状态和传感器数据实时显示
- ✅ **硬件抽象**: 统一的 HAL 层，便于移植和扩展

### 技术特点
- **分层架构**: HAL 层、BSP 层、应用层清晰分离
- **模块化设计**: 可扩展的硬件抽象和驱动架构
- **ESP-IDF 5.4**: 基于最新的 ESP-IDF 框架
- **双环境支持**: Makefile 开发测试 + ESP-IDF 硬件部署

## 支持的硬件

### 芯片支持
- ✅ **ESP32-S3** - 主力开发板（推荐）
- ✅ **ESP32-C3** - 经济型选择
- 🔄 **ESP32-P4** - 高性能版本（规划中）
- 🔄 **ESP32-C6** - 支持 Matter 协议（规划中）
- 🔄 **ESP32-H2** - Zigbee/Thread 支持（规划中）

### 传感器支持
| 传感器 | 型号 | 功能 | 状态 |
|--------|------|------|------|
| 温湿度传感器 | DHT11 | 温度、湿度检测 | ✅ |
| 温度传感器 | DS18B20 | 高精度温度检测 | ✅ |

### 控制设备支持
| 设备 | 类型 | 功能 | 状态 |
|------|------|------|------|
| LED | GPIO/PWM | 开关、亮度调节 | ✅ |
| 继电器 | GPIO | 高功率设备控制 | ✅ |
| 舵机 | PWM | 角度控制（0-180°） | ✅ |
| PWM输出 | PWM | 自定义频率和占空比输出 | ✅ |

## GPIO 引脚映射

### ESP32-S3 DevKit 标准配置

#### LED 控制（PWM）
| 功能 | 标识 | GPIO 引脚 | 说明 |
|------|------|-----------|------|
| LED1 | L1 | GPIO42 | 可调亮度 LED |
| LED2 | L2 | GPIO41 | 可调亮度 LED |
| LED3 | L3 | GPIO37 | 可调亮度 LED |
| LED4 | L4 | GPIO36 | 可调亮度 LED |

#### 继电器控制
| 功能 | 标识 | GPIO 引脚 | 触发方式 |
|------|------|-----------|----------|
| 继电器1 | RELAY1 | GPIO01 | 高电平触发 |
| 继电器2 | RELAY2 | GPIO02 | 高电平触发 |

#### 传感器接口
| 功能 | 标识 | GPIO 引脚 | 协议 |
|------|------|-----------|------|
| DHT11 温湿度 | S1 | GPIO35 | 单总线 |
| DS18B20 温度 | S2 | GPIO39 | 单总线 |

#### 舵机控制
| 功能 | 标识 | GPIO 引脚 | 角度范围 |
|------|------|-----------|----------|
| 舵机1 | M1 | GPIO48 | 0-180° |

#### PWM输出控制
| 功能 | 标识 | GPIO 引脚 | 说明 |
|------|------|-----------|------|
| PWM输出 (M2) | M2 | GPIO40 | 可自定义频率(1-40000Hz)和占空比(0-100%) |

#### 系统按键
| 功能 | GPIO 引脚 | 说明 |
|------|-----------|------|
| Boot 按键 | GPIO0 | 长按 3 秒进入配网模式 |

## 项目结构

```
aiot-esp32/
├── main/                           # 主应用程序
│   ├── main.c                     # 应用入口
│   ├── app_config.h               # 应用配置
│   ├── bluetooth/                 # 蓝牙模块（预留）
│   ├── bsp/                       # 板级支持包
│   ├── button/                    # 按键处理
│   ├── captive_portal/            # Captive Portal 实现
│   ├── device/                    # 设备控制
│   │   ├── device_control.c      # 设备控制逻辑
│   │   ├── preset_control.c      # 预设指令
│   │   ├── pwm_control.c         # PWM输出控制
│   │   └── sensor_manager.c      # 传感器管理
│   ├── hal/                       # 硬件抽象层
│   ├── mqtt/                      # MQTT 客户端
│   │   ├── aiot_mqtt_client.c    # MQTT 客户端实现
│   │   └── mqtt_topics.h         # 主题定义
│   ├── ota/                       # OTA 更新
│   ├── provisioning/              # 设备配网
│   ├── server/                    # HTTP 服务器
│   ├── startup/                   # 启动管理
│   │   └── startup_manager.c     # 启动流程控制
│   ├── system/                    # 系统功能
│   ├── wechat_ble/                # 微信小程序 BLE（预留）
│   └── wifi_config/               # WiFi 配置
├── boards/                         # 板级配置
│   ├── esp32-s3-devkit/          # ESP32-S3 开发板配置
│   ├── esp32-c3-mini/            # ESP32-C3 迷你板配置
│   └── esp32-p4-function-ev/     # ESP32-P4 功能评估板
├── components/                     # 自定义组件
│   ├── display/                   # 显示驱动
│   └── ui/                        # UI 组件
├── drivers/                        # 外设驱动
│   ├── lcd/                       # LCD 驱动（ST7789）
│   └── sensors/                   # 传感器驱动
├── managed_components/             # ESP 组件管理器
│   ├── espressif__esp_lvgl_port/ # LVGL 移植
│   └── lvgl__lvgl/               # LVGL 图形库
├── build/                          # 编译输出（自动生成）
├── tools/                          # 工具脚本
│   └── get_mac.py                # 获取设备 MAC 地址
├── partitions.csv                 # Flash 分区表
├── sdkconfig                      # ESP-IDF 配置
├── CMakeLists.txt                 # CMake 构建配置
└── build_esp_idf.sh              # 快速构建脚本
```

## 快速开始

### 前置要求

1. **硬件**
   - ESP32-S3 开发板
   - USB-C 数据线
   - （可选）DHT11、DS18B20 传感器
   - （可选）LED、继电器等控制设备

2. **软件**
   - ESP-IDF 5.4+
   - Python 3.8+
   - Git

### 环境搭建

#### 1. 安装 ESP-IDF

```bash
# macOS/Linux
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
git checkout release/v5.4
./install.sh esp32s3

# 配置环境变量（每次新终端都需要）
. $HOME/esp/esp-idf/export.sh
```

#### 2. 克隆项目

```bash
cd /Users/zhangqixun/AICodeing/AIOT-Admin-Server/firmware/aiot-esp32
```

### 编译和烧录

#### 方式一：使用快速构建脚本（推荐）

```bash
# 1. 激活 ESP-IDF 环境
. $HOME/esp/esp-idf/export.sh

# 2. 使用快速构建脚本
./build_esp_idf.sh

# 可用命令：
# check       - 检查 ESP-IDF 环境
# target      - 设置目标芯片（esp32s3）
# menuconfig  - 配置项目
# build       - 编译项目
# flash       - 烧录固件
# monitor     - 监控串口输出
# flash-mon   - 烧录并监控
# clean       - 清理构建文件
# fullclean   - 完全清理
```

#### 方式二：使用 ESP-IDF 命令

```bash
# 1. 激活环境
. $HOME/esp/esp-idf/export.sh

# 2. 设置目标芯片
idf.py set-target esp32s3

# 3. 配置项目（可选）
idf.py menuconfig

# 4. 编译
idf.py build

# 5. 烧录
idf.py -p /dev/cu.usbserial-* flash

# 6. 监控串口
idf.py -p /dev/cu.usbserial-* monitor

# 7. 一键烧录并监控
idf.py -p /dev/cu.usbserial-* flash monitor
```

### 查看设备端口

```bash
# macOS
ls /dev/cu.usbserial-* /dev/cu.usbmodem-*

# Linux
ls /dev/ttyUSB* /dev/ttyACM*

# Windows
# 在设备管理器中查看 COM 端口
```

## 功能说明

### WiFi 配网

设备支持两种配网方式：

#### 1. Boot 按键配网（推荐）

1. **进入配网模式**
   - 长按 Boot 按键 3 秒
   - LED 开始闪烁，LCD 显示"配网模式"

2. **连接热点**
   - 设备创建名为 `AIOT-ESP32-XXXXXX` 的 WiFi 热点
   - 使用手机或电脑连接该热点

3. **配置 WiFi**
   - 浏览器自动打开配网页面（Captive Portal）
   - 或手动访问 `http://192.168.4.1`
   - 选择目标 WiFi 并输入密码

4. **完成配网**
   - 设备自动连接到指定 WiFi
   - LCD 显示 IP 地址

#### 2. 首次启动自动配网

如果设备未配置过 WiFi，首次启动会自动进入配网模式。

详细说明：[WiFi 配网指南](WIFI_CONFIG_GUIDE.md)

### MQTT 通信

设备通过 MQTT 与服务器进行双向通信。

#### 连接配置

```c
// 在 app_config.h 中配置
#define MQTT_BROKER_HOST "localhost"
#define MQTT_BROKER_PORT 1883
```

#### 主题结构

**设备发布（上报数据）**：
```
devices/{device_id}/data        # 传感器数据
devices/{device_id}/status      # 设备状态
devices/{device_id}/heartbeat   # 设备心跳
devices/{device_id}/response    # 控制响应
```

**设备订阅（接收命令）**：
```
devices/{device_id}/control     # 控制命令
devices/{device_id}/config      # 配置下发
```

详细协议：[设备 API 参考](DEVICE_API_REFERENCE.md)

### 传感器数据上报

#### DHT11 温湿度传感器

```json
{
  "device_id": "AIOT-ESP32-597E64DE",
  "sensor": "DHT11",
  "temperature": 25.5,
  "humidity": 60.2,
  "timestamp": 1699516800
}
```

#### DS18B20 温度传感器

```json
{
  "device_id": "AIOT-ESP32-597E64DE",
  "sensor": "DS18B20",
  "temperature": 22.3,
  "timestamp": 1699516800
}
```

### 设备控制

#### LED 控制

```json
{
  "device_id": "AIOT-ESP32-597E64DE",
  "type": "control",
  "port": "L1",
  "value": 1,
  "brightness": 80
}
```

#### 继电器控制

```json
{
  "device_id": "AIOT-ESP32-597E64DE",
  "type": "control",
  "port": "RELAY1",
  "value": 1
}
```

#### 舵机控制

```json
{
  "device_id": "AIOT-ESP32-597E64DE",
  "type": "control",
  "port": "M1",
  "value": 1,
  "angle": 90
}
```

#### PWM输出控制

```json
{
  "cmd": "pwm",
  "channel": 2,
  "frequency": 5000,
  "duty_cycle": 50.0
}
```

**参数说明**：
- `cmd`: 命令类型，固定为 "pwm"
- `channel`: PWM通道，M2对应通道2
- `frequency`: PWM频率（Hz），范围：1-40000
- `duty_cycle`: 占空比（%），范围：0.0-100.0

**应用场景**：
- 直流电机速度控制
- LED亮度精确调节
- 蜂鸣器音调控制
- 其他需要PWM信号的设备

#### LED预设控制

固件支持2种LED预设效果：

**1. LED闪烁 (Blink)**
```json
{
  "cmd": "preset",
  "device_type": "led",
  "device_id": 0,
  "preset_type": "blink",
  "parameters": {
    "count": 5,
    "on_time": 500,
    "off_time": 500
  }
}
```
用途：LED快速闪烁，适合警告、提示等场景。`device_id` 为 0 表示所有LED，或指定单个LED编号（1-4）。

**2. LED波浪灯 (Wave)** - 支持自定义序列
```json
{
  "cmd": "preset",
  "device_type": "led",
  "device_id": 0,
  "preset_type": "wave",
  "parameters": {
    "interval_ms": 200,
    "cycles": 3,
    "reverse": false,
    "led_sequence": [1, 3, 4, 2]
  }
}
```
用途：LED依次点亮形成流水效果。

**参数说明**：
- `led_sequence`（可选）：自定义LED点亮顺序，如 `[1, 3, 4, 2]` 表示按 LED1→LED3→LED4→LED2 的顺序流水
- 如果不提供 `led_sequence`，则使用所有LED（1-4）按顺序流水
- `reverse`: true 表示反向播放序列
- `cycles`: 循环次数

**使用示例**：
- 全部LED流水：不提供 `led_sequence` 或 `led_sequence: [1, 2, 3, 4]`
- 只用部分LED：`led_sequence: [1, 3, 4]`（只点亮LED1、3、4）
- 自定义顺序：`led_sequence: [4, 2, 1, 3]`（按4→2→1→3顺序流水）
- 反向流水：设置 `reverse: true`，序列将逆序执行

#### PWM预设控制

固件支持5种PWM预设效果：

**1. PWM渐变 (Fade)** - 平滑过渡
```json
{
  "cmd": "preset",
  "device_type": "pwm",
  "device_id": 2,
  "preset_type": "fade",
  "parameters": {
    "frequency": 5000,
    "start_duty": 0.0,
    "end_duty": 100.0,
    "duration": 2000,
    "step_interval": 50
  }
}
```
用途：占空比从起始值平滑过渡到目标值，适合灯光渐亮/渐暗、电机平滑启动。

**2. PWM呼吸灯 (Breathe)** - 循环呼吸
```json
{
  "cmd": "preset",
  "device_type": "pwm",
  "device_id": 2,
  "preset_type": "breathe",
  "parameters": {
    "frequency": 5000,
    "min_duty": 0.0,
    "max_duty": 100.0,
    "fade_in_time": 1500,
    "fade_out_time": 1500,
    "hold_time": 500,
    "cycles": 5
  }
}
```
用途：循环渐亮渐暗，模拟呼吸效果，适合氛围灯、状态指示。

**3. PWM步进 (Step)** - 逐级调整
```json
{
  "cmd": "preset",
  "device_type": "pwm",
  "device_id": 2,
  "preset_type": "step",
  "parameters": {
    "frequency": 5000,
    "start_duty": 0.0,
    "end_duty": 100.0,
    "step_value": 10.0,
    "step_delay": 300
  }
}
```
用途：按设定步进值逐级调整，适合需要分档调整的场合。

**4. PWM脉冲 (Pulse)** - 快速切换
```json
{
  "cmd": "preset",
  "device_type": "pwm",
  "device_id": 2,
  "preset_type": "pulse",
  "parameters": {
    "frequency": 5000,
    "duty_high": 80.0,
    "duty_low": 20.0,
    "high_time": 500,
    "low_time": 500,
    "cycles": 10
  }
}
```
用途：快速在两个占空比之间切换，适合闪烁效果、警示灯。

**5. PWM固定输出 (Fixed)** - 恒定输出
```json
{
  "cmd": "preset",
  "device_type": "pwm",
  "device_id": 2,
  "preset_type": "fixed",
  "parameters": {
    "frequency": 5000,
    "duty_cycle": 50.0,
    "duration": 0
  }
}
```
用途：设置固定的频率和占空比，duration为0表示持续输出。

### OTA 固件升级

支持通过 MQTT 或 HTTP 进行远程固件升级。

```json
{
  "type": "ota",
  "version": "1.2.0",
  "url": "http://server.com/firmware.bin",
  "md5": "abc123..."
}
```

详细说明：[OTA 升级指南](OTA_GUIDE.md)

## MQTT 通信协议

### 消息格式

所有 MQTT 消息均使用 JSON 格式。

#### 设备心跳

**主题**: `devices/{device_id}/heartbeat`

```json
{
  "device_id": "AIOT-ESP32-597E64DE",
  "timestamp": 1699516800,
  "uptime": 3600,
  "free_heap": 102400,
  "rssi": -45
}
```

#### 设备状态

**主题**: `devices/{device_id}/status`

```json
{
  "device_id": "AIOT-ESP32-597E64DE",
  "online": true,
  "ip": "192.168.1.100",
  "mac": "80:B5:4E:D6:F8:60",
  "rssi": -45,
  "firmware_version": "1.0.0"
}
```

#### 控制响应

**主题**: `devices/{device_id}/response`

```json
{
  "device_id": "AIOT-ESP32-597E64DE",
  "command_id": "cmd_123",
  "status": "success",
  "message": "LED1 已开启"
}
```

### QoS 级别

- **心跳、状态**: QoS 0（最多一次）
- **传感器数据**: QoS 1（至少一次）
- **控制命令**: QoS 1（至少一次）

## 开发指南

### 添加新的传感器

1. **创建驱动文件**

在 `drivers/sensors/` 中创建新的传感器驱动：

```c
// drivers/sensors/my_sensor.c
#include "my_sensor.h"

esp_err_t my_sensor_init(gpio_num_t pin) {
    // 初始化代码
    return ESP_OK;
}

esp_err_t my_sensor_read(float *value) {
    // 读取传感器数据
    return ESP_OK;
}
```

2. **注册到传感器管理器**

在 `main/device/sensor_manager.c` 中注册：

```c
void sensor_manager_init(void) {
    // 注册新传感器
    my_sensor_init(GPIO_MY_SENSOR);
}
```

3. **配置 MQTT 上报**

在 `app_config.h` 中配置上报间隔：

```c
#define MY_SENSOR_REPORT_INTERVAL_MS 5000
```

### 添加新的控制设备

1. **定义 GPIO**

在 `boards/esp32-s3-devkit/board_config.h` 中：

```c
#define GPIO_MY_DEVICE 10
```

2. **实现控制逻辑**

在 `main/device/device_control.c` 中：

```c
esp_err_t my_device_control(int value) {
    gpio_set_level(GPIO_MY_DEVICE, value);
    return ESP_OK;
}
```

3. **添加 MQTT 处理**

在 `main/startup/startup_manager.c` 中处理控制命令。

### 修改产品配置

编辑后端的产品配置脚本，添加新的端口定义：

```python
# backend/init_product_config.py
control_ports = {
    "MY_DEVICE": {
        "name": "我的设备",
        "type": "digital",
        "pin": 10,
        "enabled": True
    }
}
```

### 调试技巧

#### 1. 启用详细日志

在 `menuconfig` 中：
```
Component config → Log output → Default log verbosity → Verbose
```

#### 2. 监控内存

```c
#include "esp_heap_caps.h"

ESP_LOGI(TAG, "Free heap: %d bytes", esp_get_free_heap_size());
```

#### 3. 使用 GDB 调试

```bash
idf.py openocd &
xtensa-esp32s3-elf-gdb build/aiot-esp32s3-firmware.elf
(gdb) target remote :3333
(gdb) break app_main
(gdb) continue
```

## 故障排查

### 编译问题

#### 问题：找不到 ESP-IDF

**解决方案**：
```bash
# 确保激活了 ESP-IDF 环境
. $HOME/esp/esp-idf/export.sh

# 检查环境变量
echo $IDF_PATH
```

#### 问题：依赖组件缺失

**解决方案**：
```bash
# 更新组件
idf.py reconfigure

# 完全清理重新编译
idf.py fullclean
idf.py build
```

### 烧录问题

#### 问题：无法识别设备

**解决方案**：
```bash
# macOS：安装 CH340 驱动
# 检查设备连接
ls /dev/cu.*

# 手动进入下载模式
# 1. 按住 Boot 按键
# 2. 按一下 Reset 按键
# 3. 释放 Boot 按键
```

#### 问题：烧录失败

**解决方案**：
```bash
# 降低波特率
idf.py -p /dev/cu.usbserial-* -b 115200 flash

# 擦除 Flash 后重新烧录
idf.py -p /dev/cu.usbserial-* erase-flash
idf.py -p /dev/cu.usbserial-* flash
```

### WiFi 连接问题

#### 问题：无法连接 WiFi

**解决方案**：
1. 检查 WiFi 信号强度
2. 确认密码正确
3. 查看串口日志中的错误信息
4. 重新进入配网模式

```bash
# 监控 WiFi 连接日志
idf.py monitor | grep -i wifi
```

#### 问题：配网页面无法打开

**解决方案**：
1. 确认已连接到设备热点
2. 手动访问 `http://192.168.4.1`
3. 关闭手机的移动数据
4. 尝试使用其他浏览器

### MQTT 连接问题

#### 问题：无法连接到 MQTT 服务器

**解决方案**：
1. 检查服务器地址和端口配置
2. 确认 MQTT 服务器正在运行
3. 检查防火墙设置
4. 查看串口日志

```bash
# 测试 MQTT 服务器
mosquitto_sub -h localhost -p 1883 -t "devices/#" -v
```

### 传感器读取问题

#### 问题：DHT11 读取失败

**解决方案**：
1. 检查接线（GPIO35）
2. 确认传感器供电正常
3. 增加重试次数
4. 检查时序是否正确

#### 问题：DS18B20 读取失败

**解决方案**：
1. 检查接线（GPIO39）
2. 确认上拉电阻（4.7kΩ）
3. 检查传感器地址

### 内存问题

#### 问题：内存不足导致重启

**解决方案**：
```bash
# 监控内存使用
idf.py monitor | grep -i heap

# 在代码中定期打印内存
ESP_LOGI(TAG, "Free heap: %d", esp_get_free_heap_size());

# 优化内存使用
# 1. 减少缓冲区大小
# 2. 及时释放不用的内存
# 3. 使用静态分配代替动态分配
```

### LCD 显示问题

#### 问题：LCD 不显示

**解决方案**：
1. 检查 SPI 连接
2. 检查背光电源
3. 验证 LCD 型号（ST7789）
4. 检查初始化代码

## 性能优化

### 1. 降低功耗

```c
// 启用 Light Sleep
esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

// 降低 CPU 频率
esp_pm_config_esp32s3_t pm_config = {
    .max_freq_mhz = 160,
    .min_freq_mhz = 40,
};
esp_pm_configure(&pm_config);
```

### 2. 优化 WiFi 性能

```c
// 禁用 WiFi 节能模式
esp_wifi_set_ps(WIFI_PS_NONE);

// 设置 WiFi 国家代码
wifi_country_t country = {
    .cc = "CN",
    .schan = 1,
    .nchan = 13,
};
esp_wifi_set_country(&country);
```

### 3. 优化 MQTT

```c
// 增加缓冲区大小
mqtt_cfg.buffer_size = 4096;

// 启用 Keep Alive
mqtt_cfg.keepalive = 120;
```

## 安全建议

### 1. WiFi 安全

- 使用 WPA2/WPA3 加密
- 避免硬编码 WiFi 密码
- 定期更换密码

### 2. MQTT 安全

**生产环境必须**：
- 启用 TLS/SSL 加密
- 使用用户名和密码认证
- 验证服务器证书

```c
// 启用 TLS
mqtt_cfg.transport = MQTT_TRANSPORT_OVER_SSL;
mqtt_cfg.cert_pem = (const char *)server_cert_pem_start;
```

### 3. OTA 安全

- 验证固件签名
- 使用 HTTPS 下载固件
- 验证固件 MD5/SHA256

### 4. 数据安全

- 敏感数据加密存储
- 使用 NVS 加密
- 定期清理日志中的敏感信息

## 相关文档

### 功能指南
- [WiFi 配网指南](WIFI_CONFIG_GUIDE.md) - 详细的配网流程和故障排查
- [OTA 升级指南](OTA_GUIDE.md) - 固件远程升级说明
- [OTA 集成文档](OTA_INTEGRATION.md) - OTA 功能集成指南
- [设备 API 参考](DEVICE_API_REFERENCE.md) - 完整的 MQTT API 文档

### 高级功能
- [Captive Portal 实现](CAPTIVE_PORTAL.md) - 强制门户实现细节
- [配网服务集成](PROVISIONING_SERVICE_INTEGRATION.md) - 与后端配网服务的集成

### 测试指南
- [ESP32-S3 测试指南](ESP32_S3_TEST_GUIDE.md) - 硬件测试流程

### 外部文档
- [ESP-IDF 编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32s3/)
- [FreeRTOS 文档](https://www.freertos.org/zh-cn-cmn-s/)
- [MQTT 协议规范](https://mqtt.org/)
- [LVGL 图形库](https://docs.lvgl.io/)

## 版本历史

### v1.1.0 (2025-11-10)
- ✅ 新增 PWM 输出控制功能（M2端口）
  - 支持自定义频率（1-40000 Hz）
  - 支持自定义占空比（0-100%）
  - 适用于电机、LED调光等应用
- ✅ PWM 预设控制功能
  - PWM渐变（Fade）：平滑过渡
  - PWM呼吸灯（Breathe）：循环呼吸效果
  - PWM步进（Step）：逐级调整
  - PWM脉冲（Pulse）：快速切换
  - PWM固定输出（Fixed）：恒定输出
- ✅ LCD 显示优化
  - MQTT状态实时更新
  - Uptime显示优化（小时/分钟格式）
- ✅ 预设控制增强
  - 舵机摆动（Swing）预设
  - LED波浪（Wave）效果增强
    - 支持循环和方向控制
    - **支持自定义LED序列**（可指定任意LED组合和顺序）
    - 例如：只点亮LED1→3→4，或按4→2→1→3的顺序流水

### v1.0.0 (2025-11-09)
- ✅ 基础功能完成
- ✅ WiFi 配网支持
- ✅ MQTT 通信实现
- ✅ DHT11、DS18B20 传感器支持
- ✅ LED、继电器、舵机控制
- ✅ LCD 显示功能
- ✅ OTA 升级支持

### 规划中
- 🔄 蓝牙配网
- 🔄 微信小程序控制
- 🔄 更多传感器支持
- 🔄 定时任务功能
- 🔄 场景联动

## 贡献指南

欢迎提交 Issue 和 Pull Request！

### 提交 Issue
- 清晰描述问题
- 提供复现步骤
- 附上日志信息
- 说明硬件环境

### 提交 Pull Request
- 遵循现有代码风格
- 添加必要的注释
- 更新相关文档
- 测试通过后再提交

## 许可证

[根据项目实际许可证填写]

## 技术支持

- **问题反馈**: 提交 Issue 到项目仓库
- **技术交流**: [建立技术交流群]
- **文档更新**: 2025-11-09

---

**开发环境**: ESP-IDF 5.4  
**目标芯片**: ESP32-S3  
**固件版本**: 1.0.0  
**维护者**: AIOT Admin Server Team
