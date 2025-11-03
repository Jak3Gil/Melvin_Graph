#!/usr/bin/env python3
"""
Automatically configure Jetson and start display via serial
"""

import serial
import time
import sys

PORT = "/dev/cu.usbmodem14217250286373"
USERNAME = "melvin"
PASSWORD = "123456"

def main():
    print("╔══════════════════════════════════════════════════════╗")
    print("║  Auto-Configuring Jetson Display via Serial         ║")
    print("╚══════════════════════════════════════════════════════╝")
    print()
    print("This will:")
    print("  1. Connect via serial")
    print("  2. Configure USB network")
    print("  3. Install framebuffer visualizer")
    print("  4. Start Melvin display on monitor")
    print()
    
    try:
        ser = serial.Serial(PORT, 115200, timeout=2)
        print(f"✅ Serial connected")
        time.sleep(2)
        
        # Send newlines
        for _ in range(5):
            ser.write(b"\n")
            time.sleep(0.3)
        
        # Read current state
        time.sleep(1)
        buffer = ser.read(ser.in_waiting or 1024).decode('utf-8', errors='ignore')
        print("Current state:", buffer[:200])
        
        # Login if needed
        if "login:" in buffer.lower():
            print("\n→ Logging in as melvin...")
            ser.write(f"{USERNAME}\n".encode())
            time.sleep(1)
            ser.write(f"{PASSWORD}\n".encode())
            time.sleep(3)
        
        # Clear buffer
        ser.read(ser.in_waiting or 1024)
        
        print("\n" + "="*60)
        print("CONFIGURING NETWORK")
        print("="*60)
        
        commands = [
            "sudo ip addr add 169.254.123.100/16 dev usb0 2>/dev/null || echo 'IP set'",
            PASSWORD,  # sudo password
            "sudo ip link set usb0 up",
            "ip addr show usb0 | grep inet || echo 'Network not ready'"
        ]
        
        for cmd in commands:
            if cmd == PASSWORD:
                print("→ [entering password]")
            else:
                print(f"→ {cmd}")
            ser.write(f"{cmd}\n".encode())
            time.sleep(1.5)
            if ser.in_waiting:
                output = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
                print(output)
        
        print("\n" + "="*60)
        print("CHECKING SERVICES")
        print("="*60)
        
        ser.write(b"systemctl is-active melvind melvin-core 2>&1\n")
        time.sleep(2)
        if ser.in_waiting:
            print(ser.read(ser.in_waiting).decode('utf-8', errors='ignore'))
        
        print("\n" + "="*60)
        print("STARTING FRAMEBUFFER DISPLAY")
        print("="*60)
        
        # Add user to video group
        ser.write(b"sudo usermod -aG video melvin\n")
        time.sleep(1)
        ser.write(f"{PASSWORD}\n".encode())
        time.sleep(1)
        
        # Make framebuffer accessible
        ser.write(b"sudo chmod 666 /dev/fb0\n")
        time.sleep(1)
        
        # Start visualizer directly
        ser.write(b"cd ~/melvinos\n")
        time.sleep(0.5)
        ser.write(b"sudo python3 melvin_visualizer_fb.py &\n")
        time.sleep(2)
        
        print("\n✅ Commands sent!")
        print("\nMonitor should show Melvin display in 3-5 seconds...")
        print("\nKeeping connection open for monitoring...")
        print("Press Ctrl+C to exit\n")
        
        # Show live output
        try:
            while True:
                if ser.in_waiting:
                    print(ser.read(ser.in_waiting).decode('utf-8', errors='ignore'), 
                          end='', flush=True)
                time.sleep(0.05)
        except KeyboardInterrupt:
            print("\n\n👋 Done!")
        
        ser.close()
        
    except Exception as e:
        print(f"❌ Error: {e}")
        return 1
    
    return 0

if __name__ == "__main__":
    sys.exit(main())

