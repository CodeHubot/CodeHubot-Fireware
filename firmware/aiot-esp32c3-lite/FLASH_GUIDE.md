# ESP32-C3 固件烧录指南

## 🚨 重要：解决分区表错误

如果你看到以下错误信息：
```
E (55) flash_parts: partition 0 invalid magic number 0xddcd
E (60) boot: Failed to verify partition table
E (64) boot: load partition table error!
```

**原因**: Flash中的分区表损坏或未正确烧录。

**解决方法**: 使用完整烧录脚本擦除Flash并重新烧录所有内容。

---

## 📋 烧录内容说明

ESP32-C3固件包含3个部分，**必须全部烧录**：

| 文件 | 地址 | 大小 | 说明 |
|------|------|------|------|
| `bootloader.bin` | 0x0 | 20 KB | 引导加载程序 |
| `partition-table.bin` | 0x8000 | 3 KB | 分区表 |
| `aiot-esp32c3-lite.bin` | 0x10000 | 858 KB | 应用程序 |

---

## 🚀 方法1: 使用完整烧录脚本（推荐）

### 步骤1: 运行脚本
```bash
cd /Users/zhangqixun/AICodeing/AIOT-Admin-Server/firmware/aiot-esp32c3-lite
./flash_full.sh
```

### 步骤2: 按提示操作
- 脚本会自动检测串口
- 确认后会先**擦除Flash**
- 然后烧录所有文件

### 特点
- ✅ 自动擦除Flash
- ✅ 自动烧录所有文件
- ✅ 自动检测串口
- ✅ 详细的错误提示
- ✅ 烧录后的使用说明

---

## 🔧 方法2: 使用idf.py（完整擦除）

### 步骤1: 擦除Flash
```bash
cd /Users/zhangqixun/AICodeing/AIOT-Admin-Server/firmware/aiot-esp32c3-lite
. $HOME/esp/esp-idf/export.sh
idf.py -p /dev/cu.usbserial-XXXX erase_flash
```

### 步骤2: 烧录固件
```bash
idf.py -p /dev/cu.usbserial-XXXX flash
```

---

## 🛠️ 方法3: 手动使用esptool.py

### 完整命令（擦除+烧录）
```bash
# 1. 擦除Flash
python -m esptool --chip esp32c3 --port /dev/cu.usbserial-XXXX erase_flash

# 2. 烧录所有文件
python -m esptool --chip esp32c3 --port /dev/cu.usbserial-XXXX --baud 460800 \
  --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_size 4MB --flash_freq 80m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x10000 build/aiot-esp32c3-lite.bin
```

**注意**: 请将 `/dev/cu.usbserial-XXXX` 替换为实际的串口号。

---

## 🔍 串口查找

### macOS
```bash
ls /dev/cu.*
# 常见的：
# - /dev/cu.usbserial-*
# - /dev/cu.wchusbserial*
# - /dev/cu.SLAB_USBtoUART
```

### Linux
```bash
ls /dev/ttyUSB* /dev/ttyACM*
# 常见的：
# - /dev/ttyUSB0
# - /dev/ttyACM0
```

### Windows
- COM3, COM4, COM5 等
- 在设备管理器中查看

---

## ⚠️ 常见问题

### 1. 找不到串口设备
**问题**: `未检测到串口设备`

**解决**:
- 检查USB线是否支持数据传输（不是只充电线）
- 检查驱动是否已安装：
  - CP210x驱动: https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers
  - CH340驱动: http://www.wch.cn/downloads/CH341SER_MAC_ZIP.html
- 重新插拔USB线

### 2. 串口被占用
**问题**: `Serial port is already used`

**解决**:
- 关闭所有串口监视器（Arduino IDE、PlatformIO、idf.py monitor等）
- 关闭其他可能占用串口的程序

### 3. 无法进入下载模式
**问题**: `Failed to connect to ESP32`

**解决**:
- 手动进入下载模式：
  1. 按住**Boot按键**（GPIO9）
  2. 按下**Reset按键**
  3. 松开**Reset按键**
  4. 松开**Boot按键**
- 然后再运行烧录命令

### 4. 烧录后仍然报错
**问题**: 烧录成功但启动时还是有错误

**解决**:
- 确保使用了 `erase_flash` 擦除了整个Flash
- 确保烧录了所有3个文件（bootloader、分区表、应用）
- 检查地址是否正确：
  - Bootloader: 0x0
  - 分区表: 0x8000
  - 应用程序: 0x10000

### 5. 分区表魔数错误
**问题**: `partition 0 invalid magic number 0xddcd`

**解决**:
- 这是本指南要解决的核心问题
- **必须先擦除Flash**：`idf.py erase_flash`
- 然后使用完整烧录命令

---

## 📱 烧录后的验证

### 查看串口输出
```bash
cd /Users/zhangqixun/AICodeing/AIOT-Admin-Server/firmware/aiot-esp32c3-lite
. $HOME/esp/esp-idf/export.sh
idf.py -p /dev/cu.usbserial-XXXX monitor
```

### 正常启动日志应该包含：
```
I (30) boot: ESP-IDF v5.4.3-dirty 2nd stage bootloader
I (50) boot: SPI Flash Size : 4MB
I (XX) cpu_start: Pro cpu up
I (XX) cpu_start: Starting scheduler
I (XX) AIOT_MAIN: 🚀 AIOT ESP32-C3 Lite 系统启动
I (XX) AIOT_MAIN: 芯片型号: ESP32-C3
I (XX) AIOT_MAIN: 设备ID: AIOTC3-XXXXXX
```

### LED状态指示
- **蓝色LED常亮**: 配网模式
- **红色LED闪烁**: 正常运行

### OLED显示
- 启动画面（Logo）
- 配网提示（如果首次启动）
- 运行状态（WiFi、MQTT、温湿度）

---

## 🎯 首次使用流程

### 1. 烧录固件
```bash
./flash_full.sh
```

### 2. 等待启动
- 设备会自动重启
- OLED显示启动画面
- 如果没有WiFi配置，进入配网模式

### 3. 配网（首次使用）
- **指示**: 蓝色LED常亮
- **WiFi热点**: AIOT-C3-XXXXXX（XXXXXX为MAC地址后6位）
- **密码**: 无（开放网络）

### 4. 配置设备
- 手机/电脑连接WiFi热点
- 浏览器自动弹出配置页面
- 或手动访问: http://192.168.4.1
- 输入：
  - WiFi SSID
  - WiFi密码
  - MQTT服务器地址

### 5. 正常运行
- 配置成功后，设备重启
- 连接到配置的WiFi
- 连接到MQTT服务器
- 红色LED定期闪烁
- OLED显示运行状态

---

## 📊 烧录参数说明

| 参数 | 值 | 说明 |
|------|------|------|
| `--chip` | esp32c3 | 目标芯片 |
| `--baud` | 460800 | 烧录波特率 |
| `--flash_mode` | dio | Flash模式（DIO） |
| `--flash_size` | 4MB | Flash大小 |
| `--flash_freq` | 80m | Flash频率（80MHz） |
| `--before` | default_reset | 烧录前复位 |
| `--after` | hard_reset | 烧录后硬复位 |

---

## 🔄 开发调试流程

### 编译 → 烧录 → 监控（一条命令）
```bash
./build.sh all
# 或
idf.py build flash monitor
```

### 只烧录应用程序（不擦除配置）
```bash
idf.py app-flash
```

### 只查看日志
```bash
./build.sh monitor
# 或
idf.py monitor
```

---

## 📝 build.sh 脚本对比

| 脚本 | 用途 | 是否擦除Flash | 烧录内容 |
|------|------|---------------|----------|
| `flash_full.sh` | 完整烧录 | ✅ 是 | 全部（bootloader + 分区表 + 应用） |
| `build.sh flash` | 快速烧录 | ❌ 否 | 全部（bootloader + 分区表 + 应用） |
| `build.sh app-flash` | 仅应用 | ❌ 否 | 仅应用程序 |

**建议**:
- **首次烧录**: 使用 `flash_full.sh`（擦除Flash）
- **开发调试**: 使用 `build.sh flash` 或 `build.sh app-flash`
- **遇到问题**: 使用 `flash_full.sh` 重新完整烧录

---

## 💡 最佳实践

1. **首次烧录或遇到问题时**:
   ```bash
   ./flash_full.sh  # 完整擦除并烧录
   ```

2. **日常开发**:
   ```bash
   ./build.sh build    # 编译
   ./build.sh app-flash # 只烧录应用（保留配置）
   ./build.sh monitor   # 查看日志
   ```

3. **完整开发周期**:
   ```bash
   ./build.sh all  # 编译 + 烧录 + 监控
   ```

---

## 🎓 技术细节

### ESP32-C3 Flash布局（4MB）
```
0x000000 - 0x008000  (32 KB)   : Bootloader
0x008000 - 0x009000  (4 KB)    : 分区表（实际3KB）
0x009000 - 0x00F000  (24 KB)   : NVS (WiFi配置等)
0x00F000 - 0x010000  (4 KB)    : PHY初始化数据
0x010000 - 0x310000  (3 MB)    : 应用程序
0x310000 - 0x390000  (512 KB)  : SPIFFS
0x390000 - 0x3C0000  (192 KB)  : 用户数据
0x3C0000 - 0x3C8000  (32 KB)   : 系统配置
0x3C8000 - 0x3D0000  (32 KB)   : 日志
0x3D0000 - 0x400000  (192 KB)  : 预留
```

### 为什么需要擦除Flash？
- NVS区域可能有损坏的数据
- 旧的分区表可能不兼容
- 确保干净的启动环境

### 分区表魔数
- 正确的魔数: `0xAA50`
- 错误的魔数: `0xddcd` 或其他 → 表示Flash未正确写入或已损坏

---

**祝烧录顺利！** 🎉

如果遇到问题，请查看上面的"常见问题"部分，或使用 `./flash_full.sh` 脚本重新完整烧录。

