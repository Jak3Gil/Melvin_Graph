#!/usr/bin/env python3
"""Quick serial connection to fix Jetson display"""

import serial
import time

PORT = "/dev/cu.usbmodem14217250286373"
BAUD = 115200

print("╔══════════════════════════════════════════════════════╗")
print("║  Connecting to Jetson to Fix Display                ║")
print("╚══════════════════════════════════════════════════════╝")
print()

try:
    ser = serial.Serial(PORT, BAUD, timeout=1)
    print(f"✅ Connected to {PORT}")
    time.sleep(2)
    
    # Wake up terminal
    for _ in range(5):
        ser.write(b"\n")
        time.sleep(0.5)
        if ser.in_waiting:
            print(ser.read(ser.in_waiting).decode('utf-8', errors='ignore'), end='')
    
    print("\n" + "="*60)
    print("Checking what's on the display...")
    print("="*60)
    
    # Check framebuffer
    ser.write(b"ls -la /dev/fb0\n")
    time.sleep(1)
    
    # Check if visualizer is running
    ser.write(b"ps aux | grep melvin\n")
    time.sleep(1)
    
    # Check services
    ser.write(b"systemctl status melvin-visualizer --no-pager | head -15\n")
    time.sleep(2)
    
    # Show all output
    print("\nJetson output:")
    print("-"*60)
    for _ in range(50):
        if ser.in_waiting:
            print(ser.read(ser.in_waiting).decode('utf-8', errors='ignore'), end='', flush=True)
        time.sleep(0.1)
    
    print("\n" + "="*60)
    print("\nConnection open - showing live output...")
    print("Press Ctrl+C to exit")
    print("="*60 + "\n")
    
    # Keep showing output
    while True:
        if ser.in_waiting:
            print(ser.read(ser.in_waiting).decode('utf-8', errors='ignore'), end='', flush=True)
        time.sleep(0.05)
        
except KeyboardInterrupt:
    print("\n\n👋 Closing...")
except Exception as e:
    print(f"❌ Error: {e}")

if __name__ == "__main__":
    import sys
    sys.exit(main())

