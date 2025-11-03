#!/bin/bash
# Start Melvin's consciousness display on DisplayPort

echo "Launching Melvin's Consciousness Display..."

# Set display to DisplayPort (usually HDMI-0 or DP-0 on Jetson)
export DISPLAY=:0

# Make terminal fullscreen
wmctrl -r :ACTIVE: -b add,fullscreen 2>/dev/null

# Clear screen
clear

# Launch the display
cd ~/melvinos

# Compile if needed
if [ ! -f "melvin_display" ]; then
    echo "Building display..."
    gcc -O2 -Wall -Wextra -std=c99 -o melvin_display melvin_display.c
fi

# Run fullscreen
./melvin_display


