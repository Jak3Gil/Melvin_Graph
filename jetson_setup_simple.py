#!/usr/bin/env python3
"""
Connect to Jetson and run setup commands after fresh restart
"""

import serial
import time
import sys

SERIAL_PORT = "/dev/cu.usbmodem14217250286373"
BAUD_RATE = 115200
USERNAME = "melvin"
PASSWORD = "123456"

def wait_for_text(ser, text, timeout=10):
    """Wait for specific text in serial output"""
    start = time.time()
    buffer = ""
    while time.time() - start < timeout:
        if ser.in_waiting:
            char = ser.read(1).decode('utf-8', errors='ignore')
            buffer += char
            print(char, end='', flush=True)
            if text in buffer:
                return True
        time.sleep(0.1)
    return False

def send_and_wait(ser, command, wait_for="$", timeout=5):
    """Send command and wait for prompt"""
    print(f"\n→ {command}")
    ser.write((command + "\n").encode())
    time.sleep(0.5)
    
    # Read output until we see the prompt
    start = time.time()
    buffer = ""
    while time.time() - start < timeout:
        if ser.in_waiting:
            char = ser.read(1).decode('utf-8', errors='ignore')
            print(char, end='', flush=True)
            buffer += char
            if wait_for in buffer[-50:]:  # Check last 50 chars for prompt
                return buffer
        time.sleep(0.05)
    
    return buffer

def main():
    print("╔══════════════════════════════════════════════════════╗")
    print("║     MELVIN JETSON SETUP (After Restart)             ║")
    print("╚══════════════════════════════════════════════════════╝")
    print()
    print(f"Connecting to {SERIAL_PORT}...")
    
    try:
        ser = serial.Serial(
            SERIAL_PORT,
            BAUD_RATE,
            timeout=1,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE
        )
        
        print("✅ Serial port opened")
        time.sleep(1)
        
        # Send a few newlines to see current state
        for _ in range(3):
            ser.write(b"\n")
            time.sleep(0.3)
            if ser.in_waiting:
                output = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
                print(output, end='')
        
        time.sleep(1)
        
        # Check if we need to login
        ser.write(b"\n")
        time.sleep(0.5)
        
        buffer = ""
        for _ in range(20):
            if ser.in_waiting:
                buffer += ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
            time.sleep(0.1)
        
        print("\nCurrent state:")
        print(buffer)
        
        if "login:" in buffer.lower():
            print("\n→ Logging in as melvin...")
            ser.write((USERNAME + "\n").encode())
            
            if wait_for_text(ser, "Password:", timeout=5):
                ser.write((PASSWORD + "\n").encode())
                time.sleep(2)
                
                # Clear buffer
                login_result = ""
                for _ in range(20):
                    if ser.in_waiting:
                        login_result += ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
                    time.sleep(0.1)
                
                if "incorrect" in login_result.lower():
                    print("\n❌ Login failed")
                    return 1
                
                print("\n✅ Logged in!")
        
        time.sleep(1)
        
        print("\n" + "=" * 60)
        print("RUNNING SETUP COMMANDS")
        print("=" * 60)
        
        # Create directory
        send_and_wait(ser, "mkdir -p ~/melvinos")
        send_and_wait(ser, "cd ~/melvinos")
        send_and_wait(ser, "pwd")
        
        print("\n" + "-" * 60)
        print("CONFIGURING USB NETWORK")
        print("-" * 60)
        
        # Configure network with sudo
        ser.write(b"sudo ip addr add 169.254.123.100/16 dev usb0\n")
        time.sleep(0.5)
        
        # Check if password is needed
        buffer = ""
        for _ in range(10):
            if ser.in_waiting:
                char = ser.read(1).decode('utf-8', errors='ignore')
                print(char, end='', flush=True)
                buffer += char
            time.sleep(0.1)
        
        if "password" in buffer.lower():
            print(f"\n→ Sending sudo password...")
            ser.write((PASSWORD + "\n").encode())
            time.sleep(2)
        
        # Bring interface up
        send_and_wait(ser, "sudo ip link set usb0 up")
        
        print("\n" + "-" * 60)
        print("VERIFYING NETWORK")
        print("-" * 60)
        
        send_and_wait(ser, "ip addr show usb0")
        send_and_wait(ser, "ping -c 2 169.254.123.1 || echo 'Ping failed'")
        
        print("\n" + "-" * 60)
        print("SYSTEM INFO")
        print("-" * 60)
        
        send_and_wait(ser, "hostname")
        send_and_wait(ser, "uname -a")
        send_and_wait(ser, "ls -la ~/melvinos/ | head -10 || echo 'Directory empty'")
        
        print("\n" + "=" * 60)
        print("✅ SETUP COMPLETE!")
        print("=" * 60)
        print("\nNetwork should now be accessible at: 169.254.123.100")
        print("Test from Mac with: ping 169.254.123.100")
        print("\nSerial connection remains open for monitoring...")
        print("Press Ctrl+C to exit\n")
        
        # Keep connection open
        try:
            while True:
                if ser.in_waiting:
                    char = ser.read(1).decode('utf-8', errors='ignore')
                    print(char, end='', flush=True)
                time.sleep(0.05)
        except KeyboardInterrupt:
            print("\n\nClosing connection...")
        
        ser.close()
        return 0
        
    except serial.SerialException as e:
        print(f"❌ Serial error: {e}")
        return 1
    except Exception as e:
        print(f"❌ Error: {e}")
        return 1

if __name__ == "__main__":
    sys.exit(main())

