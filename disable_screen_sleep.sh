#!/bin/bash
# Disable screen power saving and blanking on Jetson

export DISPLAY=:0
export XAUTHORITY=/run/user/1000/gdm/Xauthority

# Disable DPMS (Display Power Management Signaling)
xset -dpms

# Disable screen saver
xset s off

# Disable screen blanking
xset s noblank

echo "Display power saving disabled"

