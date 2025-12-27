#!/bin/bash
# ESP32 eFuse MAC地址读取脚本
# 用于批量检查多个ESP32设备的MAC地址

echo "==================================================="
echo "ESP32 eFuse MAC地址读取工具"
echo "==================================================="
echo ""

# 检查esptool.py是否安装
if ! command -v esptool.py &> /dev/null; then
    echo "❌ 未找到 esptool.py"
    echo "请先安装: pip install esptool"
    exit 1
fi

# 获取串口参数
if [ $# -eq 0 ]; then
    echo "使用方法:"
    echo "  $0 <串口1> [串口2] [串口3] ..."
    echo ""
    echo "示例:"
    echo "  $0 /dev/ttyUSB0"
    echo "  $0 /dev/ttyUSB0 /dev/ttyUSB1 /dev/ttyUSB2"
    echo "  $0 COM3 COM4 COM5"
    exit 1
fi

# 存储MAC地址的临时文件
TEMP_FILE=$(mktemp)
trap "rm -f $TEMP_FILE" EXIT

# 读取所有设备的MAC地址
for PORT in "$@"; do
    echo "---------------------------------------------------"
    echo "正在读取设备: $PORT"
    echo "---------------------------------------------------"
    
    # 读取MAC地址
    MAC=$(esptool.py --port "$PORT" read_mac 2>&1 | grep "MAC:" | awk '{print $2}')
    
    if [ -n "$MAC" ]; then
        echo "✅ MAC地址: $MAC"
        echo "$PORT:$MAC" >> "$TEMP_FILE"
    else
        echo "❌ 读取失败"
    fi
    echo ""
done

# 检查重复
echo "==================================================="
echo "MAC地址汇总"
echo "==================================================="

if [ -f "$TEMP_FILE" ]; then
    cat "$TEMP_FILE"
    echo ""
    
    # 检查重复的MAC地址
    DUPLICATES=$(awk -F: '{print $2":"$3":"$4":"$5":"$6":"$7}' "$TEMP_FILE" | sort | uniq -d)
    
    if [ -n "$DUPLICATES" ]; then
        echo "==================================================="
        echo "⚠️  发现重复的MAC地址:"
        echo "==================================================="
        echo "$DUPLICATES"
        echo ""
        echo "❌ 警告: 存在MAC地址重复的设备！"
        echo ""
        echo "可能原因:"
        echo "1. 使用了未烧录唯一MAC的ESP32芯片"
        echo "2. 购买了劣质或山寨模组"
        echo "3. 供应商MAC地址管理混乱"
        echo ""
        echo "建议:"
        echo "1. 更换正规渠道的ESP32-S3模组（乐鑫官方、安信可等）"
        echo "2. 联系供应商确认MAC地址烧录情况"
        echo "3. 要求供应商提供MAC地址证书"
        exit 1
    else
        echo "✅ 所有设备MAC地址唯一，无重复"
    fi
else
    echo "❌ 未能读取到任何MAC地址"
fi


