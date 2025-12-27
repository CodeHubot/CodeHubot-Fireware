# ESP32-S3-Lite-01 v1.6 固件发布说明

## 📦 固件信息

- **版本号**: v1.6
- **发布日期**: 2025-12-20
- **板子型号**: ESP32-S3-Lite-01（精简版）
- **固件大小**: 6.1 MB
- **编译环境**: ESP-IDF 5.4.3
- **芯片**: ESP32-S3

## ✨ 新增特性

### 1. 配网页面优化（测试版功能）

**注意**：此版本为历史开发版本，包含测试配置。开源版本已移除所有默认值，用户需手动输入配网信息。

### 2. 密码框明文显示

配网页面的密码输入框从 `type="password"` 改为 `type="text"`：

**之前**（v1.5）:
```
WiFi 密码: ••••••••••
```

**现在**（开源版本）:
```
WiFi 密码: （用户需输入）
```

**好处**：
- 可以直接看到密码内容
- 避免输入错误
- 调试更方便

### 3. 服务器地址更新

**注意**：开源版本已移除默认服务器地址，用户需在配网时手动输入。

## 🔧 硬件配置

### ESP32-S3-Lite-01 板子规格

**传感器**：
- ✅ DHT11 温湿度传感器 (GPIO35)

**控制设备**：
- ✅ 4×LED灯 (GPIO42, 41, 37, 36)
- ✅ 2×继电器 (GPIO1, 2)
- ✅ 2×PWM输出 (M1: GPIO48, M2: GPIO40)

**不支持**（与标准版区别）：
- ❌ DS18B20 温度传感器
- ❌ 舵机控制

## 📥 下载固件

### 方式一：直接下载

访问：[http://your-domain.com/docs/firmware-flash.html](http://your-domain.com/docs/firmware-flash.html)

或直接下载：
- **v1.6 最新版**: [ESP32-S3-Lite-01-v1.6.bin](../docs/firmware/ESP32-S3-Lite-01-v1.6.bin)
- **v1.5 稳定版**: [ESP32-S3-Lite-01-v1.5.bin](../docs/firmware/ESP32-S3-Lite-01-v1.5.bin)

### 方式二：从源码编译

```bash
cd /Users/zhangqixun/AICodeing/AIOT-Admin-Server/firmware/aiot-esp32
. $HOME/esp/esp-idf/export.sh
rm -f sdkconfig
idf.py set-target esp32s3
idf.py build
```

合并固件：
```bash
python -m esptool --chip esp32s3 merge_bin \
  -o build/ESP32-S3-Lite-01-v1.6-merged.bin \
  --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x10000 build/aiot-esp32s3-firmware.bin \
  0x610000 build/ota_data_initial.bin
```

## 🔥 烧录固件

### 方法一：使用 esptool（推荐）

```bash
# 擦除 Flash（推荐，首次烧录必须）
esptool.py --chip esp32s3 --port /dev/cu.usbserial-XXX erase_flash

# 烧录固件
esptool.py --chip esp32s3 --port /dev/cu.usbserial-XXX \
  --baud 460800 \
  --before default_reset --after hard_reset \
  write_flash \
  --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x0 ESP32-S3-Lite-01-v1.6.bin
```

### 方法二：Web 在线烧录

访问固件烧录页面：
```
http://your-domain.com/docs/firmware-flash.html
```

选择固件文件后点击"开始烧录"即可。

### 方法三：使用 idf.py

```bash
cd /Users/zhangqixun/AICodeing/AIOT-Admin-Server/firmware/aiot-esp32
. $HOME/esp/esp-idf/export.sh
idf.py -p /dev/cu.usbserial-XXX flash monitor
```

## ✅ 烧录后验证

### 1. 观察串口输出

```
[AIOT_MAIN] BSP初始化完成 (Lite板子)
[AIOT_MAIN] 📋 板子类型: ESP32-S3-DevKit-Lite
[AIOT_MAIN] 🏷️ 产品代码: ESP32-S3-Lite-01
```

### 2. 进入配网模式

首次烧录后，设备会自动进入配网模式：
- 手机连接热点：`AIOT-Config-XXXXXX`
- 浏览器访问：`http://192.168.4.1`
- 查看配网页面是否已预填充默认值

### 3. 测试配网

配网时需手动输入以下信息：
- WiFi: （您的WiFi名称）
- 密码: （您的WiFi密码）
- 服务器: （您的服务器地址，如 http://conf.example.com:8001）

## 📋 版本对比

| 特性 | v1.5 | v1.6 |
|------|------|------|
| 配网页面默认值 | 无 | ✅ 预填充 |
| 密码框显示 | 隐藏 | ✅ 明文 |
| 服务器地址 | （历史版本） | ✅ 用户手动输入 |
| 传感器支持 | DHT11 | DHT11 |
| 控制设备 | LED+继电器+PWM | LED+继电器+PWM |
| 适用场景 | 生产环境 | 测试环境 |

## 🔄 从 v1.5 升级到 v1.6

### 升级步骤

1. **备份配置**（如果需要）
2. **擦除 Flash**（推荐）:
   ```bash
   esptool.py --chip esp32s3 --port /dev/cu.usbserial-XXX erase_flash
   ```
3. **烧录新固件**:
   ```bash
   esptool.py --chip esp32s3 --port /dev/cu.usbserial-XXX \
     write_flash 0x0 ESP32-S3-Lite-01-v1.6.bin
   ```
4. **重新配网**

### 注意事项

- ⚠️ 升级会清除所有已保存的 WiFi 配置
- ⚠️ 需要重新进行配网
- ✅ 设备数据不会丢失（存储在服务器端）

## 🐛 已知问题

无已知严重问题。

## 📝 待办事项

- [ ] 添加配网页面配置开关（允许用户选择是否启用默认值）
- [ ] 添加密码显示/隐藏切换按钮
- [ ] 支持多组默认配置

## 🔗 相关文档

- [板子选择指南](../firmware/aiot-esp32/BOARD_SELECTION_GUIDE.md)
- [配网页面默认值修改指南](../firmware/aiot-esp32/CONFIG_PAGE_DEFAULT_VALUES.md)
- [固件手册](../firmware/FIRMWARE_MANUAL.md)
- [快速开始指南](../docs/quick-start.html)

## 📞 技术支持

- **问题反馈**: 提交 Issue 到项目仓库
- **固件文档**: `/Users/zhangqixun/AICodeing/AIOT-Admin-Server/firmware/`
- **在线文档**: http://your-domain.com/docs/

## 📜 更新日志

### v1.6 (2025-12-20)
- ✨ 新增：配网页面预填充默认值
- ✨ 新增：密码框明文显示
- 🔧 更新：服务器地址为 conf.aiot.powertechhub.com
- 🎯 适用：测试和开发环境

### v1.5 (2024-11-20)
- ✅ 稳定版本
- ✅ 支持 DHT11 传感器
- ✅ 支持 LED、继电器、PWM 控制
- 🎯 适用：生产环境

---

**编译环境**: ESP-IDF 5.4.3  
**目标芯片**: ESP32-S3  
**固件版本**: 1.6  
**发布日期**: 2025-12-20
