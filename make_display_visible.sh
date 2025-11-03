#!/bin/bash
# Make SOMETHING visible on the Jetson's HDMI monitor
# Run this via serial console after logging in

echo "Making display visible on HDMI monitor..."

# Method 1: Simple repeating text to tty1
sudo pkill -f "echo.*tty1" 2>/dev/null

sudo bash -c 'while true; do 
  clear > /dev/tty1
  echo "╔══════════════════════════════════════════════════════╗" > /dev/tty1
  echo "║        MELVINOS - $(date +%H:%M:%S)                      ║" > /dev/tty1  
  echo "╚══════════════════════════════════════════════════════╝" > /dev/tty1
  echo "" > /dev/tty1
  echo "Services:" > /dev/tty1
  systemctl is-active melvind melvin-core melvin-visualizer > /dev/tty1
  echo "" > /dev/tty1
  echo "If you see this, the monitor is working!" > /dev/tty1
  echo "" > /dev/tty1
  free -h | head -2 > /dev/tty1
  sleep 2
done' &

echo "Started simple display on tty1"
echo "Check your HDMI monitor - you should see status updating every 2 seconds"
echo ""
echo "If working, press Ctrl+C and run:"
echo "  sudo systemctl restart melvin-visualizer"
echo "to switch to the full visualizer"

