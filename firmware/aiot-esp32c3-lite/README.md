# AIOT ESP32-C3 Lite 精简固件

## 📋 项目说明

这是基于ESP32-C3芯片的精简版IoT设备固件，专为低成本、低功耗应用设计。

## ⚙️ 硬件配置

### 芯片规格
- **芯片型号**: ESP32-C3 (RISC-V 32位单核)
- **CPU频率**: 160MHz
- **Flash**: 4MB
- **RAM**: 400KB SRAM
- **无PSRAM**

### 核心特性
- ✅ WiFi 2.4GHz 802.11 b/g/n
- ✅ 蓝牙 5.0 LE
- ✅ 22个GPIO引脚
- ✅ 低功耗设计
- ❌ 无OTA升级 (节省Flash空间)
- ❌ 无LCD显示 (精简配置)

## 🎯 功能特性

### 支持的功能
- ✅ **WiFi配网**: Boot按键触发的Web配网
- ✅ **MQTT通信**: 设备与服务器双向通信
- ✅ **传感器**: DHT11温湿度传感器
- ✅ **控制设备**: LED、继电器
- ✅ **设备控制**: 基本的GPIO控制
- ✅ **低功耗模式**: 支持休眠唤醒

### 精简的功能 (相比完整版)
- ❌ 无OTA升级功能
- ❌ 无LVGL图形库
- ❌ 无LCD显示支持
- ❌ 无舵机控制
- ❌ 无DS18B20传感器
- ❌ 无预设控制功能
- ❌ 无PWM高级控制

## 📍 GPIO引脚映射

| 功能 | GPIO | 说明 |
|------|------|------|
| LED1 (内置) | GPIO8 | 状态指示灯 |
| 继电器1 | GPIO2 | 高功率设备控制 |
| DHT11传感器 | GPIO5 | 温湿度传感器 |
| Boot按键 | GPIO9 | 配网按键 |
| 用户按键 | GPIO6 | 用户自定义 |
| I2C SDA | GPIO1 | I2C数据线 |
| I2C SCL | GPIO0 | I2C时钟线 |

## 💾 Flash分区 (4MB 无OTA)

```
总容量: 4MB (4,194,304 字节)

分区布局:
├─ Bootloader    0x1000   (4KB)
├─ 分区表        0x1000   (4KB)
├─ NVS           0x6000   (24KB)   - WiFi配置
├─ PHY_init      0x1000   (4KB)    - 射频参数
├─ Factory应用   0x300000 (3MB)    - 主应用 ⭐ 无OTA双分区
├─ SPIFFS        0x80000  (512KB)  - 配网页面
└─ 用户数据      0x30000  (192KB)  - 传感器数据

优势:
✅ 单分区设计，无OTA分区浪费
✅ 应用可用3MB空间 (足够精简固件)
✅ 节省约2MB Flash空间
```

## 🚀 快速开始

### 1. 环境准备

```bash
# 安装 ESP-IDF 5.4
mkdir -p ~/esp
cd ~/esp
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
git checkout release/v5.4
./install.sh esp32c3

# 激活环境
. $HOME/esp/esp-idf/export.sh
```

### 2. 编译固件

**方式一：使用便捷脚本（推荐）**

```bash
# 编译固件
./build.sh build

# 编译并生成合并固件（推荐用于生产）
./build.sh merge

# 烧录
./build.sh flash

# 烧录并监控
./build.sh flash-monitor
```

**方式二：使用idf.py命令**

```bash
# 设置目标芯片
idf.py set-target esp32c3

# 编译
idf.py build

# 烧录
idf.py -p /dev/cu.usbserial-* flash monitor
```

### 3. 配网使用

1. **进入配网模式**
   - 首次启动自动进入配网
   - 或长按Boot按键3秒

2. **连接热点**
   - SSID: `AIOT-C3-XXXXXX`
   - 密码: 无密码

3. **配置WiFi**
   - 浏览器自动打开配网页面
   - 或访问 `http://192.168.4.1`

4. **完成**
   - 设备自动连接WiFi
   - LED闪烁表示连接成功

## 📊 固件大小对比

| 版本 | Flash | OTA | LVGL | 固件大小 | 可用空间 |
|------|-------|-----|------|----------|----------|
| ESP32-S3 完整版 | 16MB | ✅ | ✅ | 1.3MB | 2MB应用分区 |
| **ESP32-C3 精简版** | **4MB** | ❌ | ❌ | **~400KB** | **3MB应用分区** |

**节省空间:**
- 移除LVGL库: ~300KB
- 移除OTA功能: ~100KB
- 移除显示驱动: ~50KB
- 移除舵机/PWM: ~30KB
- 代码精简: ~120KB

## 🔧 编译优化

默认已启用的优化:
```c
// 优化等级: 大小优化
CONFIG_COMPILER_OPTIMIZATION_SIZE=y

// 禁用调试信息
CONFIG_COMPILER_OPTIMIZATION_ASSERTIONS_DISABLE=y

// 禁用不需要的功能
CONFIG_LWIP_IPV6=n                 // 无IPv6
CONFIG_BT_ENABLED=n                // 可选禁用蓝牙
CONFIG_SPIRAM=n                    // 无PSRAM
```

## 📡 MQTT通信协议

### 主题结构

**设备发布 (上报数据):**
```
devices/{device_id}/data        # 传感器数据
devices/{device_id}/status      # 设备状态
devices/{device_id}/heartbeat   # 心跳
```

**设备订阅 (接收命令):**
```
devices/{device_id}/control     # 控制命令
```

### 消息示例

**DHT11数据上报:**
```json
{
  "device_id": "C3-LITE-AABBCC",
  "sensor": "DHT11",
  "temperature": 25.5,
  "humidity": 60.2,
  "timestamp": 1699516800
}
```

**LED控制命令:**
```json
{
  "device_id": "C3-LITE-AABBCC",
  "type": "control",
  "port": "LED1",
  "value": 1
}
```

**继电器控制命令:**
```json
{
  "device_id": "C3-LITE-AABBCC",
  "type": "control",
  "port": "RELAY1",
  "value": 1
}
```

## 🔋 功耗优化

ESP32-C3的低功耗特性:

### 运行模式
- **正常运行**: ~40mA (WiFi开启)
- **WiFi省电**: ~15mA (DTIM3)
- **轻度睡眠**: ~3mA (CPU暂停)
- **深度睡眠**: ~5μA (仅RTC运行)

### 优化建议
```c
// 启用WiFi省电模式
esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

// 降低CPU频率
rtc_cpu_freq_config_t config;
rtc_clk_cpu_freq_mhz_to_config(80, &config);
rtc_clk_cpu_freq_set_config(&config);
```

## 📦 固件编译与烧录

### 合并固件生成（推荐用于生产）

生成单一的合并固件文件，便于批量烧录和分发：

```bash
# 编译并生成合并固件
./build.sh merge

# 或单独运行合并脚本
./merge_firmware.sh
```

**输出文件位置：** `build/merged/`
- `aiot-esp32c3-lite_merged.bin` - 合并固件（4MB完整镜像）
- `aiot-esp32c3-lite_v1.0.0_YYYYMMDD.bin` - 带版本号的固件
- `FLASH_INSTRUCTIONS.txt` - 详细烧录说明

### 烧录合并固件

**使用esptool.py（最通用）：**
```bash
python -m esptool --chip esp32c3 --port /dev/cu.usbserial-* --baud 460800 \
    --before default_reset --after hard_reset write_flash \
    --flash_mode dio --flash_size 4MB --flash_freq 80m \
    0x0 build/merged/aiot-esp32c3-lite_merged.bin
```

**使用乐鑫Flash下载工具：**
1. 下载：https://www.espressif.com/zh-hans/support/download/other-tools
2. 选择ESP32-C3芯片
3. 添加固件文件，地址设为 `0x0`
4. SPI配置：DIO, 80MHz, 4MB
5. 点击START烧录

**使用Web工具：**
- 访问：https://espressif.github.io/esptool-js/
- 选择固件，烧录地址 `0x0`

### 固件升级方式

由于精简版不支持OTA，固件升级方式:

**方式一: USB串口升级（推荐）**
```bash
# 使用分离固件（开发调试）
idf.py -p /dev/cu.usbserial-* flash

# 使用合并固件（生产环境）
python -m esptool --chip esp32c3 --port PORT write_flash 0x0 merged.bin
```

**方式二: 自定义升级**
如果需要远程升级，建议:
1. 使用外部MCU做代理
2. 或使用ESP32-C3的ROM Bootloader
3. 或考虑添加简化的OTA支持

## 🛠️ 故障排查

### 编译问题

**Q: 找不到LVGL库**
A: 正常，精简版已移除LVGL依赖

**Q: 编译失败 - 找不到OTA头文件**
A: 检查是否正确移除了OTA相关代码

### 运行问题

**Q: 设备无法连接WiFi**
A: 
1. 检查WiFi信号强度
2. 确认密码正确
3. 查看串口日志

**Q: MQTT连接失败**
A:
1. 检查服务器地址
2. 确认端口开放
3. 验证设备ID格式

## 📚 相关文档

- [ESP32-C3技术参考手册](https://www.espressif.com/sites/default/files/documentation/esp32-c3_technical_reference_manual_cn.pdf)
- [ESP-IDF编程指南](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32c3/)
- [项目后端文档](../../backend/README.md)

## 🎓 适用场景

### 推荐使用
✅ 简单的传感器节点
✅ 开关控制设备
✅ 低成本IoT终端
✅ 电池供电设备
✅ 批量部署项目

### 不推荐使用
❌ 需要复杂UI显示
❌ 需要远程OTA升级
❌ 需要多个传感器
❌ 需要舵机控制
❌ 对实时性要求极高

## 📊 成本分析

| 组件 | ESP32-S3完整版 | ESP32-C3精简版 | 节省 |
|------|----------------|----------------|------|
| 芯片 | $2.5 | $1.5 | $1.0 |
| Flash (16MB vs 4MB) | $1.5 | $0.5 | $1.0 |
| LCD屏幕 | $3.0 | $0 | $3.0 |
| 其他外设 | $1.0 | $0.5 | $0.5 |
| **总计** | **$8.0** | **$2.5** | **$5.5** |

**批量生产优势:**
- 每台节省约$5.5
- 1000台节省$5,500
- 10000台节省$55,000

## 🔄 版本历史

### v1.0.0 (2025-12-27)
- ✅ 初始版本发布
- ✅ ESP32-C3支持
- ✅ 基础WiFi配网
- ✅ MQTT通信
- ✅ DHT11传感器
- ✅ LED和继电器控制
- ✅ 精简4MB Flash支持

## 📝 许可证

[根据主项目许可证]

## 💬 技术支持

- **问题反馈**: 提交Issue到项目仓库
- **更新日期**: 2025-12-27
- **维护者**: AIOT Admin Server Team

