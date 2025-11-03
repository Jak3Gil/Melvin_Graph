# Connect to Jetson via USB Serial

## Quick Connection

```bash
# Open serial console (will open in new window)
screen /dev/cu.usbmodem14217250286373 115200
```

**Login credentials:**
- Username: `melvin`
- Password: `123456`

**To exit screen:** Press `Ctrl+A` then type `:quit` and press Enter

---

## Setup Commands to Run on Jetson

Once logged in via serial, run these commands:

### 1. Create directory
```bash
mkdir -p ~/melvinos
cd ~/melvinos
```

### 2. Configure USB network
```bash
sudo ip addr add 169.254.123.100/16 dev usb0
sudo ip link set usb0 up
```

### 3. Verify network
```bash
ip addr show usb0
ping -c 3 169.254.123.1
```

### 4. Make network configuration permanent (optional)
```bash
sudo nano /etc/netplan/50-usb.yaml
```

Add this content:
```yaml
network:
  version: 2
  ethernets:
    usb0:
      addresses: [169.254.123.100/16]
```

Save and apply:
```bash
sudo netplan apply
```

---

## After Network is Configured

From your Mac, test the connection:
```bash
ping 169.254.123.100
```

If successful, deploy Melvin:
```bash
cd /Users/jakegilbert/Desktop/single_mel_file
./deploy_jetson.sh
```

---

## Alternative: Use the Helper Script

```bash
# This opens screen with the right settings
./connect_to_jetson.sh
```

Then manually type the setup commands above.

---

## Troubleshooting

**If screen says "Clear screen capability required":**
```bash
export TERM=xterm-256color
screen /dev/cu.usbmodem14217250286373 115200
```

**If login is locked after failed attempts:**
- Wait 5-10 minutes for the lockout to expire
- Or reboot the Jetson (power cycle)

**If USB network device not found:**
```bash
# On Jetson, check available interfaces:
ip link show

# May be named differently: l4tbr0, eth0, etc.
# Use the appropriate interface name instead of usb0
```

