#!/usr/bin/env python3
"""
ESP32 MAC地址检查工具
用于检查多个设备的MAC地址是否重复
"""

import serial
import time
import sys
from collections import defaultdict

def read_mac_from_device(port, baudrate=115200, timeout=5):
    """
    从ESP32设备读取MAC地址
    """
    try:
        ser = serial.Serial(port, baudrate, timeout=timeout)
        time.sleep(2)  # 等待设备启动
        
        # 发送重启命令（可选）
        # ser.write(b'\x03')  # Ctrl+C
        
        # 读取串口输出，查找MAC地址
        start_time = time.time()
        mac_address = None
        
        while time.time() - start_time < timeout:
            if ser.in_waiting:
                line = ser.readline().decode('utf-8', errors='ignore').strip()
                print(f"[{port}] {line}")
                
                # 查找MAC地址模式：XX:XX:XX:XX:XX:XX 或 AIOT_XXXXXXXXXXXX
                if 'MAC' in line.upper() or 'mac' in line:
                    # 尝试提取MAC地址
                    parts = line.split()
                    for part in parts:
                        if ':' in part and len(part) == 17:  # XX:XX:XX:XX:XX:XX
                            mac_address = part.upper()
                            break
        
        ser.close()
        return mac_address
        
    except Exception as e:
        print(f"❌ 读取设备 {port} 失败: {e}")
        return None

def check_duplicate_macs(mac_list):
    """
    检查MAC地址是否有重复
    """
    mac_count = defaultdict(list)
    
    for port, mac in mac_list:
        if mac:
            mac_count[mac].append(port)
    
    print("\n" + "="*60)
    print("MAC地址检查结果")
    print("="*60)
    
    has_duplicate = False
    for mac, ports in mac_count.items():
        if len(ports) > 1:
            print(f"⚠️  重复MAC: {mac}")
            print(f"   出现在设备: {', '.join(ports)}")
            has_duplicate = True
        else:
            print(f"✅ 唯一MAC: {mac} (设备: {ports[0]})")
    
    if has_duplicate:
        print("\n❌ 发现MAC地址重复！")
        print("\n可能原因:")
        print("1. 使用了未烧录唯一MAC的芯片")
        print("2. 购买了劣质或山寨ESP32模组")
        print("3. 芯片eFuse中的MAC地址相同")
        print("\n建议:")
        print("1. 更换正规渠道购买的ESP32-S3模组")
        print("2. 联系供应商确认MAC地址烧录情况")
        print("3. 使用esptool.py检查芯片MAC地址:")
        print("   esptool.py --port /dev/ttyUSB0 read_mac")
    else:
        print("\n✅ 所有设备MAC地址唯一")
    
    return not has_duplicate

def main():
    """
    主函数
    """
    if len(sys.argv) < 2:
        print("使用方法:")
        print(f"  {sys.argv[0]} <串口1> [串口2] [串口3] ...")
        print("\n示例:")
        print(f"  {sys.argv[0]} /dev/ttyUSB0 /dev/ttyUSB1 /dev/ttyUSB2")
        print(f"  {sys.argv[0]} COM3 COM4 COM5")
        sys.exit(1)
    
    ports = sys.argv[1:]
    print(f"将检查 {len(ports)} 个设备的MAC地址")
    print("="*60)
    
    mac_list = []
    for port in ports:
        print(f"\n正在读取设备: {port}")
        mac = read_mac_from_device(port)
        if mac:
            print(f"✅ 读取成功: {mac}")
            mac_list.append((port, mac))
        else:
            print(f"❌ 未能读取到MAC地址")
    
    if mac_list:
        check_duplicate_macs(mac_list)
    else:
        print("\n❌ 未能读取到任何MAC地址")

if __name__ == "__main__":
    main()


