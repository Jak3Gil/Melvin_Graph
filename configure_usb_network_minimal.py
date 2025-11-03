#!/usr/bin/env python3
"""
Configure USB network on minimal Jetson via serial
"""

import serial
import time

SERIAL_PORT = "/dev/cu.usbmodem14217250286373"
BAUD_RATE = 115200
USERNAME = "melvin"
PASSWORD = "123456"

def send_and_wait(ser, command, wait=1.5):
    """Send command and read response"""
    print(f"→ {command}")
    ser.write((command + "\n").encode())
    time.sleep(wait)
    
    response = ""
    for _ in range(20):
        if ser.in_waiting:
            response += ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
        time.sleep(0.1)
    
    print(response)
    return response

def main():
    print("╔══════════════════════════════════════════════════════╗")
    print("║  Configuring USB Network on Minimal Jetson         ║")
    print("╚══════════════════════════════════════════════════════╝")
    print()
    
    try:
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1)
        print(f"✅ Connected to {SERIAL_PORT}")
        time.sleep(2)
        
        # Send newlines to wake up
        for _ in range(3):
            ser.write(b"\n")
            time.sleep(0.3)
        
        # Check current state
        time.sleep(1)
        buffer = ""
        for _ in range(20):
            if ser.in_waiting:
                buffer += ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
            time.sleep(0.1)
        
        print("Current state:")
        print(buffer)
        print()
        
        # Login if needed
        if "login:" in buffer.lower():
            print("Logging in...")
            ser.write((USERNAME + "\n").encode())
            time.sleep(1)
            
            response = ""
            for _ in range(10):
                if ser.in_waiting:
                    response += ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
                time.sleep(0.1)
            print(response)
            
            if "password" in response.lower():
                ser.write((PASSWORD + "\n").encode())
                time.sleep(2)
                
                # Clear buffer
                for _ in range(10):
                    if ser.in_waiting:
                        ser.read(ser.in_waiting)
                    time.sleep(0.1)
        
        print("\n" + "="*60)
        print("CONFIGURING USB NETWORK")
        print("="*60 + "\n")
        
        send_and_wait(ser, "sudo ip addr add 169.254.123.100/16 dev usb0 2>/dev/null || echo 'IP set'")
        send_and_wait(ser, PASSWORD)  # sudo password
        
        send_and_wait(ser, "sudo ip link set usb0 up")
        
        send_and_wait(ser, "ip addr show usb0")
        
        print("\n" + "="*60)
        print("CHECKING SERVICES")
        print("="*60 + "\n")
        
        send_and_wait(ser, "systemctl is-active melvind melvin-core melvin-visualizer")
        send_and_wait(ser, "ps aux | grep melvin | grep -v grep | head -10")
        
        print("\n" + "="*60)
        print("INSTALLING FRAMEBUFFER VISUALIZER")
        print("="*60 + "\n")
        
        send_and_wait(ser, "cd ~/melvinos")
        send_and_wait(ser, "chmod +x install_framebuffer_visualizer.sh")
        send_and_wait(ser, "./install_framebuffer_visualizer.sh", wait=3.0)
        
        print("\n✅ Configuration complete!")
        print("\nNetwork should be available at: 169.254.123.100")
        print("Press Ctrl+C to exit, or wait for interactive mode...")
        print()
        
        # Keep connection open
        try:
            while True:
                if ser.in_waiting:
                    print(ser.read(ser.in_waiting).decode('utf-8', errors='ignore'), end='', flush=True)
                time.sleep(0.05)
        except KeyboardInterrupt:
            print("\n\n👋 Closing connection...")
        
        ser.close()
        
    except Exception as e:
        print(f"❌ Error: {e}")
        return 1
    
    return 0

if __name__ == "__main__":
    import sys
    sys.exit(main())

