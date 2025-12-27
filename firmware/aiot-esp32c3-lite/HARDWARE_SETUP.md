# ESP32-C3开发板硬件连接指南

## 📋 确认的GPIO映射

根据你的原理图，以下是确认的硬件连接：

| 功能 | GPIO | 说明 | 状态 |
|------|------|------|------|
| **OLED SDA** | GPIO4 | I2C数据线 | ✅ 已适配 |
| **OLED SCL** | GPIO5 | I2C时钟线 | ✅ 已适配 |
| **红色LED** | GPIO18 | LED指示灯1 | ✅ 已适配 |
| **蓝色LED** | GPIO19 | LED指示灯2 | ✅ 已适配 |
| **Boot按键** | GPIO9 | 长按3秒配网 | ✅ 已适配 |
| **Reset按键** | EN引脚 | 硬件复位 | ✅ 已适配 |
| **DHT11** | GPIO6 | 温湿度传感器 | ✅ 已改为GPIO6 |

## 🔌 硬件接线说明

### 1. OLED显示屏 (SSD1306 128x64)

```
OLED模块 → ESP32-C3
------------------------
VCC      → 3.3V
GND      → GND
SDA      → GPIO4
SCL      → GPIO5
```

**注意事项：**
- OLED I2C地址通常是0x3C（可能是0x3D）
- 如果显示不工作，检查I2C地址
- 确保OLED供电为3.3V（不是5V）

### 2. DHT11温湿度传感器

```
DHT11模块 → ESP32-C3
------------------------
VCC       → 3.3V (或5V，取决于模块)
DATA      → GPIO6 ⚠️ 注意：已从GPIO11改为GPIO6
GND       → GND
```

**重要提示：**
- ⚠️ 原理图上DHT11接的是GPIO11（Flash引脚），无法使用
- ✅ **请将DHT11的DATA线改接到GPIO6**
- DHT11需要4.7K-10K上拉电阻（通常模块自带）

### 3. LED指示灯

```
红色LED → GPIO18
蓝色LED → GPIO19
```

**电路说明：**
- ESP32-C3 → 限流电阻(330Ω-1KΩ) → LED+ → LED- → GND
- 或者：ESP32-C3 → LED+ → LED- → 限流电阻 → GND

### 4. 按键

```
Boot按键: GPIO9 → 按键 → GND (按下时GPIO9为低电平)
Reset按键: EN → 按键 → GND (按下复位芯片)
```

## 🔧 硬件修改建议

### 必须修改：DHT11连接

**原理图错误：GPIO11是Flash引脚**

需要飞线修改：
1. 断开DHT11的DATA与GPIO11的连接
2. 将DHT11的DATA线连接到GPIO6
3. 确保GPIO6没有被其他设备占用

**可选改接引脚：**
- GPIO6 (推荐，已在固件中配置) ✅
- GPIO7 (备选)
- GPIO10 (备选)

## 📊 GPIO资源分配表

ESP32-C3可用GPIO: GPIO0-10, GPIO18-21 (共22个)

| GPIO | 功能 | 方向 | 说明 |
|------|------|------|------|
| GPIO0 | I2C SCL备用 | - | 可用于其他功能 |
| GPIO1 | I2C SDA备用 | - | 可用于其他功能 |
| GPIO2 | 预留 | - | 可用 |
| GPIO3 | 预留 | - | 可用 |
| GPIO4 | OLED SDA | I/O | I2C数据线 ✅ |
| GPIO5 | OLED SCL | I/O | I2C时钟线 ✅ |
| GPIO6 | DHT11 DATA | I/O | 温湿度传感器 ✅ |
| GPIO7 | 预留 | - | 可用 |
| GPIO8 | 预留 | - | 可用 |
| GPIO9 | Boot按键 | Input | 配网按键 ✅ |
| GPIO10 | 预留 | - | 可用 |
| GPIO11-17 | Flash | - | ⚠️ 不可用（连接Flash芯片） |
| GPIO18 | 红色LED | Output | LED1 ✅ |
| GPIO19 | 蓝色LED | Output | LED2 ✅ |
| GPIO20 | 预留 | - | 可用 |
| GPIO21 | 预留 | - | 可用 |

## 🧪 硬件测试步骤

### 1. 基本电源测试
```bash
# 连接USB-C线
# 检查红色电源LED是否亮起
# 用万用表测量3.3V输出
```

### 2. LED测试
编译并烧录测试固件后：
- 红色LED (GPIO18) 应该可以闪烁
- 蓝色LED (GPIO19) 应该可以闪烁

### 3. OLED测试
启动后OLED应该显示：
```
AIOT ESP32-C3
Initializing
```

### 4. DHT11测试
查看串口输出，应该看到：
```
DHT11读取: 温度=xx.x°C, 湿度=xx.x%
```

### 5. WiFi配网测试
长按Boot按键3秒，OLED显示：
```
Config Mode
Connect to:
AIOT-C3-XXXXXX
192.168.4.1
```

## ⚡ 功耗估算

| 模式 | 电流 | 说明 |
|------|------|------|
| 正常运行 | ~80mA | WiFi+MQTT+OLED+传感器 |
| WiFi省电 | ~40mA | WiFi省电模式 |
| 待机 | ~20mA | WiFi保持，OLED关闭 |

**供电建议：**
- USB供电：5V 500mA (足够)
- 电池供电：3.7V锂电池 + DC-DC升压（需要至少500mA输出）

## 🔍 常见硬件问题

### OLED不显示
1. 检查I2C连线是否正确
2. 用万用表测量OLED的3.3V供电
3. 确认I2C地址（0x3C或0x3D）
4. 检查SDA/SCL是否接反

### DHT11读取失败
1. ⚠️ 确认已将DATA从GPIO11改到GPIO6
2. 检查供电（3.3V或5V）
3. 确认上拉电阻存在
4. DHT11读取间隔至少2秒

### LED不亮
1. 检查限流电阻
2. 确认LED极性
3. 测试GPIO18/GPIO19输出

### 无法烧录
1. 检查USB线是否支持数据传输
2. 手动进入下载模式：
   - 按住Boot按键
   - 按一下Reset按键
   - 释放Boot按键

## 📐 原理图建议

如果还在设计阶段，建议修改：

1. **DHT11连接** ⚠️ 
   - 原理图：GPIO11 → ❌ 错误（Flash引脚）
   - 建议：GPIO6 → ✅ 正确

2. **添加测试点**
   - 3.3V测试点
   - GND测试点
   - I2C SDA/SCL测试点

3. **LED指示**
   - 电源指示LED
   - WiFi状态LED
   - 用户LED

## 📞 硬件支持

如有硬件问题：
1. 检查本文档的接线说明
2. 使用万用表测量各引脚电压
3. 查看串口日志
4. 提供原理图和照片

---

**更新日期**: 2025-12-27  
**固件版本**: v1.1-HW  
**硬件版本**: ESP32-C3自定义板

