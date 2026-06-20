#!/bin/bash
#
# 统一发布合并固件入口
# 用法:
#   ./publish_merged_firmware.sh s3 [--build]
#   ./publish_merged_firmware.sh c3 [--build]
#

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FIRMWARE_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REPO_ROOT="$(cd "$FIRMWARE_DIR/.." && pwd)"

TARGET="${1:-}"
DO_BUILD=false

if [ $# -eq 0 ]; then
    echo "用法: $0 {s3|c3} [--build]"
    echo ""
    echo "示例:"
    echo "  $0 s3           # 合并 ESP32-S3 并发布到 docs/firmware/"
    echo "  $0 s3 --build   # 编译 + 合并 + 发布"
    echo "  $0 c3 --build   # 编译 + 合并 + 发布 ESP32-C3"
    exit 1
fi

shift || true
for arg in "$@"; do
    [ "$arg" = "--build" ] && DO_BUILD=true
done

ensure_idf() {
    if [ -z "$IDF_PATH" ] && [ -f "$HOME/esp/esp-idf/export.sh" ]; then
        # shellcheck disable=SC1091
        . "$HOME/esp/esp-idf/export.sh"
    fi
    if [ -z "$IDF_PATH" ]; then
        echo -e "${RED}错误: 请先激活 ESP-IDF 环境${NC}"
        exit 1
    fi
}

ensure_idf

case "$TARGET" in
    s3|esp32s3|rain)
        PROJECT="$FIRMWARE_DIR/aiot-esp32"
        ARGS=(--publish)
        $DO_BUILD && ARGS=(--build --publish)
        echo -e "${GREEN}>>> ESP32-S3 合并固件发布${NC}"
        cd "$PROJECT"
        chmod +x merge_firmware.sh
        ./merge_firmware.sh "${ARGS[@]}"
        ;;
    c3|esp32c3|lite)
        PROJECT="$FIRMWARE_DIR/aiot-esp32c3-lite"
        ARGS=(--publish)
        $DO_BUILD && ARGS=(--build --publish)
        echo -e "${GREEN}>>> ESP32-C3 合并固件发布${NC}"
        cd "$PROJECT"
        chmod +x merge_firmware.sh
        ./merge_firmware.sh "${ARGS[@]}"
        ;;
    *)
        echo -e "${RED}未知目标: $TARGET（支持 s3 / c3）${NC}"
        exit 1
        ;;
esac

echo ""
echo -e "${GREEN}发布目录:${NC} $REPO_ROOT/docs/firmware/"
ls -lh "$REPO_ROOT/docs/firmware/"*.bin 2>/dev/null || true
echo ""
echo -e "${YELLOW}下一步: 检查版本号后 git add docs/firmware/ 并提交推送${NC}"
