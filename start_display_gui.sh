#!/bin/bash
# Start Melvin Display on HDMI/DisplayPort monitor - FULLSCREEN 3-PANEL VERSION

export DISPLAY=:0
export TERM=xterm-256color

# Kill any existing displays
pkill -f melvin_display
pkill -f melvin_visualizer
sleep 1

# Wait for X to be ready
sleep 3

# Start FULLSCREEN xterm with NEW 3-panel visualizer
exec xterm -fullscreen -maximized \
    -bg black -fg white \
    -fa 'DejaVu Sans Mono' -fs 14 \
    +sb -bc \
    -geometry 240x68 \
    -title "Melvin Consciousness" \
    -e "cd /home/melvin/melvinos && TERM=xterm-256color exec python3 melvin_visualizer_curses.py"

