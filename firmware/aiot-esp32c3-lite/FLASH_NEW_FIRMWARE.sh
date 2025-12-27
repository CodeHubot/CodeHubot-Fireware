#!/bin/bash

# 烧录新固件脚本（40MHz Flash频率）

set -e

cd /Users/zhangqixun/AICodeing/AIOT-Admin-Server/firmware/aiot-esp32c3-lite

echo "════════════════════════════════════════════════"
echo "  ESP32-C3 烧录新固件（40MHz Flash）"
echo "════════════════════════════════════════════════"
echo ""

# 加载ESP-IDF环境
. $HOME/esp/esp-idf/export.sh

# 检测串口
PORT=$(ls /dev/cu.* | grep -E "(usbmodem|usbserial)" | head -1)

if [ -z "$PORT" ]; then
    echo "❌ 错误: 未检测到串口"
    echo "   请确保："
    echo "   1. ESP32-C3已连接"
    echo "   2. 已退出 idf.py monitor (按 Ctrl+])"
    exit 1
fi

echo "✅ 检测到串口: $PORT"
echo ""

echo "═══ 步骤1/2: 擦除Flash ═══"
python -m esptool --chip esp32c3 --port $PORT --baud 460800 erase_flash

sleep 2
echo ""

echo "═══ 步骤2/2: 烧录固件（QIO 40MHz）═══"
python -m esptool --chip esp32c3 --port $PORT --baud 460800 \
  --before default_reset --after hard_reset write_flash \
  --flash_mode qio --flash_size 4MB --flash_freq 40m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x10000 build/aiot-esp32c3-lite.bin

echo ""
echo "════════════════════════════════════════════════"
echo "  ✅ 烧录完成！"
echo "════════════════════════════════════════════════"
echo ""
echo "查看日志："
echo "  idf.py -p $PORT monitor"
echo ""
echo "预期输出："
echo "  I (44) boot.esp32c3: SPI Speed : 40MHz  ← 新配置"
echo "  I (60) boot: Partition Table:           ← 分区表正常"
echo ""

