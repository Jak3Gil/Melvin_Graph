#!/bin/bash
# Create systemd service for melvin_core

sudo tee /etc/systemd/system/melvin-core.service > /dev/null << 'SERVICE'
[Unit]
Description=Melvin Core Intelligence
After=network.target melvind.service
Wants=melvind.service

[Service]
Type=simple
User=melvin
WorkingDirectory=/home/melvin/melvinos
ExecStart=/bin/bash -c '/home/melvin/melvinos/melvin_core > /tmp/melvin_core.log 2>&1'
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
SERVICE

sudo systemctl daemon-reload
sudo systemctl enable melvin-core.service

echo "✓ Melvin Core service installed and enabled"
echo ""
echo "To start now:"
echo "  sudo systemctl start melvin-core"
echo ""
echo "Core will auto-start on boot"

