#!/bin/bash
# Setup Melvin Display to auto-start on DisplayPort

echo "Setting up Melvin Display for auto-start..."

# Create systemd service for display
sudo tee /etc/systemd/system/melvin-display.service > /dev/null << 'SERVICE'
[Unit]
Description=Melvin Consciousness Display
After=graphical.target melvind.service gdm.service
Wants=graphical.target

[Service]
Type=simple
User=melvin
Environment="DISPLAY=:0"
Environment="TERM=xterm-256color"
Environment="XAUTHORITY=/run/user/1000/gdm/Xauthority"
WorkingDirectory=/home/melvin/melvinos
ExecStartPre=/bin/sleep 10
ExecStart=/home/melvin/melvinos/start_display_gui.sh
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=graphical.target
SERVICE

# Enable the service
sudo systemctl daemon-reload
sudo systemctl enable melvin-display.service

# Set console to auto-login (optional)
sudo mkdir -p /etc/systemd/system/getty@tty1.service.d/
sudo tee /etc/systemd/system/getty@tty1.service.d/override.conf > /dev/null << 'CONF'
[Service]
ExecStart=
ExecStart=-/sbin/agetty --autologin melvin --noclear %I $TERM
CONF

echo ""
echo "✓ Melvin Display service installed"
echo "✓ Auto-login configured for tty1"
echo ""
echo "To start now:"
echo "  sudo systemctl start melvin-display"
echo ""
echo "Display will auto-start on boot to DisplayPort/HDMI"

