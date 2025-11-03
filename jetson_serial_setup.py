#!/usr/bin/env python3
"""
Connect to Jetson via USB serial and run setup commands
"""

import serial
import time
import sys

SERIAL_PORT = "/dev/cu.usbmodem14217250286373"
BAUD_RATE = 115200
USERNAME = "melvin"
PASSWORD = "123456"

def send_command(ser, command, wait_time=1.0, expect_password=False):
    """Send a command and wait for response"""
    print(f"→ {command}")
    ser.write((command + "\n").encode())
    time.sleep(wait_time)
    
    if expect_password:
        # Send password if prompted
        time.sleep(0.5)
        ser.write((PASSWORD + "\n").encode())
        time.sleep(wait_time)
    
    # Read response
    response = ""
    while ser.in_waiting:
        response += ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
        time.sleep(0.1)
    
    if response:
        print(response)
    
    return response

def main():
    print("╔══════════════════════════════════════════════════════╗")
    print("║     MELVIN JETSON SETUP VIA USB SERIAL              ║")
    print("╚══════════════════════════════════════════════════════╝")
    print()
    print(f"Connecting to {SERIAL_PORT} at {BAUD_RATE} baud...")
    
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
        print()
        
        # Give it a moment to stabilize
        time.sleep(2)
        
        # Send newline to wake up the terminal
        ser.write(b"\n")
        time.sleep(1)
        
        # Read initial output
        initial_output = ""
        for _ in range(5):
            if ser.in_waiting:
                initial_output += ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
            time.sleep(0.2)
        
        print("Initial terminal state:")
        print(initial_output)
        print()
        
        # Check if we need to login
        if "login:" in initial_output or "Login" in initial_output:
            print("Logging in...")
            ser.write((USERNAME + "\n").encode())
            time.sleep(1)
            
            # Read response
            response = ""
            for _ in range(5):
                if ser.in_waiting:
                    response += ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
                time.sleep(0.2)
            
            print(response)
            
            # Send password
            if "Password" in response or "password" in response:
                ser.write((PASSWORD + "\n").encode())
                time.sleep(2)
                
                # Read login response
                login_response = ""
                for _ in range(5):
                    if ser.in_waiting:
                        login_response += ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
                    time.sleep(0.2)
                
                print(login_response)
                
                if "Login incorrect" in login_response:
                    print("❌ Login failed - check username/password")
                    ser.close()
                    return 1
                
                print("✅ Logged in successfully")
            
            # Clear any remaining output
            time.sleep(1)
            while ser.in_waiting:
                ser.read(ser.in_waiting)
        else:
            print("Already logged in or at a prompt")
            # Clear any existing output
            while ser.in_waiting:
                ser.read(ser.in_waiting)
        
        print()
        print("Running setup commands...")
        print("=" * 60)
        print()
        
        # Create directory
        send_command(ser, "mkdir -p ~/melvinos", 1.0)
        
        # Check current directory
        send_command(ser, "pwd", 0.5)
        
        # Check system info
        send_command(ser, "whoami", 0.5)
        send_command(ser, "hostname", 0.5)
        
        print()
        print("Configuring USB network...")
        print("-" * 60)
        
        # Configure USB network (may need password)
        send_command(ser, "sudo ip addr add 169.254.123.100/16 dev usb0 2>/dev/null || echo 'IP already set'", 2.0, expect_password=True)
        send_command(ser, "sudo ip link set usb0 up 2>/dev/null || echo 'Interface already up'", 2.0, expect_password=True)
        
        print()
        print("Checking network status...")
        print("-" * 60)
        
        send_command(ser, "ip addr show usb0 2>/dev/null || echo 'usb0 not available'", 1.0)
        
        print()
        print("Checking existing files...")
        print("-" * 60)
        
        send_command(ser, "ls -la ~/melvinos/ 2>/dev/null || echo 'Directory empty'", 1.0)
        
        print()
        print("Checking running services...")
        print("-" * 60)
        
        send_command(ser, "ps aux | grep -E 'melvin|melvind' | grep -v grep || echo 'No Melvin processes running'", 1.0)
        
        print()
        print("Checking display...")
        print("-" * 60)
        
        send_command(ser, "ls -la /dev/fb* 2>/dev/null || echo 'No framebuffer'", 1.0)
        
        print()
        print("=" * 60)
        print("✅ Setup commands completed")
        print()
        print("Network should be available at: 169.254.123.100")
        print("Test with: ping 169.254.123.100")
        print()
        
        ser.close()
        
    except serial.SerialException as e:
        print(f"❌ Serial port error: {e}")
        print()
        print("Make sure:")
        print("  1. Jetson is connected via USB")
        print("  2. No other program is using the serial port")
        print("  3. You have permission to access the device")
        return 1
    
    except KeyboardInterrupt:
        print("\n\n⚠️  Interrupted by user")
        return 1
    
    return 0

if __name__ == "__main__":
    sys.exit(main())

