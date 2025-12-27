#!/usr/bin/env python3
"""
快速获取ESP32设备MAC地址的工具

使用方法：
    python tools/get_mac.py [端口]

示例：
    python tools/get_mac.py                  # 自动检测端口
    python tools/get_mac.py /dev/ttyUSB0     # 指定端口
    python tools/get_mac.py COM3             # Windows

功能：
    - 自动检测串口
    - 读取并显示MAC地址
    - 生成设备标签（用于打印）
    - 保存到文件

输出格式：
    ====================================
    📱 设备MAC地址
    ====================================
    MAC: 80:B5:4E:D6:F8:60
    ====================================
    
    💡 请将此MAC地址打印并贴在设备上
"""

import sys
import serial
import serial.tools.list_ports
import re
import time

def find_esp32_port():
    """自动检测ESP32串口"""
    ports = list(serial.tools.list_ports.comports())
    
    # ESP32常见的USB转串口芯片
    esp32_chips = ['CP210', 'CH340', 'FT232', 'SLAB']
    
    for port in ports:
        for chip in esp32_chips:
            if chip in port.description or chip in str(port.hwid):
                return port.device
    
    # 如果没找到，返回第一个可用端口
    if ports:
        return ports[0].device
    
    return None

def read_mac_from_serial(port, baudrate=115200, timeout=10):
    """从串口读取MAC地址"""
    try:
        ser = serial.Serial(port, baudrate, timeout=1)
        print(f"🔌 连接到: {port}")
        print(f"⏳ 正在读取MAC地址（最多等待{timeout}秒）...")
        print(f"💡 提示：如果设备已启动，请按复位按钮重启设备\n")
        
        start_time = time.time()
        lines_buffer = []
        
        while time.time() - start_time < timeout:
            if ser.in_waiting > 0:
                try:
                    line = ser.readline().decode('utf-8', errors='ignore').strip()
                    lines_buffer.append(line)
                    
                    # 保持最近100行
                    if len(lines_buffer) > 100:
                        lines_buffer.pop(0)
                    
                    # 查找MAC地址
                    # 支持多种格式：
                    # - MAC: 80:B5:4E:D6:F8:60
                    # - MAC地址: 80:B5:4E:D6:F8:60
                    # - 80:B5:4E:D6:F8:60
                    mac_pattern = r'([0-9A-Fa-f]{2}[:-]){5}[0-9A-Fa-f]{2}'
                    match = re.search(mac_pattern, line)
                    
                    if match:
                        mac = match.group(0).upper()
                        # 统一使用冒号分隔
                        mac = mac.replace('-', ':')
                        ser.close()
                        return mac, lines_buffer
                    
                except UnicodeDecodeError:
                    continue
        
        ser.close()
        return None, lines_buffer
        
    except serial.SerialException as e:
        print(f"❌ 串口错误: {e}")
        return None, []

def print_mac_label(mac, product_id=1, firmware_version="1.0.0"):
    """打印设备标签"""
    print("\n" + "="*50)
    print(" 📱 AIOT 设备信息".center(50))
    print("="*50)
    print(f"\n  MAC地址:     {mac}")
    print(f"  产品ID:      {product_id}")
    print(f"  固件版本:    {firmware_version}")
    print("\n" + "="*50)
    print(" 💡 请将此信息打印并贴在设备上".center(48))
    print("="*50 + "\n")

def save_to_file(mac, filename="device_mac.txt"):
    """保存MAC地址到文件"""
    try:
        with open(filename, 'a', encoding='utf-8') as f:
            timestamp = time.strftime('%Y-%m-%d %H:%M:%S')
            f.write(f"{timestamp} - MAC: {mac}\n")
        print(f"✅ MAC地址已保存到: {filename}")
    except Exception as e:
        print(f"⚠️  保存文件失败: {e}")

def generate_qr_label(mac):
    """生成二维码标签（可选）"""
    try:
        import qrcode
        qr = qrcode.QRCode(version=1, box_size=10, border=2)
        qr.add_data(mac)
        qr.make(fit=True)
        
        # 在终端显示ASCII二维码
        qr.print_ascii(invert=True)
        
        # 保存为图片
        img = qr.make_image(fill_color="black", back_color="white")
        filename = f"mac_{mac.replace(':', '')}.png"
        img.save(filename)
        print(f"✅ 二维码已保存为: {filename}")
        
    except ImportError:
        print("💡 提示: 安装 qrcode 可生成二维码标签: pip install qrcode[pil]")

def main():
    """主函数"""
    print("\n🔍 ESP32 MAC地址获取工具\n")
    
    # 获取端口
    if len(sys.argv) > 1:
        port = sys.argv[1]
        print(f"📌 使用指定端口: {port}")
    else:
        port = find_esp32_port()
        if port:
            print(f"📌 自动检测到端口: {port}")
        else:
            print("❌ 未找到ESP32设备")
            print("\n可用端口:")
            for p in serial.tools.list_ports.comports():
                print(f"  - {p.device}: {p.description}")
            print("\n请手动指定端口: python tools/get_mac.py <端口>")
            sys.exit(1)
    
    # 读取MAC地址
    mac, lines = read_mac_from_serial(port)
    
    if mac:
        print(f"\n✅ 找到MAC地址: {mac}")
        
        # 打印标签
        print_mac_label(mac)
        
        # 保存到文件
        save_to_file(mac)
        
        # 生成二维码（可选）
        # generate_qr_label(mac)
        
        # 询问是否显示完整日志
        print("📋 完整串口日志:")
        show_log = input("是否显示完整串口日志? (y/n): ").lower()
        if show_log == 'y':
            print("\n" + "-"*50)
            for line in lines[-30:]:  # 显示最后30行
                print(line)
            print("-"*50)
        
        print("\n🎉 完成！请使用此MAC地址在管理后台注册设备")
        
    else:
        print("\n❌ 未能读取MAC地址")
        print("\n可能的原因:")
        print("  1. 设备未连接或未上电")
        print("  2. 端口选择错误")
        print("  3. 设备已启动完成（请按复位按钮重启）")
        print("  4. 波特率不匹配（当前: 115200）")
        
        print("\n📋 串口输出（最后30行）:")
        print("-"*50)
        for line in lines[-30:]:
            print(line)
        print("-"*50)
        
        sys.exit(1)

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n\n👋 用户中断")
        sys.exit(0)

