# 🔧 Fix Jetson Display - Quick Steps

## Current Situation

After removing the bloat (desktop/GNOME), the Jetson is now:
- ✅ Running minimal console system
- ✅ Boots faster (~10 seconds)
- ❌ Network not auto-configured (we removed NetworkManager)
- ❌ Display showing blank/console text only

## What You Need to Do

### Step 1: Connect via Serial Console

```bash
screen /dev/cu.usbmodem14217250286373 115200
```

**Login:**
- Username: `melvin`  
- Password: `123456`

### Step 2: Configure USB Network (on Jetson)

Once logged in, run these commands:

```bash
# Configure USB network
sudo ip addr add 169.254.123.100/16 dev usb0
sudo ip link set usb0 up

# Verify
ip addr show usb0
```

### Step 3: Make Network Permanent

```bash
# Create systemd-networkd config
sudo tee /etc/systemd/network/10-usb0.network > /dev/null << 'EOF'
[Match]
Name=usb0

[Network]
Address=169.254.123.100/16
EOF

# Enable systemd-networkd
sudo systemctl enable systemd-networkd
sudo systemctl start systemd-networkd
```

### Step 4: Exit Serial and Test Network

Exit screen: `Ctrl+A` then type `:quit`

From your Mac:
```bash
ping 169.254.123.100
```

If it responds ✅, continue to Step 5.

### Step 5: Install Framebuffer Visualizer

```bash
ssh melvin@169.254.123.100
cd ~/melvinos
chmod +x install_framebuffer_visualizer.sh
./install_framebuffer_visualizer.sh
```

### Step 6: Start the Display

```bash
sudo systemctl start melvin-visualizer

# Check if running
sudo systemctl status melvin-visualizer
```

**The monitor should now show Melvin's visualization!**

---

## Alternative: Quick Fix via Serial

Or run ALL commands via serial in one go:

```bash
screen /dev/cu.usbmodem14217250286373 115200

# Login: melvin / 123456

# Then paste all these:
sudo ip addr add 169.254.123.100/16 dev usb0
sudo ip link set usb0 up
sudo tee /etc/systemd/network/10-usb0.network > /dev/null << 'EOF'
[Match]
Name=usb0
[Network]
Address=169.254.123.100/16
EOF
sudo systemctl enable systemd-networkd
sudo systemctl start systemd-networkd
cd ~/melvinos
chmod +x install_framebuffer_visualizer.sh
./install_framebuffer_visualizer.sh
sudo systemctl start melvin-visualizer
```

---

## What Will Appear on Monitor

After starting melvin-visualizer:

```
╔══════════════════════════════════════════════════════╗
║        MELVINOS VISUALIZATION v1.0                   ║
╚══════════════════════════════════════════════════════╝

[Mind Stream Panel] [Brain Graph Panel]
                    [Metrics Panel]

- Fullscreen graphics
- No desktop clutter  
- Direct framebuffer rendering
- Fast updates
```

---

## Troubleshooting

**If monitor shows login prompt:**
- That's OK! The visualizer runs as root on tty1
- Services should auto-start

**If visualizer not showing:**
```bash
# Check service
sudo systemctl status melvin-visualizer

# Check logs  
sudo journalctl -u melvin-visualizer -n 50

# Run manually to debug
sudo python3 /home/melvin/melvinos/melvin_visualizer_fb.py
```

**If framebuffer permission denied:**
```bash
sudo usermod -aG video melvin
sudo chmod 666 /dev/fb0
```

---

## Expected Boot Sequence

```
Power On
  ↓
Kernel boot (5s)
  ↓
Console login prompt appears
  ↓
Services auto-start in background:
  - melvind (system monitor)
  - melvin-core (intelligence)
  - melvin-visualizer (display)
  ↓
Visualization takes over screen (~2s after login prompt)
  ↓
FULLSCREEN MELVIN DISPLAY
```

Total: ~7-10 seconds to visualization!

---

**TL;DR: Connect via serial, configure network, install framebuffer visualizer, start it!**

