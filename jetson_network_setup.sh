#!/bin/bash
# Jetson USB Network Setup Script
# Run this ON THE JETSON after connecting via serial console

echo "═══════════════════════════════════════════════════"
echo "MELVINOS - Jetson USB Network Setup"
echo "═══════════════════════════════════════════════════"
echo ""

# Check if running as root
if [ "$EUID" -eq 0 ]; then
    SUDO=""
else
    SUDO="sudo"
fi

# Find USB network interface
USB_IF=""
for iface in usb0 rndis0 l4tbr0; do
    if ip link show $iface &>/dev/null; then
        USB_IF=$iface
        echo "✓ Found USB interface: $USB_IF"
        break
    fi
done

if [ -z "$USB_IF" ]; then
    echo "✗ No USB network interface found"
    echo "  Available interfaces:"
    ip link show | grep "^[0-9]" | cut -d: -f2
    echo ""
    echo "  Trying to load USB networking..."
    $SUDO modprobe g_ether
    $SUDO modprobe rndis_host
    sleep 2
    
    # Try again
    for iface in usb0 rndis0 l4tbr0; do
        if ip link show $iface &>/dev/null; then
            USB_IF=$iface
            echo "✓ Found USB interface: $USB_IF"
            break
        fi
    done
fi

if [ -z "$USB_IF" ]; then
    echo "✗ Still no USB interface. Check your USB connection."
    exit 1
fi

echo ""
echo "Setting up network on $USB_IF..."

# Configure IP address
echo "Setting IP 169.254.123.100/16..."
$SUDO ip addr flush dev $USB_IF 2>/dev/null
$SUDO ip addr add 169.254.123.100/16 dev $USB_IF

# Bring interface up
echo "Bringing interface up..."
$SUDO ip link set $USB_IF up

sleep 2

# Verify
echo ""
echo "═══════════════════════════════════════════════════"
echo "Network Configuration:"
echo "═══════════════════════════════════════════════════"
ip addr show $USB_IF

echo ""
echo "═══════════════════════════════════════════════════"
echo "Testing connectivity..."
echo "═══════════════════════════════════════════════════"

# Try to ping the host (Mac) at common gateway
ping -c 2 169.254.123.1 &>/dev/null && echo "✓ Can reach host" || echo "✗ Cannot reach host (may be normal)"

# Ensure SSH is running
echo ""
echo "Ensuring SSH is enabled..."
$SUDO systemctl enable ssh 2>/dev/null
$SUDO systemctl start ssh 2>/dev/null
$SUDO systemctl status ssh | head -3

echo ""
echo "═══════════════════════════════════════════════════"
echo "Setup Complete!"
echo "═══════════════════════════════════════════════════"
echo ""
echo "Network Details:"
echo "  Interface: $USB_IF"
echo "  IP Address: 169.254.123.100"
echo "  SSH: Enabled"
echo ""
echo "From your Mac, you should now be able to:"
echo "  ping 169.254.123.100"
echo "  ssh melvin@169.254.123.100"
echo "  ./deploy_jetson.sh"
echo ""

