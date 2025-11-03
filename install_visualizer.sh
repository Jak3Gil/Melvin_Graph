#!/bin/bash
# Install MelvinOS Visualization Interface

set -e

echo "╔══════════════════════════════════════════════════════╗"
echo "║  MelvinOS Visualization Installer                    ║"
echo "╚══════════════════════════════════════════════════════╝"
echo ""

# No dependencies needed - uses built-in curses
echo "[1/5] Using curses-based visualizer (no external dependencies)..."

# Copy visualizer to system location
echo "[2/5] Installing visualizer..."
sudo cp melvin_visualizer_curses.py /home/melvin/melvinos/melvin_visualizer.py
sudo chmod +x /home/melvin/melvinos/melvin_visualizer.py
sudo chown melvin:melvin /home/melvin/melvinos/melvin_visualizer.py

# Stop old display service
echo "[3/5] Stopping old display service..."
sudo systemctl stop melvin-display 2>/dev/null || true
sudo systemctl disable melvin-display 2>/dev/null || true

# Install new visualizer service
echo "[4/5] Installing visualizer service..."
sudo cp melvin-visualizer.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable melvin-visualizer.service

# Start service
echo "[5/5] Starting visualizer..."
sudo systemctl start melvin-visualizer.service

echo ""
echo "╔══════════════════════════════════════════════════════╗"
echo "║  ✅ MelvinOS Visualizer Installed!                   ║"
echo "╚══════════════════════════════════════════════════════╝"
echo ""
echo "Status:"
sudo systemctl status melvin-visualizer --no-pager | head -10
echo ""
echo "The visualization will auto-start on boot."
echo "Monitor should show full 3D brain visualization!"
echo ""
echo "To view logs:"
echo "  sudo journalctl -u melvin-visualizer -f"
echo ""

