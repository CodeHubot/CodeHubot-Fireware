#!/bin/bash

# AIOT ESP32 固件重新编译脚本
# 清理所有缓存并重新编译

set -e  # 遇到错误立即退出

echo "════════════════════════════════════════════════════════════════"
echo "🔧 AIOT ESP32 固件重新编译"
echo "════════════════════════════════════════════════════════════════"
echo ""

# 1. 检查当前目录
if [ ! -f "CMakeLists.txt" ]; then
    echo "❌ 错误：请在 firmware/aiot-esp32 目录下执行此脚本"
    exit 1
fi

# 2. 拉取最新代码
echo "📥 1. 拉取最新代码..."
git pull origin main
echo ""

# 3. 彻底清理所有缓存
echo "🧹 2. 彻底清理所有缓存..."
cleaned=false
if [ -d "build" ]; then
    rm -rf build
    echo "   ✅ build 目录已删除"
    cleaned=true
fi
if [ -f "sdkconfig.old" ]; then
    rm -f sdkconfig.old
    echo "   ✅ sdkconfig.old 已删除"
    cleaned=true
fi
if [ -d ".cache" ]; then
    rm -rf .cache
    echo "   ✅ .cache 目录已删除"
    cleaned=true
fi
if [ "$cleaned" = false ]; then
    echo "   ℹ️  没有需要清理的缓存文件"
fi
echo ""

# 4. 检查 ESP-IDF 环境
echo "🔍 3. 检查 ESP-IDF 环境..."
if ! command -v idf.py &> /dev/null; then
    echo "   ⚠️  ESP-IDF 环境未激活"
    echo ""
    echo "   请先激活 ESP-IDF 环境："
    echo "   . \$HOME/esp/esp-idf/export.sh"
    echo ""
    exit 1
fi
echo "   ✅ ESP-IDF 环境已激活"
echo ""

# 5. 彻底清理编译产物
echo "🧹 4. 彻底清理编译产物..."
idf.py fullclean 2>/dev/null || echo "   ℹ️  跳过 fullclean（build 目录不存在）"
echo ""

# 6. 重新配置
echo "⚙️  5. 重新配置项目..."
idf.py reconfigure
echo ""

# 7. 编译
echo "🔨 6. 编译固件..."
idf.py build
echo ""

# 8. 提示烧录
echo "════════════════════════════════════════════════════════════════"
echo "✅ 编译成功！"
echo "════════════════════════════════════════════════════════════════"
echo ""
echo "烧录到设备："
echo "  idf.py flash"
echo ""
echo "烧录并监视："
echo "  idf.py flash monitor"
echo ""
echo "只查看日志："
echo "  idf.py monitor"
echo ""

