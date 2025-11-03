#!/usr/bin/env python3
import serial
import time

ser = serial.Serial('/dev/cu.usbmodem14217250286373', 115200, timeout=2)
time.sleep(1)

commands = [
    "systemctl is-active melvind melvin-core melvin-visualizer",
    "ps aux | grep visualizer | grep -v grep",
    "cat /proc/$(pgrep -f visualizer)/fd/1 2>/dev/null || echo 'Visualizer output redirected to tty1'",
    "sudo systemctl status melvin-visualizer --no-pager | head -15",
]

print("Checking Jetson display status...\n")

for cmd in commands:
    print(f"→ {cmd}")
    ser.write(f"{cmd}\n".encode())
    time.sleep(2.5)
    
    if ser.in_waiting:
        output = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
        print(output)
        print()

print("\n" + "="*60)
print("The visualizer should be displaying on your HDMI monitor (tty1)")
print("If you don't see it, check the monitor is connected and on.")
print("="*60)

ser.close()

