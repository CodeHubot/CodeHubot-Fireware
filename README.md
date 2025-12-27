# CodeHubot Fireware

ESP32 系列芯片的物联网设备固件项目，提供完整的设备管理、传感器数据采集、远程控制和 OTA 升级功能。

## 📋 项目简介

CodeHubot Fireware 是一个基于 ESP-IDF 5.4 框架开发的物联网设备固件，支持 ESP32-S3、ESP32-C3 等多种芯片平台。固件提供了完整的设备管理功能，包括 WiFi 配网、MQTT 通信、传感器数据采集、设备控制和 OTA 升级等核心功能。

## ✨ 核心特性

### 网络连接
- ✅ **WiFi 配网**: 支持 Boot 按键触发的 Web 配网功能，集成 Captive Portal 实现自动跳转
- ✅ **MQTT 通信**: 完整的 MQTT 客户端实现，支持 QoS 0/1/2 和自动重连
- ✅ **设备注册**: 通过 MAC 地址自动注册设备，获取 MQTT 凭证

### 传感器支持
- ✅ **DHT11**: 温湿度传感器
- ✅ **DS18B20**: 高精度温度传感器
- ✅ **可扩展**: 模块化设计，易于添加新传感器

### 设备控制
- ✅ **LED 控制**: 4 路 PWM 调光 LED，支持闪烁、波浪等预设效果
- ✅ **继电器控制**: 2 路继电器，支持高功率设备控制
- ✅ **舵机控制**: PWM 舵机角度控制（0-180°），支持摆动预设
- ✅ **PWM 输出**: 自定义频率和占空比输出，支持渐变、呼吸灯、步进、脉冲等预设

### 系统功能
- ✅ **OTA 升级**: 支持远程固件升级，支持版本检测和回滚
- ✅ **LCD 显示**: 实时显示设备状态、传感器数据和网络信息
- ✅ **预设控制**: 丰富的预设指令库，支持复杂控制场景
- ✅ **硬件抽象**: 统一的 HAL 层，便于移植到不同硬件平台

## 🎯 支持的硬件平台

### 芯片支持
- ✅ **ESP32-S3** - 主力开发板（推荐）
- ✅ **ESP32-C3** - 经济型选择

### 开发板支持
- ESP32-S3-DevKit
- ESP32-S3-DevKit-Lite
- ESP32-S3-DevKit-Rain
- ESP32-C3-Mini

## 📁 项目结构

```
CodeHubot-Fireware/
├── firmware/                    # 固件源代码
│   ├── aiot-esp32/             # ESP32-S3 主固件
│   │   ├── main/               # 主应用程序
│   │   │   ├── wifi_config/    # WiFi 配网模块
│   │   │   ├── mqtt/           # MQTT 客户端
│   │   │   ├── device/         # 设备控制模块
│   │   │   ├── ota/            # OTA 升级模块
│   │   │   └── ...
│   │   ├── boards/             # 板级配置文件
│   │   ├── drivers/            # 外设驱动
│   │   └── components/         # 自定义组件
│   └── aiot-esp32c3-lite/      # ESP32-C3 精简版固件
├── docs/                        # 项目文档
│   ├── index.html              # 文档首页
│   ├── quick-start.html        # 快速开始指南
│   └── ...
└── README.md                    # 项目说明（本文件）
```

## 🚀 快速开始

### 环境要求

- **ESP-IDF**: 5.4 或更高版本
- **Python**: 3.8 或更高版本
- **操作系统**: Linux、macOS 或 Windows

### 编译和烧录

1. **安装 ESP-IDF**

```bash
# 克隆 ESP-IDF
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf
git checkout release/v5.4
./install.sh esp32s3

# 激活环境
. $HOME/esp/esp-idf/export.sh
```

2. **编译固件**

```bash
cd firmware/aiot-esp32
idf.py set-target esp32s3
idf.py build
```

3. **烧录固件**

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

### 设备配网

1. 长按 Boot 按键 3 秒进入配网模式
2. 连接设备创建的 WiFi 热点（`AIOT-Config-XXXXXX`）
3. 浏览器会自动打开配网页面（Captive Portal）
4. 输入 WiFi 名称、密码和服务器地址
5. 设备自动连接并注册到服务器

详细配网说明请参考：[快速开始指南](docs/quick-start.html)

## 📚 文档

完整的项目文档位于 `docs/` 目录：

- [快速开始](docs/quick-start.html) - 10 分钟上手指南
- [固件烧录指南](docs/firmware-flash.html) - 固件烧录详细步骤
- [设备 API 参考](firmware/aiot-esp32/DEVICE_API_REFERENCE.md) - MQTT 通信协议
- [OTA 升级指南](firmware/aiot-esp32/OTA_GUIDE.md) - 远程固件升级
- [板级配置指南](firmware/aiot-esp32/BOARD_SELECTION_GUIDE.md) - 硬件配置说明

## 🔧 开发指南

### 添加新传感器

1. 在 `drivers/sensors/` 中创建传感器驱动
2. 在 `main/device/sensor_manager.c` 中注册传感器
3. 配置 MQTT 数据上报主题

### 添加新控制设备

1. 在 `boards/` 中定义 GPIO 引脚
2. 在 `main/device/device_control.c` 中实现控制逻辑
3. 添加 MQTT 命令处理

### 自定义板级配置

1. 在 `boards/` 目录创建新的板级配置目录
2. 实现 `board_config.h` 和 BSP 文件
3. 在 `CMakeLists.txt` 中添加板级选项

详细开发文档请参考：[固件开发文档](firmware/aiot-esp32/README.md)

## 🛠️ 技术架构

### 分层设计

```
应用层 (Application Layer)
    ├── 设备控制 (Device Control)
    ├── 传感器管理 (Sensor Manager)
    ├── MQTT 通信 (MQTT Client)
    └── OTA 升级 (OTA Manager)
         ↓
硬件抽象层 (HAL Layer)
    ├── GPIO 控制
    ├── PWM 控制
    ├── 传感器接口
    └── 显示接口
         ↓
板级支持包 (BSP Layer)
    ├── ESP32-S3 配置
    ├── ESP32-C3 配置
    └── 外设驱动
```

### 核心模块

- **WiFi 配网**: 基于 Captive Portal 的 Web 配网，支持自动跳转
- **MQTT 客户端**: 完整的 MQTT 3.1.1 协议实现，支持 QoS 和自动重连
- **设备控制**: 统一的设备控制接口，支持 LED、继电器、舵机、PWM 等
- **OTA 升级**: 支持 HTTP/HTTPS 固件下载和版本管理
- **传感器管理**: 模块化传感器驱动，易于扩展

## 📦 功能模块

### WiFi 配网模块

- Boot 按键触发配网模式
- Captive Portal 自动跳转
- Web 界面配置 WiFi 和服务器地址
- 配置信息持久化存储（NVS）

### MQTT 通信模块

- 设备自动注册获取凭证
- 双向通信（上报数据 + 接收控制命令）
- QoS 0/1/2 支持
- 自动重连机制
- 心跳保活

### 设备控制模块

- **LED 控制**: 4 路 PWM 调光，支持闪烁、波浪预设
- **继电器控制**: 2 路数字输出
- **舵机控制**: PWM 角度控制，支持摆动预设
- **PWM 输出**: 自定义频率和占空比，支持渐变、呼吸灯、步进、脉冲、固定输出预设

### OTA 升级模块

- 版本检测和比较
- HTTP/HTTPS 固件下载
- 固件校验（MD5）
- 升级进度显示
- 失败回滚机制

## 🔐 安全特性

- WiFi 密码加密存储（NVS）
- MQTT 用户名密码认证
- 设备 MAC 地址唯一标识
- OTA 固件 MD5 校验
- 敏感信息已从代码中移除（适合开源）

## 📄 许可证

本项目采用 MIT 许可证，详见 [LICENSE](LICENSE) 文件。

## 🙏 致谢

本项目在开发过程中参考和借鉴了以下优秀的开源项目：

### [xiaozhi-esp32](https://github.com/78/xiaozhi-esp32)

感谢 [xiaozhi-esp32](https://github.com/78/xiaozhi-esp32) 项目提供的优秀实现参考，特别是在以下方面：

- **Captive Portal 实现**: 学习了 xiaozhi-esp32 的 Captive Portal 架构，实现了自动跳转的配网页面
- **WiFi 配网流程**: 参考了其配网页面的设计和交互逻辑
- **代码架构设计**: 借鉴了其模块化设计和代码组织方式

xiaozhi-esp32 是一个基于 MCP 协议的聊天机器人项目，支持语音交互和多种硬件平台，为 ESP32 开发者提供了很好的参考。我们在此向 xiaozhi-esp32 项目及其贡献者表示诚挚的感谢！

### 其他开源项目

- **ESP-IDF**: Espressif 官方开发框架
- **LVGL**: 嵌入式图形库
- **FreeRTOS**: 实时操作系统内核

## 🤝 贡献

欢迎提交 Issue 和 Pull Request！

### 贡献指南

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启 Pull Request

### 代码规范

- 遵循 ESP-IDF 代码风格
- 添加必要的注释和文档
- 确保代码通过编译和测试
- 更新相关文档

## 📞 联系方式

- **项目仓库**: [CodeHubot/CodeHubot-Fireware](https://github.com/CodeHubot/CodeHubot-Fireware)
- **问题反馈**: 提交 [Issue](https://github.com/CodeHubot/CodeHubot-Fireware/issues)
- **文档网站**: 查看 `docs/` 目录下的 HTML 文档

## 📈 版本历史

### v1.7 (当前版本)
- ✅ 完善 WiFi 配网功能
- ✅ 优化 MQTT 通信稳定性
- ✅ 增强设备控制功能
- ✅ 改进 OTA 升级流程
- ✅ 清理敏感信息，准备开源

### 规划中
- 🔄 更多传感器支持
- 🔄 蓝牙配网功能
- 🔄 场景联动功能
- 🔄 定时任务功能



