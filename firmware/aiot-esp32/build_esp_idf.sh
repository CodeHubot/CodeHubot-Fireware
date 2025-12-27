#!/bin/bash

# ESP-IDF Build and Flash Script for AIOT ESP32-S3 Project
# This script helps build and flash the project using ESP-IDF

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Function to print colored output
print_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Check if ESP-IDF is installed
check_esp_idf() {
    if [ -z "$IDF_PATH" ]; then
        print_error "ESP-IDF not found. Please install and source ESP-IDF first."
        print_info "Visit: https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/"
        exit 1
    fi
    print_info "ESP-IDF found at: $IDF_PATH"
}

# Set target to ESP32-S3
set_target() {
    print_info "Setting target to ESP32-S3..."
    idf.py set-target esp32s3
}

# Configure project
configure() {
    print_info "Configuring project..."
    idf.py menuconfig
}

# Build project
build() {
    print_info "Building project..."
    idf.py build
}

# Flash to device
flash() {
    if [ -z "$1" ]; then
        print_info "Flashing to device (auto-detect port)..."
        idf.py flash
    else
        print_info "Flashing to device on port $1..."
        idf.py -p $1 flash
    fi
}

# Monitor serial output
monitor() {
    if [ -z "$1" ]; then
        print_info "Starting monitor (auto-detect port)..."
        idf.py monitor
    else
        print_info "Starting monitor on port $1..."
        idf.py -p $1 monitor
    fi
}

# Flash and monitor
flash_monitor() {
    if [ -z "$1" ]; then
        print_info "Flashing and monitoring (auto-detect port)..."
        idf.py flash monitor
    else
        print_info "Flashing and monitoring on port $1..."
        idf.py -p $1 flash monitor
    fi
}

# Clean build
clean() {
    print_info "Cleaning build..."
    idf.py fullclean
}

# Show help
show_help() {
    echo "ESP-IDF Build Script for AIOT ESP32-S3 Project"
    echo ""
    echo "Usage: $0 [command] [port]"
    echo ""
    echo "Commands:"
    echo "  check       - Check ESP-IDF installation"
    echo "  target      - Set target to ESP32-S3"
    echo "  config      - Configure project (menuconfig)"
    echo "  build       - Build project"
    echo "  flash       - Flash to device"
    echo "  monitor     - Monitor serial output"
    echo "  flash-mon   - Flash and monitor"
    echo "  clean       - Clean build"
    echo "  help        - Show this help"
    echo ""
    echo "Examples:"
    echo "  $0 build"
    echo "  $0 flash /dev/ttyUSB0"
    echo "  $0 flash-mon"
    echo ""
}

# Main script logic
case "$1" in
    "check")
        check_esp_idf
        ;;
    "target")
        check_esp_idf
        set_target
        ;;
    "config")
        check_esp_idf
        configure
        ;;
    "build")
        check_esp_idf
        build
        ;;
    "flash")
        check_esp_idf
        flash $2
        ;;
    "monitor")
        check_esp_idf
        monitor $2
        ;;
    "flash-mon")
        check_esp_idf
        flash_monitor $2
        ;;
    "clean")
        check_esp_idf
        clean
        ;;
    "help"|"--help"|"-h")
        show_help
        ;;
    "")
        print_info "Building project with ESP-IDF..."
        check_esp_idf
        build
        ;;
    *)
        print_error "Unknown command: $1"
        show_help
        exit 1
        ;;
esac

print_info "Script completed successfully!"