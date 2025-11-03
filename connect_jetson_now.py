#!/usr/bin/env python3
import serial
import time
import sys
import select
import termios
import tty

PORT = "/dev/cu.usbmodem14217250286373"
USERNAME = "melvin"
PASSWORD = "123456"

def main():
    print("Connecting to Jetson...")
    print(f"Port: {PORT}")
    print(f"User: {USERNAME}")
    print()

    try:
        ser = serial.Serial(PORT, 115200, timeout=2)
        print("✅ Serial connected\n")
        time.sleep(2)
        
        # Send newlines to wake terminal
        for i in range(5):
            ser.write(b"\n")
            time.sleep(0.4)
        
        # Read current state
        time.sleep(1)
        buffer = ""
        for _ in range(30):
            if ser.in_waiting:
                buffer += ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
            time.sleep(0.1)
        
        print("Current state:")
        print(buffer)
        print()
        
        # Login if needed
        if "login:" in buffer.lower():
            print(f"→ Logging in as {USERNAME}...")
            ser.write(f"{USERNAME}\n".encode())
            time.sleep(1.5)
            
            response = ""
            for _ in range(15):
                if ser.in_waiting:
                    response += ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
                time.sleep(0.1)
            
            print(response)
            
            if "password" in response.lower():
                print(f"→ Sending password...")
                ser.write(f"{PASSWORD}\n".encode())
                time.sleep(3)
                
                login_result = ""
                for _ in range(20):
                    if ser.in_waiting:
                        login_result += ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
                    time.sleep(0.1)
                
                print(login_result)
                
                if "incorrect" in login_result.lower():
                    print("❌ Login failed!")
                    ser.close()
                    return 1
                
                print("✅ Logged in!\n")
        
        print("="*60)
        print("Connected - Interactive Session")
        print("="*60)
        print("Type commands or Ctrl+C to exit")
        print()
        
        # Interactive mode
        old_settings = termios.tcgetattr(sys.stdin)
        try:
            tty.setcbreak(sys.stdin.fileno())
            
            while True:
                # Keyboard input
                if select.select([sys.stdin], [], [], 0)[0]:
                    char = sys.stdin.read(1)
                    if ord(char) == 3:  # Ctrl+C
                        break
                    ser.write(char.encode())
                
                # Serial output
                if ser.in_waiting:
                    output = ser.read(ser.in_waiting).decode('utf-8', errors='ignore')
                    print(output, end='', flush=True)
                
                time.sleep(0.01)
        
        finally:
            termios.tcsetattr(sys.stdin, termios.TCSADRAIN, old_settings)
        
        ser.close()
        print("\n\n👋 Disconnected")
        return 0
    
    except KeyboardInterrupt:
        print("\n\n👋 Exiting...")
        return 0
    except Exception as e:
        print(f"❌ Error: {e}")
        return 1

if __name__ == "__main__":
    sys.exit(main())
