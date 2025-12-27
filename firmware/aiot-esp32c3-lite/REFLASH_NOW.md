# 🔧 ESP32-C3 重新烧录步骤

## 当前问题
分区表魔数错误，需要使用正确的Flash模式重新烧录。

## 🚨 立即执行的步骤

### 步骤1: 关闭监控程序
如果有 `idf.py monitor` 在运行，按 `Ctrl+]` 退出。

### 步骤2: 重新插拔USB
1. **拔掉** ESP32-C3的USB线
2. 等待 **5秒**
3. **重新插上** USB线
4. 等待设备被识别（约2-3秒）

### 步骤3: 确认串口
```bash
ls /dev/cu.* | grep -E "(usbmodem|usbserial)"
```

应该看到类似：`/dev/cu.usbmodem5B141411321`

### 步骤4: 擦除并重新烧录（使用QIO模式）

**一键完成命令**（复制整段执行）：
```bash
cd /Users/zhangqixun/AICodeing/AIOT-Admin-Server/firmware/aiot-esp32c3-lite && \
. $HOME/esp/esp-idf/export.sh && \
PORT=$(ls /dev/cu.* | grep -E "(usbmodem|usbserial)" | head -1) && \
echo "检测到串口: $PORT" && \
echo "=== 步骤1/2: 擦除Flash ===" && \
python -m esptool --chip esp32c3 --port $PORT --baud 460800 erase_flash && \
sleep 2 && \
echo "=== 步骤2/2: 烧录固件（QIO模式）===" && \
python -m esptool --chip esp32c3 --port $PORT --baud 460800 \
  --before default_reset --after hard_reset write_flash \
  --flash_mode qio --flash_size 4MB --flash_freq 80m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x10000 build/aiot-esp32c3-lite.bin && \
echo "" && \
echo "✅ 烧录完成！设备正在重启..." && \
sleep 3 && \
echo "查看日志：idf.py -p $PORT monitor"
```

---

## 📋 或者分步执行

### 1. 擦除Flash
```bash
cd /Users/zhangqixun/AICodeing/AIOT-Admin-Server/firmware/aiot-esp32c3-lite
. $HOME/esp/esp-idf/export.sh
PORT=$(ls /dev/cu.* | grep -E "(usbmodem|usbserial)" | head -1)
python -m esptool --chip esp32c3 --port $PORT --baud 460800 erase_flash
```

### 2. 烧录固件（QIO模式）
```bash
python -m esptool --chip esp32c3 --port $PORT --baud 460800 \
  --before default_reset --after hard_reset write_flash \
  --flash_mode qio --flash_size 4MB --flash_freq 80m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x10000 build/aiot-esp32c3-lite.bin
```

### 3. 查看日志
```bash
idf.py -p $PORT monitor
```

---

## ⚙️ 关键修改

**之前的问题**：使用了 `--flash_mode dio`  
**现在修复**：使用 `--flash_mode qio`（与bootloader配置匹配）

## 🎯 预期结果

烧录成功后，设备启动应该显示：

```
I (30) boot: ESP-IDF v5.4.3-dirty 2nd stage bootloader
I (39) boot.esp32c3: SPI Mode       : QIO
I (50) boot: Enabling RNG early entropy source...
I (60) boot: Partition Table:
I (64) boot: ## Label            Usage          Type ST Offset   Length
I (71) boot:  0 nvs              WiFi data        01 02 00009000 00006000
I (78) boot:  1 phy_init         RF data          01 01 0000f000 00001000
I (86) boot:  2 factory          factory app      00 00 00010000 00300000
I (93) boot:  3 spiffs           Unknown data     01 82 00310000 00080000
...
I (XXX) AIOT_MAIN: 🚀 AIOT ESP32-C3 Lite 系统启动
```

**不应该再出现**：`E (55) flash_parts: partition 0 invalid magic number 0xddcd`

---

## 💡 提示

- 确保USB线支持数据传输（不是只充电线）
- 如果烧录失败，尝试手动进入下载模式：
  1. 按住 Boot 键
  2. 按下 Reset 键
  3. 松开 Reset 键
  4. 松开 Boot 键
  5. 再次运行烧录命令

---

**准备好后，执行上面的一键命令即可！** 🚀

