# AIOT ESP32 Multi-Board Firmware Makefile
# 用于演示不同开发板配置的编译

# 编译器设置
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I. -Imain -Imain/hal -Imain/bsp

# 目标文件目录
BUILD_DIR = build
ESP32_S3_OBJ_DIR = $(BUILD_DIR)/obj-s3
ESP32_C3_OBJ_DIR = $(BUILD_DIR)/obj-c3

# 源文件
COMMON_SOURCES = \
	main/bsp/bsp_interface.c \
	main/main.c

# 开发板特定源文件
ESP32_S3_SOURCES = boards/esp32-s3-devkit/bsp_esp32_s3_devkit.c
ESP32_C3_SOURCES = boards/esp32-c3-mini/bsp_esp32_c3_mini.c

# 目标文件 - 为不同开发板使用独立目录
ESP32_S3_COMMON_OBJECTS = $(COMMON_SOURCES:%.c=$(ESP32_S3_OBJ_DIR)/%.o)
ESP32_S3_BOARD_OBJECTS = $(ESP32_S3_SOURCES:%.c=$(ESP32_S3_OBJ_DIR)/%.o)
ESP32_C3_COMMON_OBJECTS = $(COMMON_SOURCES:%.c=$(ESP32_C3_OBJ_DIR)/%.o)
ESP32_C3_BOARD_OBJECTS = $(ESP32_C3_SOURCES:%.c=$(ESP32_C3_OBJ_DIR)/%.o)

# 可执行文件
ESP32_S3_TARGET = $(BUILD_DIR)/aiot-esp32-s3-devkit
ESP32_C3_TARGET = $(BUILD_DIR)/aiot-esp32-c3-mini

# 默认目标
.PHONY: all clean esp32-s3 esp32-c3 demo

all: esp32-s3 esp32-c3

# ESP32-S3开发板
esp32-s3: CFLAGS += -DCONFIG_BOARD_ESP32_S3_DEVKIT=1
esp32-s3: $(ESP32_S3_TARGET)

$(ESP32_S3_TARGET): $(ESP32_S3_COMMON_OBJECTS) $(ESP32_S3_BOARD_OBJECTS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^
	@echo "ESP32-S3 DevKit firmware built successfully: $@"

# ESP32-C3开发板
esp32-c3: CFLAGS += -DCONFIG_BOARD_ESP32_C3_MINI=1
esp32-c3: $(ESP32_C3_TARGET)

$(ESP32_C3_TARGET): $(ESP32_C3_COMMON_OBJECTS) $(ESP32_C3_BOARD_OBJECTS)
	@mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^
	@echo "ESP32-C3 Mini firmware built successfully: $@"

# 编译ESP32-S3目标文件
$(ESP32_S3_OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DCONFIG_BOARD_ESP32_S3_DEVKIT=1 -c $< -o $@

# 编译ESP32-C3目标文件
$(ESP32_C3_OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DCONFIG_BOARD_ESP32_C3_MINI=1 -c $< -o $@

# 运行演示
demo: esp32-s3 esp32-c3
	@echo "=== Running ESP32-S3 DevKit Demo ==="
	./$(ESP32_S3_TARGET)
	@echo ""
	@echo "=== Running ESP32-C3 Mini Demo ==="
	./$(ESP32_C3_TARGET)

# 清理
clean:
	rm -rf $(BUILD_DIR)
	@echo "Build directory cleaned"

# 显示帮助
help:
	@echo "AIOT ESP32 Multi-Board Firmware Build System"
	@echo ""
	@echo "Available targets:"
	@echo "  all      - Build all board configurations"
	@echo "  esp32-s3 - Build ESP32-S3 DevKit firmware"
	@echo "  esp32-c3 - Build ESP32-C3 Mini firmware"
	@echo "  demo     - Build and run both configurations"
	@echo "  clean    - Clean build directory"
	@echo "  help     - Show this help message"
	@echo ""
	@echo "Board configurations:"
	@echo "  ESP32-S3 DevKit: Full-featured board with display and audio"
	@echo "  ESP32-C3 Mini:   Compact board with basic IoT features"

# 显示项目信息
info:
	@echo "=== AIOT ESP32 Multi-Board Firmware ==="
	@echo "Project Structure:"
	@echo "  main/           - Common application code"
	@echo "  main/hal/       - Hardware Abstraction Layer"
	@echo "  main/bsp/       - Board Support Package interface"
	@echo "  boards/         - Board-specific configurations"
	@echo "  build/          - Build output directory"
	@echo ""
	@echo "Supported Boards:"
	@echo "  - ESP32-S3 DevKit (boards/esp32-s3-devkit/)"
	@echo "  - ESP32-C3 Mini   (boards/esp32-c3-mini/)"
	@echo ""
	@echo "To add a new board:"
	@echo "  1. Create boards/your-board/ directory"
	@echo "  2. Add board_config.h with hardware definitions"
	@echo "  3. Add bsp_your_board.c/.h with BSP implementation"
	@echo "  4. Update Makefile with new board target"