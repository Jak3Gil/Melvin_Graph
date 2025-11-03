#!/usr/bin/env python3
"""Auto-setup minimal Jetson via serial"""

import serial
import time
import sys

PORT = "/dev/cu.usbmodem14217250286373"
USERNAME = "melvin"
PASSWORD = "123456"

def send_cmd(ser, cmd, wait=2.0, show_output=True):
    """Send command and optionally show output"""
    if show_output:
        print(f"  → {cmd}")
    ser.write(f"{cmd}\n".encode())
    time.sleep(wait)
    
    if show_output:
        output = ""
        for _ in range(30):
            if ser.in_waiting:
                output += ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
            time.sleep(0.05)
        if output.strip():
            print(output)
    return output

print("╔══════════════════════════════════════════════════════╗")
print("║  Auto-Setup Minimal Jetson Display                  ║")
print("╚══════════════════════════════════════════════════════╝")
print()

try:
    ser = serial.Serial(PORT, 115200, timeout=1)
    print(f"✅ Connected to serial port")
    time.sleep(2)
    
    # Wake up
    for _ in range(3):
        ser.write(b"\n")
        time.sleep(0.5)
    
    # Check state
    time.sleep(1)
    buffer = ""
    for _ in range(20):
        if ser.in_waiting:
            buffer += ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
        time.sleep(0.1)
    
    print(f"Terminal state: {buffer[:100]}...")
    
    # Login if needed
    if "login:" in buffer.lower():
        print(f"\n→ Logging in as {USERNAME}...")
        ser.write(f"{USERNAME}\n".encode())
        time.sleep(1.5)
        
        response = ""
        for _ in range(10):
            if ser.in_waiting:
                response += ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
            time.sleep(0.1)
        
        if "password" in response.lower():
            ser.write(f"{PASSWORD}\n".encode())
            time.sleep(3)
            print("✅ Logged in")
    
    # Clear buffer
    ser.read(ser.in_waiting or 1024)
    
    print("\n" + "="*60)
    print("STEP 1: Configure USB Network")
    print("="*60 + "\n")
    
    send_cmd(ser, "sudo ip addr add 169.254.123.100/16 dev usb0 2>/dev/null || true")
    send_cmd(ser, PASSWORD, wait=1.0, show_output=False)
    send_cmd(ser, "sudo ip link set usb0 up")
    send_cmd(ser, "ip addr show usb0 | grep inet")
    
    print("\n" + "="*60)
    print("STEP 2: Make Network Permanent")
    print("="*60 + "\n")
    
    send_cmd(ser, "sudo mkdir -p /etc/systemd/network")
    send_cmd(ser, """sudo tee /etc/systemd/network/10-usb0.network << 'EOF'
[Match]
Name=usb0

[Network]
Address=169.254.123.100/16
EOF""", wait=1.5)
    
    send_cmd(ser, "sudo systemctl enable systemd-networkd")
    send_cmd(ser, "sudo systemctl restart systemd-networkd")
    
    print("\n" + "="*60)
    print("STEP 3: Setup Framebuffer Access")
    print("="*60 + "\n")
    
    send_cmd(ser, "sudo chmod 666 /dev/fb0")
    send_cmd(ser, "sudo usermod -aG video melvin")
    send_cmd(ser, "ls -la /dev/fb0")
    
    print("\n" + "="*60)
    print("STEP 4: Install and Start Visualizer")
    print("="*60 + "\n")
    
    send_cmd(ser, "cd ~/melvinos")
    send_cmd(ser, "chmod +x install_framebuffer_visualizer.sh melvin_visualizer_fb.py")
    send_cmd(ser, "./install_framebuffer_visualizer.sh", wait=3.0)
    
    print("\n" + "="*60)
    print("STEP 5: Start Display on Monitor")
    print("="*60 + "\n")
    
    send_cmd(ser, "sudo systemctl start melvin-visualizer")
    time.sleep(2)
    send_cmd(ser, "systemctl status melvin-visualizer --no-pager | head -15")
    
    print("\n" + "="*60)
    print("✅ SETUP COMPLETE!")
    print("="*60)
    print()
    print("Check your HDMI monitor - Melvin's display should be showing!")
    print()
    print("Services running:")
    send_cmd(ser, "ps aux | grep -E 'melvin|melvind' | grep -v grep")
    
    print("\nPress Ctrl+C to exit, or wait for live output...")
    
    # Show live output
    try:
        while True:
            if ser.in_waiting:
                print(ser.read(ser.in_waiting).decode('utf-8', errors='ignore'),
                      end='', flush=True)
            time.sleep(0.05)
    except KeyboardInterrupt:
        pass
    
    ser.close()
    print("\n\n✅ Done! Check your monitor!")
    
except Exception as e:
    print(f"❌ Error: {e}")
    print("\nManual steps in FIX_DISPLAY_NOW.md")
    return 1

if __name__ == "__main__":
    sys.exit(main())

