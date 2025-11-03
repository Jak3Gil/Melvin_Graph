# 🎯 Quick Fix - Make Display Fullscreen

## Problem
Display is working ✅ but:
- Only using 1/4 of screen ❌
- Showing old visualizer ❌

## Solution

The visualizer service is set to run on **tty1** (the text console). Since there's no X server running consistently, let's use the console directly!

### Via SSH - Run These Commands:

```bash
ssh melvin@169.254.123.100

# Stop service
sudo systemctl stop melvin-visualizer

# Run visualizer directly on console fullscreen
sudo openvt -c 1 -s -w -- python3 /home/melvin/melvinos/melvin_visualizer_curses.py
```

This will run the NEW 3-panel visualizer directly on tty1 and should use the full screen!

### Make It Permanent:

Update the service:

```bash
sudo tee /etc/systemd/system/melvin-visualizer.service > /dev/null << 'EOF'
[Unit]
Description=Melvin Console Visualizer
After=multi-user.target melvind.service melvin-core.service
Wants=melvind.service melvin-core.service

[Service]
Type=simple
User=root
WorkingDirectory=/home/melvin/melvinos
ExecStart=/usr/bin/openvt -c 1 -s -w -- python3 /home/melvin/melvinos/melvin_visualizer_curses.py
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable melvin-visualizer
sudo systemctl restart melvin-visualizer
```

This should make it FULLSCREEN on your monitor!

## What Changed

- Uses `openvt` to run directly on virtual console 1
- `-s` = switch to that console
- `-w` = wait for process
- Runs the NEW `melvin_visualizer_curses.py` (3-panel version)

**Your monitor should now show the full 3-panel visualization taking up the entire screen!** 🧠✨

