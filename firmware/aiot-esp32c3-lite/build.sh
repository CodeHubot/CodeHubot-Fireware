#!/bin/bash
# ESP32-C3 Lite 固件编译脚本

set -e  # 遇到错误立即退出

echo "========================================"
echo "  AIOT ESP32-C3 Lite 固件编译脚本"
echo "========================================"

# 检查ESP-IDF环境
if [ -z "$IDF_PATH" ]; then
    echo "❌ 错误: ESP-IDF环境未配置"
    echo "请先运行: . \$HOME/esp/esp-idf/export.sh"
    exit 1
fi

echo "✅ ESP-IDF路径: $IDF_PATH"

# 项目目录
PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$PROJECT_DIR"

echo "📁 项目目录: $PROJECT_DIR"

# 解析命令
case "${1:-build}" in
    clean)
        echo "🧹 清理构建文件..."
        idf.py fullclean
        rm -f sdkconfig
        echo "✅ 清理完成"
        ;;
    
    config)
        echo "⚙️  打开配置菜单..."
        idf.py menuconfig
        ;;
    
    build)
        echo "🔨 开始编译固件..."
        
        # 设置目标芯片
        echo "📌 设置目标芯片: ESP32-C3"
        idf.py set-target esp32c3
        
        # 编译
        echo "⏳ 正在编译..."
        idf.py build
        
        # 显示固件信息
        if [ -f "build/aiot-esp32c3-lite.bin" ]; then
            SIZE=$(du -h "build/aiot-esp32c3-lite.bin" | cut -f1)
            echo ""
            echo "✅ 编译成功！"
            echo "📦 固件文件: build/aiot-esp32c3-lite.bin"
            echo "📊 固件大小: $SIZE"
            echo ""
            echo "提示："
            echo "  烧录: ./build.sh flash"
            echo "  监控: ./build.sh monitor"
        else
            echo "❌ 编译失败，请查看错误信息"
            exit 1
        fi
        ;;
    
    flash)
        PORT="${2:-/dev/cu.usbserial-*}"
        echo "📤 烧录固件到设备..."
        echo "   端口: $PORT"
        idf.py -p "$PORT" flash
        echo "✅ 烧录完成"
        ;;
    
    monitor)
        PORT="${2:-/dev/cu.usbserial-*}"
        echo "📺 打开串口监控..."
        echo "   端口: $PORT"
        echo "   退出: Ctrl+]"
        idf.py -p "$PORT" monitor
        ;;
    
    flash-monitor|fm)
        PORT="${2:-/dev/cu.usbserial-*}"
        echo "📤 烧录固件并打开监控..."
        idf.py -p "$PORT" flash monitor
        ;;
    
    erase)
        PORT="${2:-/dev/cu.usbserial-*}"
        echo "🗑️  擦除Flash..."
        idf.py -p "$PORT" erase-flash
        echo "✅ Flash已擦除"
        ;;
    
    size)
        echo "📊 分析固件大小..."
        idf.py size
        idf.py size-components
        ;;
    
    all)
        echo "🔄 完整编译流程..."
        idf.py fullclean
        rm -f sdkconfig
        idf.py set-target esp32c3
        idf.py build
        echo "✅ 完整编译完成"
        ;;
    
    merge)
        echo "🔀 合并固件为单个文件..."
        
        # 首先确保固件已编译
        if [ ! -f "build/aiot-esp32c3-lite.bin" ]; then
            echo "📦 固件未编译，开始编译..."
            idf.py set-target esp32c3
            idf.py build
        fi
        
        # 调用合并脚本
        if [ -f "./merge_firmware.sh" ]; then
            ./merge_firmware.sh
        else
            echo "❌ 未找到 merge_firmware.sh 脚本"
            exit 1
        fi
        ;;
    
    *)
        echo "用法: $0 {命令} [选项]"
        echo ""
        echo "命令:"
        echo "  build           - 编译固件 (默认)"
        echo "  clean           - 清理构建文件"
        echo "  config          - 打开配置菜单"
        echo "  flash [端口]    - 烧录固件"
        echo "  monitor [端口]  - 打开串口监控"
        echo "  flash-monitor   - 烧录并监控 (简写: fm)"
        echo "  erase [端口]    - 擦除Flash"
        echo "  size            - 分析固件大小"
        echo "  all             - 完整清理并编译"
        echo "  merge           - 合并为单个固件文件"
        echo ""
        echo "示例:"
        echo "  $0 build                    # 编译"
        echo "  $0 flash /dev/ttyUSB0       # 烧录到指定端口"
        echo "  $0 fm                       # 烧录并监控"
        echo "  $0 merge                    # 生成合并固件"
        ;;
esac

echo ""
echo "========================================"

