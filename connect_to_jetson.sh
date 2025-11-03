#!/bin/bash
# Simple helper to connect to Jetson via USB serial

SERIAL_PORT="/dev/cu.usbmodem14217250286373"

echo "╔══════════════════════════════════════════════════════╗"
echo "║     CONNECTING TO JETSON VIA USB SERIAL             ║"
echo "╚══════════════════════════════════════════════════════╝"
echo ""
echo "Serial port: $SERIAL_PORT"
echo "Baud rate: 115200"
echo ""
echo "Login credentials:"
echo "  Username: melvin"
echo "  Password: 123456"
echo ""
echo "To exit screen:"
echo "  Press Ctrl+A, then type :quit and press Enter"
echo ""
echo "After login, run these commands:"
echo "  mkdir -p ~/melvinos"
echo "  sudo ip addr add 169.254.123.100/16 dev usb0"
echo "  sudo ip link set usb0 up"
echo ""
echo "Press Enter to connect..."
read

# Set TERM for screen
export TERM=xterm-256color

# Connect
exec screen $SERIAL_PORT 115200

