# macOS 开发环境安装与编译部署指南

> 本文档用于在**全新的 macOS 系统**上搭建 CodeHubot Fireware 固件开发环境，并指导编译、烧录、合并固件与发布部署。  
> 可直接复制给 AI，按步骤在新机器上执行。

---

## 1. 项目概览

| 项目 | 路径 | 芯片 | Flash | 用途 |
|------|------|------|-------|------|
| **ESP32-S3 主固件** | `firmware/aiot-esp32` | ESP32-S3 | 16MB | 完整功能：LCD、OTA、多传感器、多板型 |
| **ESP32-C3 精简固件** | `firmware/aiot-esp32c3-lite` | ESP32-C3 | 4MB | 精简版：WiFi + MQTT + DHT11 + OLED，无 OTA |

**远程仓库：**

```
git@github.com:CodeHubot/CodeHubot-Fireware.git
https://github.com/CodeHubot/CodeHubot-Fireware
```

**推荐工具链版本（与项目一致）：**

- ESP-IDF：`release/v5.4`（发布说明中使用 5.4.3）
- Python：3.8+
- Git：2.x

---

## 2. 给 AI 的执行指令（可直接复制）

将以下内容粘贴到新 Mac 的 AI 对话中：

```
请在本机 macOS 上为 CodeHubot Fireware 项目搭建 ESP-IDF 开发环境并完成首次编译验证。

要求：
1. 安装 Xcode Command Line Tools（如未安装）
2. 安装 Homebrew（如未安装）
3. 安装 git、python3
4. 克隆 ESP-IDF release/v5.4 到 ~/esp/esp-idf
5. 执行 ./install.sh esp32s3 esp32c3
6. 克隆仓库到 ~/CodeHubot-Fireware（或用户指定目录）
7. 编译 firmware/aiot-esp32（ESP32-S3，当前默认板型 Rain）
8. 编译 firmware/aiot-esp32c3-lite（ESP32-C3）
9. 输出编译产物路径、固件大小、环境验证结果
10. 如有 USB 设备连接，列出 /dev/cu.* 串口

参考文档：仓库内 docs/MACOS_DEV_ENV_SETUP.md
不要修改 git config，不要自动 commit/push。
```

---

## 3. 系统前置条件

### 3.1 硬件建议

- Mac（Apple Silicon M 系列或 Intel 均可）
- 磁盘空闲空间 ≥ 15GB（ESP-IDF + 编译缓存）
- ESP32 开发板 + **支持数据传输的 USB 线**（非仅充电线）
- （可选）USB 转串口芯片驱动：CH340 / CP2102 / FTDI

### 3.2 安装基础工具

```bash
# 1. Xcode 命令行工具（编译必需）
xcode-select --install

# 2. Homebrew（推荐）
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# 3. 基础依赖
brew install git python3 cmake ninja dfu-util

# 4. 验证
git --version
python3 --version
```

---

## 4. 安装 ESP-IDF 5.4

```bash
# 创建工作目录
mkdir -p ~/esp
cd ~/esp

# 克隆 ESP-IDF（必须 --recursive）
git clone --recursive https://github.com/espressif/esp-idf.git
cd esp-idf

# 切换到项目使用的版本分支
git checkout release/v5.4
git submodule update --init --recursive

# 安装工具链（S3 + C3 两个目标一起装）
./install.sh esp32s3 esp32c3

# 激活环境（每个新终端都要执行）
. ~/esp/esp-idf/export.sh

# 验证
idf.py --version
echo $IDF_PATH
```

### 4.1 持久化环境变量（推荐）

在 `~/.zshrc` 或 `~/.bash_profile` 末尾添加：

```bash
alias get_idf='. $HOME/esp/esp-idf/export.sh'
```

以后每次打开终端执行：

```bash
get_idf
```

---

## 5. 克隆项目代码

```bash
# SSH（需已配置 GitHub SSH Key）
git clone git@github.com:CodeHubot/CodeHubot-Fireware.git ~/CodeHubot-Fireware

# 或 HTTPS
git clone https://github.com/CodeHubot/CodeHubot-Fireware.git ~/CodeHubot-Fireware

cd ~/CodeHubot-Fireware
git status
git log -1 --oneline
```

---

## 6. ESP32-S3 主固件（aiot-esp32）

### 6.1 项目说明

- 目标芯片：`esp32s3`
- 输出固件：`build/aiot-esp32s3-firmware.bin`
- Flash 分区：16MB，支持 OTA 双分区
- 支持板型（在 `sdkconfig.defaults` 中选择）：

| 配置项 | 板型 | 产品代码 |
|--------|------|----------|
| `CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT=y` | 标准版 | ESP32-S3-Dev-01 |
| `CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_RAIN=y` | 雨水传感器版 | ESP32-S3-Rain-01 |
| `CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_LITE=y` | 精简版 | ESP32-S3-Lite-01 |

### 6.2 选择板型

编辑 `firmware/aiot-esp32/sdkconfig.defaults`，确保**只有一个**板型为 `y`：

```ini
# CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT=y
CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_RAIN=y
# CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_LITE=y
```

切换板型后建议清理旧配置：

```bash
cd ~/CodeHubot-Fireware/firmware/aiot-esp32
rm -f sdkconfig
```

### 6.3 编译

```bash
get_idf   # 或 . ~/esp/esp-idf/export.sh

cd ~/CodeHubot-Fireware/firmware/aiot-esp32

idf.py set-target esp32s3
idf.py build
```

或使用脚本：

```bash
./build_esp_idf.sh check
./build_esp_idf.sh build
```

编译成功后主要产物：

```
build/bootloader/bootloader.bin
build/partition_table/partition-table.bin
build/ota_data_initial.bin
build/aiot-esp32s3-firmware.bin
```

### 6.4 烧录（开发调试）

```bash
# 查看串口
ls /dev/cu.usbserial-* /dev/cu.wchusbserial* /dev/cu.usbmodem* 2>/dev/null

# 擦除 Flash（首次烧录或换板型建议执行）
python -m esptool --chip esp32s3 --port /dev/cu.usbserial-XXXX erase_flash

# 烧录并监控
idf.py -p /dev/cu.usbserial-XXXX flash monitor
# 退出 monitor：Ctrl + ]
```

**Flash 参数（ESP32-S3）：**

| 参数 | 值 |
|------|-----|
| 芯片 | esp32s3 |
| Flash 大小 | 16MB |
| Flash 模式 | dio |
| Flash 频率 | 80m |
| 串口波特率 | 460800（失败可降为 115200） |

### 6.5 生成合并固件（生产烧录 / 分发）

合并固件便于使用烧录工具一次性写入，适合批量生产：

```bash
cd ~/CodeHubot-Fireware/firmware/aiot-esp32

python -m esptool --chip esp32s3 merge_bin \
  -o build/ESP32-S3-Rain-01-merged.bin \
  --flash_mode dio --flash_freq 80m --flash_size 16MB \
  0x0      build/bootloader/bootloader.bin \
  0x8000   build/partition_table/partition-table.bin \
  0x10000  build/aiot-esp32s3-firmware.bin \
  0x610000 build/ota_data_initial.bin
```

烧录合并固件：

```bash
python -m esptool --chip esp32s3 --port /dev/cu.usbserial-XXXX --baud 460800 \
  --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x0 build/ESP32-S3-Rain-01-merged.bin
```

> 合并文件名建议按板型和版本命名，例如 `ESP32-S3-Rain-01-v1.7.bin`，可放入 `docs/firmware/` 归档。

### 6.6 OTA 远程升级

ESP32-S3 主固件支持 OTA，通常只需上传 app 分区固件：

```
build/aiot-esp32s3-firmware.bin
```

详细流程见：`firmware/aiot-esp32/OTA_GUIDE.md`

---

## 7. ESP32-C3 精简固件（aiot-esp32c3-lite）

### 7.1 项目说明

- 目标芯片：`esp32c3`
- 输出固件：`build/aiot-esp32c3-lite.bin`
- Flash 分区：4MB，**无 OTA**
- 功能：WiFi 配网、MQTT、DHT11、SSD1306 OLED

### 7.2 编译

```bash
get_idf

cd ~/CodeHubot-Fireware/firmware/aiot-esp32c3-lite

./build.sh build
# 等价于：idf.py set-target esp32c3 && idf.py build
```

### 7.3 烧录

```bash
# 方式一：脚本
./build.sh flash /dev/cu.usbserial-XXXX
./build.sh monitor /dev/cu.usbserial-XXXX
./build.sh fm /dev/cu.usbserial-XXXX   # 烧录 + 监控

# 方式二：idf.py
idf.py -p /dev/cu.usbserial-XXXX flash monitor
```

**Flash 参数（ESP32-C3）：**

| 参数 | 值 |
|------|-----|
| 芯片 | esp32c3 |
| Flash 大小 | 4MB |
| Flash 模式 | dio |
| Flash 频率 | 80m |

### 7.4 生成合并固件（推荐生产使用）

```bash
cd ~/CodeHubot-Fireware/firmware/aiot-esp32c3-lite

./build.sh merge
# 或
./merge_firmware.sh
```

输出目录：

```
build/merged/aiot-esp32c3-lite_merged.bin          # 4MB 完整镜像
build/merged/aiot-esp32c3-lite_v1.0.0_YYYYMMDD.bin
build/merged/FLASH_INSTRUCTIONS.txt
```

烧录合并固件（**地址必须是 0x0**）：

```bash
python -m esptool --chip esp32c3 --port /dev/cu.usbserial-XXXX --baud 460800 \
  --before default_reset --after hard_reset write_flash \
  --flash_mode dio --flash_size 4MB --flash_freq 80m \
  0x0 build/merged/aiot-esp32c3-lite_merged.bin
```

更多说明见：

- `firmware/aiot-esp32c3-lite/MERGED_FIRMWARE_GUIDE.md`
- `firmware/aiot-esp32c3-lite/QUICK_FLASH_GUIDE.md`

---

## 8. 串口与 USB 驱动（macOS）

### 8.1 查找串口

```bash
ls /dev/cu.*
ls /dev/cu.usbserial-* /dev/cu.wchusbserial* /dev/cu.usbmodem* 2>/dev/null
```

常见命名：

| 芯片 | 典型串口名 |
|------|-----------|
| CH340 | `/dev/cu.wchusbserial*` |
| CP2102 | `/dev/cu.usbserial-*` |
| 原生 USB-JTAG | `/dev/cu.usbmodem*` |

### 8.2 驱动下载

- CH340：https://www.wch.cn/downloads/CH341SER_MAC_ZIP.html
- CP2102：https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers

### 8.3 烧录失败时的处理

1. 关闭所有占用串口的程序（monitor、Arduino IDE 等）
2. 重新插拔 USB
3. 手动进入下载模式：按住 **BOOT** → 按一下 **RESET** → 松开 **BOOT**
4. 降低波特率：`--baud 115200`
5. 先擦除再烧录：`python -m esptool --chip <chip> --port <port> erase_flash`

---

## 9. 首次设备使用（配网）

两个固件配网流程类似：

1. 上电后 3 秒内长按 **Boot 键** 可强制进入配网（部分固件首次启动自动配网）
2. 手机连接设备热点：
   - S3：`AIOT-Config-XXXXXX` 或类似名称
   - C3：`AIOT-C3-XXXXXX`
3. 浏览器访问 `http://192.168.4.1`（Captive Portal 可能自动弹出）
4. 填写 WiFi 密码、配置服务器地址
5. 保存后设备重启并连接 WiFi / MQTT

---

## 10. 环境验证清单

在新 Mac 上完成以下检查，即表示环境可用：

```bash
# [ ] ESP-IDF 已激活
echo $IDF_PATH
idf.py --version

# [ ] 仓库已克隆
test -d ~/CodeHubot-Fireware/firmware/aiot-esp32 && echo OK

# [ ] S3 编译通过
cd ~/CodeHubot-Fireware/firmware/aiot-esp32
idf.py build
test -f build/aiot-esp32s3-firmware.bin && echo "S3 firmware OK"

# [ ] C3 编译通过
cd ~/CodeHubot-Fireware/firmware/aiot-esp32c3-lite
./build.sh build
test -f build/aiot-esp32c3-lite.bin && echo "C3 firmware OK"

# [ ] esptool 可用
python -m esptool version

# [ ] 串口可见（接设备时）
ls /dev/cu.*
```

---

## 11. 常见问题

### Q1: `IDF_PATH` 为空 / 找不到 idf.py

```bash
. ~/esp/esp-idf/export.sh
# 或
get_idf
```

### Q2: 编译报 Python 包缺失

```bash
cd ~/esp/esp-idf
./install.sh esp32s3 esp32c3
. ./export.sh
```

### Q3: 切换 S3 板型后编译异常

```bash
cd firmware/aiot-esp32
rm -f sdkconfig
idf.py set-target esp32s3
idf.py build
```

### Q4: macOS 不允许打开驱动

`系统设置 → 隐私与安全性` 中允许对应内核扩展/驱动。

### Q5: 两个项目来回编译

每次切换项目前确保 `get_idf` 已执行；若 target 不同，`idf.py set-target` 会触发 reconfigure，首次较慢属正常。

### Q6: 合并固件烧录后无法启动

- 确认烧录地址（C3 合并固件必须是 `0x0`）
- 确认 Flash 大小参数与实际硬件一致（S3=16MB，C3=4MB）
- 先 `erase_flash` 再烧录

---

## 12. 相关文档索引

| 文档 | 说明 |
|------|------|
| `README.md` | 项目总览 |
| `firmware/aiot-esp32/README.md` | S3 固件详细说明 |
| `firmware/aiot-esp32/BOARD_SELECTION_GUIDE.md` | S3 板型切换 |
| `firmware/aiot-esp32/OTA_GUIDE.md` | OTA 升级 |
| `firmware/aiot-esp32/DEVICE_API_REFERENCE.md` | MQTT 协议 |
| `firmware/aiot-esp32c3-lite/README.md` | C3 精简固件说明 |
| `firmware/aiot-esp32c3-lite/MERGED_FIRMWARE_GUIDE.md` | C3 合并固件烧录 |
| `docs/quick-start.html` | 用户向快速开始（Web 文档） |

---

## 13. 快速命令速查

```bash
# ========== 环境 ==========
get_idf

# ========== ESP32-S3 ==========
cd ~/CodeHubot-Fireware/firmware/aiot-esp32
idf.py set-target esp32s3 && idf.py build
idf.py -p PORT flash monitor

# ========== ESP32-C3 ==========
cd ~/CodeHubot-Fireware/firmware/aiot-esp32c3-lite
./build.sh merge
./build.sh fm PORT

# ========== 合并固件烧录 ==========
# S3 (16MB, 地址 0x0)
python -m esptool --chip esp32s3 --port PORT write_flash \
  --flash_mode dio --flash_size 16MB --flash_freq 80m \
  0x0 build/ESP32-S3-Rain-01-merged.bin

# C3 (4MB, 地址 0x0)
python -m esptool --chip esp32c3 --port PORT write_flash \
  --flash_mode dio --flash_size 4MB --flash_freq 80m \
  0x0 build/merged/aiot-esp32c3-lite_merged.bin
```

---

**文档版本：** 1.0  
**适用仓库：** CodeHubot/CodeHubot-Fireware  
**最后更新：** 2026-03-21
