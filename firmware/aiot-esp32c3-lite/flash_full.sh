#!/bin/bash

########################################
# ESP32-C3 完整烧录脚本
# 包含：擦除Flash、烧录Bootloader、分区表和应用程序
########################################

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 项目目录
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_DIR"

echo -e "${GREEN}========================================"
echo "  ESP32-C3 完整烧录脚本"
echo "========================================${NC}"

# 检查ESP-IDF环境
if [ -z "$IDF_PATH" ]; then
    echo -e "${YELLOW}⚠️  未检测到ESP-IDF环境，正在加载...${NC}"
    if [ -f "$HOME/esp/esp-idf/export.sh" ]; then
        . "$HOME/esp/esp-idf/export.sh"
        echo -e "${GREEN}✅ ESP-IDF环境已加载${NC}"
    else
        echo -e "${RED}❌ 错误: 找不到ESP-IDF，请先安装ESP-IDF${NC}"
        exit 1
    fi
else
    echo -e "${GREEN}✅ ESP-IDF路径: $IDF_PATH${NC}"
fi

# 检查编译文件
echo -e "\n${YELLOW}📁 检查编译文件...${NC}"
if [ ! -f "build/bootloader/bootloader.bin" ]; then
    echo -e "${RED}❌ 错误: bootloader.bin 不存在，请先编译${NC}"
    exit 1
fi

if [ ! -f "build/partition_table/partition-table.bin" ]; then
    echo -e "${RED}❌ 错误: partition-table.bin 不存在，请先编译${NC}"
    exit 1
fi

if [ ! -f "build/aiot-esp32c3-lite.bin" ]; then
    echo -e "${RED}❌ 错误: aiot-esp32c3-lite.bin 不存在，请先编译${NC}"
    exit 1
fi

echo -e "${GREEN}✅ 所有编译文件就绪${NC}"
ls -lh build/bootloader/bootloader.bin
ls -lh build/partition_table/partition-table.bin
ls -lh build/aiot-esp32c3-lite.bin

# 获取串口端口
echo -e "\n${YELLOW}🔌 检测可用串口...${NC}"
if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS
    PORTS=$(ls /dev/cu.* 2>/dev/null | grep -E "(usbserial|wchusbserial|SLAB_USBtoUART)" || true)
    if [ -z "$PORTS" ]; then
        PORTS=$(ls /dev/cu.* 2>/dev/null || true)
    fi
else
    # Linux
    PORTS=$(ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || true)
fi

if [ -z "$PORTS" ]; then
    echo -e "${RED}❌ 错误: 未检测到串口设备${NC}"
    echo "请检查："
    echo "  1. ESP32-C3是否已连接到电脑"
    echo "  2. USB驱动是否已安装"
    echo "  3. USB线是否支持数据传输（不是只充电线）"
    exit 1
fi

echo -e "${GREEN}检测到以下串口：${NC}"
echo "$PORTS" | nl

# 选择串口
PORT_COUNT=$(echo "$PORTS" | wc -l)
if [ $PORT_COUNT -eq 1 ]; then
    PORT=$(echo "$PORTS" | head -1)
    echo -e "${GREEN}✅ 自动选择串口: $PORT${NC}"
else
    echo -e "${YELLOW}请输入串口编号 (1-$PORT_COUNT):${NC}"
    read -r PORT_NUM
    PORT=$(echo "$PORTS" | sed -n "${PORT_NUM}p")
    if [ -z "$PORT" ]; then
        echo -e "${RED}❌ 错误: 无效的串口编号${NC}"
        exit 1
    fi
fi

echo -e "\n${YELLOW}📋 烧录信息：${NC}"
echo "  芯片: ESP32-C3"
echo "  串口: $PORT"
echo "  波特率: 460800"
echo "  Flash大小: 4MB"
echo "  Flash模式: DIO"
echo "  Flash频率: 80MHz"
echo ""
echo "  Bootloader:     0x0000   (20 KB)"
echo "  分区表:         0x8000   (3 KB)"
echo "  应用程序:       0x10000  (858 KB)"

# 确认烧录
echo -e "\n${RED}⚠️  警告: 此操作将擦除ESP32-C3的所有数据！${NC}"
echo -e "${YELLOW}是否继续？(y/N):${NC}"
read -r CONFIRM
if [[ ! "$CONFIRM" =~ ^[Yy]$ ]]; then
    echo -e "${YELLOW}❌ 用户取消操作${NC}"
    exit 0
fi

# 步骤1: 擦除Flash
echo -e "\n${YELLOW}🧹 步骤1: 擦除Flash...${NC}"
python -m esptool --chip esp32c3 --port "$PORT" --baud 460800 erase_flash
if [ $? -eq 0 ]; then
    echo -e "${GREEN}✅ Flash擦除成功${NC}"
else
    echo -e "${RED}❌ Flash擦除失败${NC}"
    exit 1
fi

# 等待一下
sleep 2

# 步骤2: 烧录固件
echo -e "\n${YELLOW}📥 步骤2: 烧录完整固件...${NC}"
python -m esptool --chip esp32c3 --port "$PORT" --baud 460800 \
    --before default_reset --after hard_reset write_flash \
    --flash_mode dio --flash_size 4MB --flash_freq 80m \
    0x0 build/bootloader/bootloader.bin \
    0x8000 build/partition_table/partition-table.bin \
    0x10000 build/aiot-esp32c3-lite.bin

if [ $? -eq 0 ]; then
    echo -e "\n${GREEN}========================================${NC}"
    echo -e "${GREEN}  ✅ 烧录成功！${NC}"
    echo -e "${GREEN}========================================${NC}"
    echo ""
    echo -e "${YELLOW}📱 首次使用步骤：${NC}"
    echo "  1. 设备将自动重启"
    echo "  2. 如果没有WiFi配置，将进入配网模式"
    echo "  3. 蓝色LED常亮表示配网模式"
    echo "  4. 连接WiFi热点: AIOT-C3-XXXXXX"
    echo "  5. 浏览器访问: http://192.168.4.1"
    echo "  6. 输入WiFi和MQTT服务器信息"
    echo ""
    echo -e "${YELLOW}🔍 查看串口输出：${NC}"
    echo "  idf.py -p $PORT monitor"
    echo ""
    echo -e "${YELLOW}或使用build.sh脚本：${NC}"
    echo "  ./build.sh monitor"
else
    echo -e "\n${RED}========================================${NC}"
    echo -e "${RED}  ❌ 烧录失败！${NC}"
    echo -e "${RED}========================================${NC}"
    echo ""
    echo "可能的原因："
    echo "  1. 串口被占用（关闭串口监视器）"
    echo "  2. ESP32-C3未进入下载模式"
    echo "  3. USB连接不稳定"
    echo ""
    echo "解决方法："
    echo "  1. 关闭所有串口监视器程序"
    echo "  2. 重新插拔USB线"
    echo "  3. 按住Boot键，然后按Reset键，再松开Boot键"
    exit 1
fi

