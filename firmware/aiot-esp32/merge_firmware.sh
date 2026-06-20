#!/bin/bash
#
# ESP32-S3 合并固件生成与发布脚本
# 用法:
#   ./merge_firmware.sh              # 仅合并到 build/merged/
#   ./merge_firmware.sh --publish    # 合并并复制到 docs/firmware/
#   ./merge_firmware.sh --build      # 先编译再合并
#   ./merge_firmware.sh --build --publish
#

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$PROJECT_DIR/../.." && pwd)"
DO_PUBLISH=false
DO_BUILD=false

for arg in "$@"; do
    case "$arg" in
        --publish) DO_PUBLISH=true ;;
        --build)   DO_BUILD=true ;;
        -h|--help)
            echo "用法: $0 [--build] [--publish]"
            exit 0
            ;;
    esac
done

log_info()  { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn()  { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

ensure_idf() {
    if [ -z "$IDF_PATH" ]; then
        if [ -f "$HOME/esp/esp-idf/export.sh" ]; then
            # shellcheck disable=SC1091
            . "$HOME/esp/esp-idf/export.sh"
        else
            log_error "未找到 ESP-IDF，请先执行: . ~/esp/esp-idf/export.sh"
            exit 1
        fi
    fi
    log_info "ESP-IDF: $IDF_PATH"
}

read_product_code() {
    local cfg=""
    if [ -f "$PROJECT_DIR/sdkconfig" ]; then
        cfg="$PROJECT_DIR/sdkconfig"
    elif [ -f "$PROJECT_DIR/sdkconfig.defaults" ]; then
        cfg="$PROJECT_DIR/sdkconfig.defaults"
    fi

    if [ -n "$cfg" ]; then
        if grep -q '^CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_RAIN=y' "$cfg"; then
            echo "ESP32-S3-Rain-01"
            return
        fi
        if grep -q '^CONFIG_AIOT_BOARD_ESP32_S3_DEVKIT_LITE=y' "$cfg"; then
            echo "ESP32-S3-Lite-01"
            return
        fi
    fi
    echo "ESP32-S3-Dev-01"
}

read_firmware_version() {
    grep '#define DEVICE_FIRMWARE_VERSION' "$PROJECT_DIR/DEVICE_CONFIG.h" \
        | sed -E 's/.*"([^"]+)".*/\1/'
}

cd "$PROJECT_DIR"
ensure_idf

if $DO_BUILD; then
    log_info "编译固件..."
    idf.py set-target esp32s3
    idf.py build
fi

BOOTLOADER="build/bootloader/bootloader.bin"
PARTITION="build/partition_table/partition-table.bin"
APP_BIN="build/aiot-esp32s3-firmware.bin"
OTA_DATA="build/ota_data_initial.bin"

for f in "$BOOTLOADER" "$PARTITION" "$APP_BIN" "$OTA_DATA"; do
    if [ ! -f "$f" ]; then
        log_error "缺少文件: $f"
        log_warn "请先编译: idf.py set-target esp32s3 && idf.py build"
        exit 1
    fi
done

PRODUCT_CODE="$(read_product_code)"
FW_VERSION="$(read_firmware_version)"
OUTPUT_DIR="build/merged"
RELEASE_NAME="${PRODUCT_CODE}-v${FW_VERSION}"
MERGED_BIN="${OUTPUT_DIR}/${RELEASE_NAME}.bin"

mkdir -p "$OUTPUT_DIR"

log_info "产品: ${PRODUCT_CODE}, 版本: v${FW_VERSION}"
log_info "开始合并固件 (16MB Flash)..."

python -m esptool --chip esp32s3 merge_bin \
    -o "$MERGED_BIN" \
    --flash_mode dio \
    --flash_freq 80m \
    --flash_size 16MB \
    0x0      "$BOOTLOADER" \
    0x8000   "$PARTITION" \
    0x10000  "$APP_BIN" \
    0x610000 "$OTA_DATA"

ln -sf "$(basename "$MERGED_BIN")" "${OUTPUT_DIR}/latest-merged.bin"

if command -v md5 >/dev/null 2>&1; then
    md5 "$MERGED_BIN" > "${MERGED_BIN}.md5"
elif command -v md5sum >/dev/null 2>&1; then
    md5sum "$MERGED_BIN" > "${MERGED_BIN}.md5"
fi

log_info "合并完成: $MERGED_BIN ($(ls -lh "$MERGED_BIN" | awk '{print $5}'))"

if $DO_PUBLISH; then
    PUBLISH_DIR="${REPO_ROOT}/docs/firmware"
    mkdir -p "$PUBLISH_DIR"
    cp "$MERGED_BIN" "${PUBLISH_DIR}/$(basename "$MERGED_BIN")"
    if [ -f "${MERGED_BIN}.md5" ]; then
        cp "${MERGED_BIN}.md5" "${PUBLISH_DIR}/$(basename "$MERGED_BIN").md5"
    fi
    log_info "已发布到: ${PUBLISH_DIR}/$(basename "$MERGED_BIN")"
    echo ""
    echo -e "${BLUE}烧录命令:${NC}"
    echo "python -m esptool --chip esp32s3 --port /dev/cu.usbserial-* --baud 460800 \\"
    echo "  --before default_reset --after hard_reset write_flash \\"
    echo "  --flash_mode dio --flash_size 16MB --flash_freq 80m \\"
    echo "  0x0 ${PUBLISH_DIR}/$(basename "$MERGED_BIN")"
fi

echo ""
log_info "完成"
