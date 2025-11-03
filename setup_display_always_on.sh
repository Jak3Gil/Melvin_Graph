#!/bin/bash
# Setup display to stay on permanently

echo "Configuring display to stay on permanently..."

# 1. Create systemd service to disable screen blanking
sudo tee /etc/systemd/system/disable-screen-sleep.service > /dev/null << 'SERVICE'
[Unit]
Description=Disable Screen Power Saving
After=graphical.target gdm.service
Wants=graphical.target

[Service]
Type=oneshot
User=melvin
Environment="DISPLAY=:0"
Environment="XAUTHORITY=/run/user/1000/gdm/Xauthority"
ExecStart=/home/melvin/melvinos/disable_screen_sleep.sh
RemainAfterExit=yes

[Install]
WantedBy=graphical.target
SERVICE

# 2. Copy the script
sudo cp disable_screen_sleep.sh /home/melvin/melvinos/
sudo chmod +x /home/melvin/melvinos/disable_screen_sleep.sh
sudo chown melvin:melvin /home/melvin/melvinos/disable_screen_sleep.sh

# 3. Disable systemd-logind screen blanking
sudo mkdir -p /etc/systemd/logind.conf.d
sudo tee /etc/systemd/logind.conf.d/no-sleep.conf > /dev/null << 'LOGIND'
[Login]
HandlePowerKey=ignore
IdleAction=ignore
IdleActionSec=0
LOGIND

# 4. Disable console blanking
sudo tee /etc/systemd/system/disable-console-blanking.service > /dev/null << 'CONSOLE'
[Unit]
Description=Disable Console Blanking
After=multi-user.target

[Service]
Type=oneshot
ExecStart=/bin/sh -c 'setterm -blank 0 -powerdown 0 -powersave off > /dev/tty1'
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
CONSOLE

# 5. Enable services
sudo systemctl daemon-reload
sudo systemctl enable disable-screen-sleep.service
sudo systemctl enable disable-console-blanking.service
sudo systemctl start disable-screen-sleep.service
sudo systemctl start disable-console-blanking.service

# 6. Restart logind to apply changes
sudo systemctl restart systemd-logind

# 7. Apply settings now
export DISPLAY=:0
export XAUTHORITY=/run/user/1000/gdm/Xauthority
xset -dpms
xset s off
xset s noblank

echo ""
echo "╔══════════════════════════════════════════════════════╗"
echo "║  ✅ Display Power Saving Permanently Disabled        ║"
echo "╚══════════════════════════════════════════════════════╝"
echo ""
echo "Changes applied:"
echo "  ✓ DPMS disabled"
echo "  ✓ Screen saver disabled"
echo "  ✓ Screen blanking disabled"
echo "  ✓ Console blanking disabled"
echo "  ✓ Systemd-logind idle actions disabled"
echo "  ✓ Auto-start services enabled"
echo ""
echo "Display will now stay on permanently!"
echo ""

