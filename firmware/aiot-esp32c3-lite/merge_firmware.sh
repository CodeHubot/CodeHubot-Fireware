#!/bin/bash

########################################
# ESP32-C3 固件合并脚本
# 功能：将bootloader、分区表、应用程序合并成单一固件文件
# 输出：merged_firmware.bin (可直接使用烧录工具烧录到0x0地址)
########################################

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 项目目录
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_DIR"

echo -e "${GREEN}========================================"
echo "  ESP32-C3 固件合并工具"
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
BOOTLOADER="build/bootloader/bootloader.bin"
PARTITION_TABLE="build/partition_table/partition-table.bin"
APP_BIN="build/aiot-esp32c3-lite.bin"

if [ ! -f "$BOOTLOADER" ]; then
    echo -e "${RED}❌ 错误: $BOOTLOADER 不存在${NC}"
    echo -e "${YELLOW}请先编译固件: ./build.sh${NC}"
    exit 1
fi

if [ ! -f "$PARTITION_TABLE" ]; then
    echo -e "${RED}❌ 错误: $PARTITION_TABLE 不存在${NC}"
    echo -e "${YELLOW}请先编译固件: ./build.sh${NC}"
    exit 1
fi

if [ ! -f "$APP_BIN" ]; then
    echo -e "${RED}❌ 错误: $APP_BIN 不存在${NC}"
    echo -e "${YELLOW}请先编译固件: ./build.sh${NC}"
    exit 1
fi

echo -e "${GREEN}✅ 所有编译文件就绪${NC}"
echo -e "  ${BLUE}Bootloader:${NC}     $(ls -lh $BOOTLOADER | awk '{print $5}')"
echo -e "  ${BLUE}分区表:${NC}         $(ls -lh $PARTITION_TABLE | awk '{print $5}')"
echo -e "  ${BLUE}应用程序:${NC}       $(ls -lh $APP_BIN | awk '{print $5}')"

# 定义输出文件
OUTPUT_DIR="build/merged"
MERGED_BIN="$OUTPUT_DIR/aiot-esp32c3-lite_merged.bin"
MERGED_WITH_VERSION="$OUTPUT_DIR/aiot-esp32c3-lite_v1.2.1_$(date +%Y%m%d).bin"

# 创建输出目录
mkdir -p "$OUTPUT_DIR"

# Flash大小 (4MB = 4194304 bytes)
FLASH_SIZE=4194304

echo -e "\n${YELLOW}🔧 开始合并固件...${NC}"

# 分区地址定义 (必须与partitions.csv一致)
BOOTLOADER_OFFSET=0x0
PARTITION_TABLE_OFFSET=0x8000
APP_OFFSET=0x10000

echo -e "${BLUE}分区布局:${NC}"
echo -e "  Bootloader:     0x$(printf '%X' $BOOTLOADER_OFFSET)   ($(printf '%d' $BOOTLOADER_OFFSET))"
echo -e "  分区表:         0x$(printf '%X' $PARTITION_TABLE_OFFSET)   ($(printf '%d' $PARTITION_TABLE_OFFSET))"
echo -e "  应用程序:       0x$(printf '%X' $APP_OFFSET)  ($(printf '%d' $APP_OFFSET))"

# 创建4MB的空白文件（填充0xFF）
echo -e "\n${YELLOW}1️⃣  创建空白Flash镜像 (4MB)...${NC}"
python3 -c "
with open('$MERGED_BIN', 'wb') as f:
    f.write(b'\xFF' * $FLASH_SIZE)
print('✅ 已创建 4MB 空白镜像')
"

# 写入Bootloader到0x0
echo -e "\n${YELLOW}2️⃣  写入Bootloader (0x0)...${NC}"
python3 -c "
with open('$MERGED_BIN', 'r+b') as merged:
    with open('$BOOTLOADER', 'rb') as bootloader:
        data = bootloader.read()
        merged.seek($BOOTLOADER_OFFSET)
        merged.write(data)
        print(f'✅ 已写入 Bootloader ({len(data)} bytes)')
"

# 写入分区表到0x8000
echo -e "\n${YELLOW}3️⃣  写入分区表 (0x8000)...${NC}"
python3 -c "
with open('$MERGED_BIN', 'r+b') as merged:
    with open('$PARTITION_TABLE', 'rb') as partition:
        data = partition.read()
        merged.seek($PARTITION_TABLE_OFFSET)
        merged.write(data)
        print(f'✅ 已写入分区表 ({len(data)} bytes)')
"

# 写入应用程序到0x10000
echo -e "\n${YELLOW}4️⃣  写入应用程序 (0x10000)...${NC}"
python3 -c "
with open('$MERGED_BIN', 'r+b') as merged:
    with open('$APP_BIN', 'rb') as app:
        data = app.read()
        merged.seek($APP_OFFSET)
        merged.write(data)
        print(f'✅ 已写入应用程序 ({len(data)} bytes)')
"

# 复制一份带版本号的文件
cp "$MERGED_BIN" "$MERGED_WITH_VERSION"

# 生成烧录说明文件
FLASH_README="$OUTPUT_DIR/FLASH_INSTRUCTIONS.txt"
cat > "$FLASH_README" << 'EOF'
========================================
ESP32-C3 合并固件烧录说明
========================================

📦 固件文件：
  - aiot-esp32c3-lite_merged.bin (标准名称)
  - aiot-esp32c3-lite_vX.X.X_YYYYMMDD.bin (带版本和日期)

🔧 烧录方法：

方法1：使用esptool.py (推荐)
----------------------------------------
python -m esptool --chip esp32c3 --port /dev/cu.usbserial-* --baud 460800 \
    --before default_reset --after hard_reset write_flash \
    --flash_mode dio --flash_size 4MB --flash_freq 80m \
    0x0 aiot-esp32c3-lite_merged.bin

macOS串口示例：/dev/cu.usbserial-*
Linux串口示例：/dev/ttyUSB0
Windows串口示例：COM3


方法2：使用乐鑫Flash下载工具
----------------------------------------
1. 下载工具：https://www.espressif.com/zh-hans/support/download/other-tools
2. 打开 Flash Download Tools
3. 选择芯片：ESP32-C3
4. 添加文件：
   - 地址: 0x0
   - 文件: aiot-esp32c3-lite_merged.bin
5. SPI配置：
   - SPI SPEED: 80MHz
   - SPI MODE: DIO
   - FLASH SIZE: 4MB
6. 选择串口和波特率(460800)
7. 点击 START 开始烧录


方法3：使用Web串口烧录工具
----------------------------------------
访问: https://espressif.github.io/esptool-js/
1. 连接设备
2. 擦除Flash
3. 选择固件文件 (aiot-esp32c3-lite_merged.bin)
4. 烧录地址: 0x0
5. 点击 Program


⚙️ Flash配置参数：
----------------------------------------
芯片型号：     ESP32-C3
Flash大小：    4MB
Flash模式：    DIO
Flash频率：    80MHz
烧录地址：     0x0 (重要！)
波特率：       460800


⚠️  注意事项：
----------------------------------------
1. 合并固件必须烧录到地址 0x0
2. 烧录前建议先擦除Flash
3. 确保USB驱动已正确安装
4. 使用支持数据传输的USB线（不是只充电线）
5. 如果烧录失败，尝试：
   - 按住Boot键，按Reset键，松开Boot键
   - 降低波特率到115200
   - 更换USB端口或USB线


📱 首次使用步骤：
----------------------------------------
1. 设备将自动重启
2. 如果没有WiFi配置，将进入配网模式
3. 蓝色LED常亮表示配网模式
4. 连接WiFi热点: AIOT-C3-XXXXXX (密码: 12345678)
5. 浏览器访问: http://192.168.4.1
6. 输入WiFi和MQTT服务器信息
7. 保存后设备自动连接WiFi


🔍 查看串口输出：
----------------------------------------
波特率: 115200
使用命令: 
  idf.py -p PORT monitor
或者使用任何串口工具 (如: minicom, screen, PuTTY)


🆘 故障排除：
----------------------------------------
问题1: 烧录失败
  - 检查串口是否被占用
  - 尝试手动进入下载模式
  - 降低波特率

问题2: 设备无法启动
  - 检查Flash是否烧录完整
  - 尝试重新擦除并烧录
  - 检查电源供电是否稳定

问题3: WiFi配网失败
  - 确认手机已连接到设备热点
  - 检查防火墙设置
  - 重启设备重新配网


========================================
项目主页：https://github.com/your-repo
技术支持：your-email@example.com
========================================
EOF

# 生成MD5校验文件
echo -e "\n${YELLOW}5️⃣  生成校验信息...${NC}"
if command -v md5sum &> /dev/null; then
    MD5_CMD="md5sum"
elif command -v md5 &> /dev/null; then
    MD5_CMD="md5 -r"
else
    MD5_CMD=""
fi

if [ -n "$MD5_CMD" ]; then
    cd "$OUTPUT_DIR"
    $MD5_CMD aiot-esp32c3-lite_merged.bin > aiot-esp32c3-lite_merged.bin.md5
    $MD5_CMD "$(basename $MERGED_WITH_VERSION)" > "$(basename $MERGED_WITH_VERSION).md5"
    cd "$PROJECT_DIR"
    echo -e "${GREEN}✅ 已生成MD5校验文件${NC}"
fi

# 显示最终结果
echo -e "\n${GREEN}========================================${NC}"
echo -e "${GREEN}  ✅ 固件合并完成！${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo -e "${BLUE}📦 输出文件：${NC}"
ls -lh "$OUTPUT_DIR"/*.bin
echo ""
echo -e "${BLUE}📄 烧录说明：${NC}"
echo -e "  ${YELLOW}$FLASH_README${NC}"
echo ""
echo -e "${BLUE}🔧 快速烧录命令：${NC}"
echo -e "${YELLOW}python -m esptool --chip esp32c3 --port /dev/cu.usbserial-* --baud 460800 \\${NC}"
echo -e "${YELLOW}    --before default_reset --after hard_reset write_flash \\${NC}"
echo -e "${YELLOW}    --flash_mode dio --flash_size 4MB --flash_freq 80m \\${NC}"
echo -e "${YELLOW}    0x0 $MERGED_BIN${NC}"
echo ""
echo -e "${BLUE}📝 注意事项：${NC}"
echo -e "  1. 合并固件必须烧录到地址 ${RED}0x0${NC}"
echo -e "  2. 烧录前建议先擦除Flash"
echo -e "  3. 文件大小: $(ls -lh $MERGED_BIN | awk '{print $5}')"
echo -e "  4. Flash配置: DIO, 80MHz, 4MB"
echo ""
echo -e "${GREEN}✨ 可以使用任何ESP32烧录工具烧录此固件！${NC}"
echo ""
