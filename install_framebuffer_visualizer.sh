#!/bin/bash
# Install Framebuffer Visualizer (No X11 Required!)

echo "╔══════════════════════════════════════════════════════╗"
echo "║  Installing Framebuffer Visualizer                   ║"
echo "╚══════════════════════════════════════════════════════╝"
echo ""

# Test framebuffer access
if [ ! -e /dev/fb0 ]; then
    echo "❌ Framebuffer device not found!"
    exit 1
fi

echo "✅ Framebuffer device: /dev/fb0"

# Add user to video group for framebuffer access
sudo usermod -aG video melvin

# Make visualizer executable
chmod +x melvin_visualizer_fb.py

# Create/update visualizer service
sudo tee /etc/systemd/system/melvin-visualizer.service > /dev/null << 'SERVICE'
[Unit]
Description=MelvinOS Framebuffer Visualizer
After=multi-user.target melvind.service melvin-core.service
Wants=melvind.service melvin-core.service

[Service]
Type=simple
User=root
WorkingDirectory=/home/melvin/melvinos
ExecStartPre=/bin/sleep 3
ExecStart=/usr/bin/python3 /home/melvin/melvinos/melvin_visualizer_fb.py
Restart=always
RestartSec=5
StandardInput=tty
StandardOutput=journal
StandardError=journal
TTYPath=/dev/tty1

[Install]
WantedBy=multi-user.target
SERVICE

# Reload systemd
sudo systemctl daemon-reload
sudo systemctl enable melvin-visualizer

echo ""
echo "╔══════════════════════════════════════════════════════╗"
echo "║  ✅ Framebuffer Visualizer Installed!               ║"
echo "╚══════════════════════════════════════════════════════╝"
echo ""
echo "Service status:"
sudo systemctl status melvin-visualizer --no-pager | head -10 || true
echo ""
echo "To start now:"
echo "  sudo systemctl start melvin-visualizer"
echo ""
echo "Will auto-start on boot and display on HDMI/DisplayPort!"
echo "No X11, no desktop - just pure Melvin! 🧠"
echo ""

