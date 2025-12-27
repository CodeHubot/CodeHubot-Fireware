#!/bin/bash

echo "=== ESP32-S3 硬件连接检查 ==="
echo

# 检查USB设备连接
echo "1. 检查USB设备连接..."
if ls /dev/cu.usbserial-* /dev/cu.usbmodem-* 2>/dev/null; then
    echo "✅ 检测到ESP32设备"
else
    echo "❌ 未检测到ESP32设备"
    echo "请检查："
    echo "  - USB线是否正确连接"
    echo "  - 是否使用数据线（非充电线）"
    echo "  - 开发板是否正常供电"
    echo "  - 驱动是否正确安装"
fi

echo
echo "2. 检查系统USB设备..."
system_profiler SPUSBDataType | grep -A 5 -B 5 "ESP32\|Silicon Labs\|CP210\|CH340"

echo
echo "3. 建议的连接步骤："
echo "  1) 断开ESP32-S3开发板"
echo "  2) 重新连接USB线"
echo "  3) 按住BOOT按钮，然后按RST按钮"
echo "  4) 释放RST按钮，再释放BOOT按钮"
echo "  5) 重新运行检查"
