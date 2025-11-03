#!/bin/bash
# Deploy MelvinOS Daemon to Jetson via USB

JETSON_IP="melvin@169.254.123.100"
JETSON_PASS="123456"

echo "=== Deploying MelvinOS Daemon to Jetson ==="
echo "Target: $JETSON_IP"
echo ""

# Create deployment directory
echo "[1/5] Creating remote directory..."
sshpass -p "$JETSON_PASS" ssh -o StrictHostKeyChecking=no "$JETSON_IP" \
    "mkdir -p ~/melvinos && mkdir -p ~/melvinos/bin"

# Copy source files
echo "[2/5] Copying source files..."
sshpass -p "$JETSON_PASS" scp -o StrictHostKeyChecking=no \
    melvin_protocol.h shm_bridge.h shm_bridge.c \
    proc_monitor.h proc_monitor.c \
    sys_monitor.h sys_monitor.c \
    log_monitor.h log_monitor.c \
    net_monitor.h net_monitor.c \
    can_monitor.h can_monitor.c \
    melvind_main.c \
    Makefile_daemon \
    "$JETSON_IP:~/melvinos/"

# Copy melvin_core
echo "[3/6] Copying melvin_core..."
sshpass -p "$JETSON_PASS" scp -o StrictHostKeyChecking=no \
    melvin_core.c Makefile \
    "$JETSON_IP:~/melvinos/"

# Copy display files
echo "[4/6] Copying display files..."
sshpass -p "$JETSON_PASS" scp -o StrictHostKeyChecking=no \
    melvin_display.c setup_display_autostart.sh start_melvin_display.sh \
    "$JETSON_IP:~/melvinos/" 2>/dev/null || echo "Display files not found, skipping..."

# Build on Jetson
echo "[5/6] Building on Jetson..."
sshpass -p "$JETSON_PASS" ssh -o StrictHostKeyChecking=no "$JETSON_IP" << 'EOF'
cd ~/melvinos
echo "Building melvind daemon..."
make -f Makefile_daemon clean
make -f Makefile_daemon
echo "Building melvin_core..."
make clean
make
echo "Building melvin_display..."
gcc -O2 -Wall -Wextra -std=c99 -o melvin_display melvin_display.c 2>/dev/null || echo "Display build skipped"
chmod +x *.sh 2>/dev/null
echo "Build complete!"
ls -lh melvind melvin_core melvin_display 2>/dev/null
EOF

# Create systemd services
echo "[6/6] Creating systemd services..."
sshpass -p "$JETSON_PASS" ssh -o StrictHostKeyChecking=no "$JETSON_IP" << 'EOF'
sudo tee /etc/systemd/system/melvind.service > /dev/null << 'SERVICE'
[Unit]
Description=MelvinOS Daemon - System Monitor
After=network.target

[Service]
Type=simple
User=root
WorkingDirectory=/home/melvin/melvinos
ExecStart=/home/melvin/melvinos/melvind
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
SERVICE

sudo systemctl daemon-reload
echo "✅ Daemon service installed"

# Setup display service if script exists
if [ -f ~/melvinos/setup_display_autostart.sh ]; then
    echo "Setting up display service..."
    cd ~/melvinos && ./setup_display_autostart.sh
else
    echo "Display setup script not found, skipping..."
fi
EOF

echo ""
echo "=== Deployment Complete ==="
echo ""
echo "✅ Files deployed and built on Jetson"
echo ""
echo "To start services:"
echo "  ssh $JETSON_IP"
echo "  sudo systemctl start melvind        # Start daemon"
echo "  sudo systemctl start melvin-display # Start DisplayPort display"
echo ""
echo "To enable on boot:"
echo "  sudo systemctl enable melvind"
echo "  sudo systemctl enable melvin-display"
echo ""
echo "To view logs:"
echo "  sudo journalctl -u melvind -f"
echo "  sudo journalctl -u melvin-display -f"
echo ""
echo "To run manually:"
echo "  cd ~/melvinos"
echo "  ./melvin_core              # Run core"
echo "  ./melvin_display           # Run display"

