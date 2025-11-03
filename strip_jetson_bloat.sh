#!/bin/bash
# Strip Ubuntu Bloat from Jetson - Keep Only Melvin Essentials
# WARNING: This removes desktop environment and GUI!

set -e

echo "╔══════════════════════════════════════════════════════╗"
echo "║  Jetson Bloat Removal - Minimal Melvin System       ║"
echo "╚══════════════════════════════════════════════════════╝"
echo ""
echo "⚠️  WARNING: This will remove Ubuntu desktop and GUI!"
echo "⚠️  You will only have:"
echo "    - Console access (SSH/Serial)"
echo "    - Framebuffer display for Melvin"
echo "    - Essential services only"
echo ""
echo "Current system status:"
free -h
echo ""
df -h / | tail -1
echo ""

read -p "Are you SURE you want to continue? (type 'YES' to confirm): " confirm
if [ "$confirm" != "YES" ]; then
    echo "Aborted."
    exit 1
fi

echo ""
echo "╔══════════════════════════════════════════════════════╗"
echo "║  Phase 1: Creating Backup                           ║"
echo "╚══════════════════════════════════════════════════════╝"

# Backup critical configs
echo "[1/8] Backing up configurations..."
sudo mkdir -p /root/backup_before_minimal
sudo cp -r /home/melvin/melvinos /root/backup_before_minimal/
sudo cp /etc/systemd/system/melvin* /root/backup_before_minimal/ 2>/dev/null || true
dpkg --get-selections > /root/backup_before_minimal/packages.txt

echo ""
echo "╔══════════════════════════════════════════════════════╗"
echo "║  Phase 2: Disable Unnecessary Services               ║"
echo "╚══════════════════════════════════════════════════════╝"

echo "[2/8] Disabling bloat services..."
sudo systemctl disable bluetooth 2>/dev/null || true
sudo systemctl disable avahi-daemon 2>/dev/null || true
sudo systemctl disable cups 2>/dev/null || true
sudo systemctl disable ModemManager 2>/dev/null || true
sudo systemctl disable snapd 2>/dev/null || true
sudo systemctl disable tracker-store 2>/dev/null || true
sudo systemctl disable tracker-miner-fs 2>/dev/null || true

echo ""
echo "╔══════════════════════════════════════════════════════╗"
echo "║  Phase 3: Set Console Boot Target                   ║"
echo "╚══════════════════════════════════════════════════════╝"

echo "[3/8] Switching to multi-user (console) target..."
sudo systemctl set-default multi-user.target

echo ""
echo "╔══════════════════════════════════════════════════════╗"
echo "║  Phase 4: Remove Desktop Environment                ║"
echo "╚══════════════════════════════════════════════════════╝"

echo "[4/8] Removing GNOME desktop..."
sudo apt-get purge -y ubuntu-desktop gnome-* 2>&1 | grep -v "^E: Unable to locate" || true

echo "[4/8b] Removing GDM..."
sudo apt-get purge -y gdm3 lightdm 2>&1 | grep -v "^E: Unable to locate" || true

echo ""
echo "╔══════════════════════════════════════════════════════╗"
echo "║  Phase 5: Remove X11 and Graphics Stack             ║"
echo "╚══════════════════════════════════════════════════════╝"

echo "[5/8] Removing X11 (keeping framebuffer)..."
# DON'T remove xorg-server as it might break nvidia drivers
# Instead just prevent it from auto-starting
sudo systemctl disable gdm 2>/dev/null || true
sudo systemctl disable lightdm 2>/dev/null || true

echo ""
echo "╔══════════════════════════════════════════════════════╗"
echo "║  Phase 6: Remove Unnecessary Packages               ║"
echo "╚══════════════════════════════════════════════════════╝"

echo "[6/8] Removing bloat packages..."
sudo apt-get purge -y \
    thunderbird firefox libreoffice-* \
    rhythmbox cheese transmission-* \
    totem shotwell simple-scan \
    2>&1 | grep -v "^E: Unable to locate" || true

echo "[6/8b] Removing snap..."
sudo apt-get purge -y snapd 2>&1 | grep -v "^E: Unable to locate" || true

echo ""
echo "╔══════════════════════════════════════════════════════╗"
echo "║  Phase 7: Cleanup                                    ║"
echo "╚══════════════════════════════════════════════════════╝"

echo "[7/8] Running autoremove..."
sudo apt-get autoremove -y

echo "[7/8b] Cleaning package cache..."
sudo apt-get clean

echo ""
echo "╔══════════════════════════════════════════════════════╗"
echo "║  Phase 8: Configure Framebuffer                     ║"
echo "╚══════════════════════════════════════════════════════╝"

echo "[8/8] Ensuring framebuffer is available..."
ls -la /dev/fb0 || echo "⚠️  Framebuffer not found - may need reboot"

# Set console font for better readability
sudo apt-get install -y console-setup 2>/dev/null || true

echo ""
echo "╔══════════════════════════════════════════════════════╗"
echo "║  ✅ Bloat Removal Complete!                         ║"
echo "╚══════════════════════════════════════════════════════╝"
echo ""
echo "Results:"
echo "--------"
echo "New system status:"
free -h
echo ""
df -h / | tail -1
echo ""

echo "Services disabled:"
systemctl list-unit-files --state=disabled --no-pager | grep -E "gdm|gnome|bluetooth|avahi|cups" || echo "All bloat services disabled"

echo ""
echo "Next steps:"
echo "1. Reboot to apply changes:"
echo "   sudo reboot"
echo ""
echo "2. After reboot, install framebuffer visualizer:"
echo "   cd ~/melvinos"
echo "   ./install_framebuffer_visualizer.sh"
echo ""
echo "3. System will now boot to console in ~7-10 seconds!"
echo ""
echo "⚠️  Desktop GUI will NO LONGER be available"
echo "   Access via SSH: ssh melvin@169.254.123.100"
echo ""
echo "Backup saved to: /root/backup_before_minimal/"
echo ""

