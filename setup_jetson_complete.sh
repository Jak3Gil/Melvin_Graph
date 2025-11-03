#!/bin/bash
# Complete Jetson setup via USB serial

set -e

SERIAL_PORT="/dev/cu.usbmodem14217250286373"
JETSON_IP="169.254.123.100"
JETSON_USER="melvin"
JETSON_PASS="123456"
LOCAL_DIR="/Users/jakegilbert/Desktop/single_mel_file"

echo "╔══════════════════════════════════════════════════════╗"
echo "║     MELVIN JETSON SETUP VIA USB                      ║"
echo "╚══════════════════════════════════════════════════════╝"
echo ""

# Step 1: Connect via serial and run initial setup
echo "[1/4] Connecting to Jetson via USB serial..."
echo "       Serial port: $SERIAL_PORT"
echo ""

if [ ! -e "$SERIAL_PORT" ]; then
    echo "❌ Serial port not found: $SERIAL_PORT"
    echo "   Available USB devices:"
    ls -la /dev/cu.usb* 2>/dev/null || echo "   No USB devices found"
    exit 1
fi

echo "✅ Serial port found"
echo ""

# Run expect script to configure network
echo "[2/4] Running initial setup commands via serial..."
echo "       This will configure the USB network..."
echo ""

if command -v expect > /dev/null; then
    ./run_jetson_commands.exp &
    EXPECT_PID=$!
    
    echo "   Expect script running (PID: $EXPECT_PID)"
    echo "   Waiting for network configuration..."
    sleep 10
    
    # Check if we can ping the Jetson
    echo ""
    echo "[3/4] Testing network connection..."
    
    for i in {1..5}; do
        if ping -c 1 -W 1 $JETSON_IP > /dev/null 2>&1; then
            echo "✅ Network connection established!"
            break
        else
            echo "   Attempt $i/5: Waiting for Jetson network..."
            sleep 2
        fi
    done
    
    if ping -c 1 -W 1 $JETSON_IP > /dev/null 2>&1; then
        echo ""
        echo "[4/4] Deploying files via network..."
        echo ""
        
        # Now we can use the deploy script
        if [ -f "./deploy_jetson.sh" ]; then
            echo "   Using existing deploy script..."
            ./deploy_jetson.sh
        else
            echo "   Manual file deployment..."
            
            # Create tar of essential files
            echo "   Creating deployment package..."
            tar -czf /tmp/melvinos_deploy.tar.gz \
                melvin_core.c melvin_display.c \
                melvin_protocol.h shm_bridge.h shm_bridge.c \
                proc_monitor.h proc_monitor.c \
                sys_monitor.h sys_monitor.c \
                log_monitor.h log_monitor.c \
                net_monitor.h net_monitor.c \
                can_monitor.h can_monitor.c \
                melvind_main.c \
                Makefile Makefile_daemon \
                melvind.conf \
                setup_display_autostart.sh start_melvin_display.sh \
                2>/dev/null || echo "   Some files may be missing"
            
            echo "   Copying to Jetson..."
            sshpass -p "$JETSON_PASS" scp -o StrictHostKeyChecking=no \
                /tmp/melvinos_deploy.tar.gz "$JETSON_USER@$JETSON_IP:~/"
            
            echo "   Extracting and building on Jetson..."
            sshpass -p "$JETSON_PASS" ssh -o StrictHostKeyChecking=no "$JETSON_USER@$JETSON_IP" << 'EOF'
cd ~
tar -xzf melvinos_deploy.tar.gz -C ~/melvinos/ 2>/dev/null || mkdir -p ~/melvinos && tar -xzf melvinos_deploy.tar.gz -C ~/melvinos/
cd ~/melvinos

echo "Building melvind daemon..."
make -f Makefile_daemon clean
make -f Makefile_daemon

echo "Building melvin_core..."
make clean
make

echo "Building display..."
gcc -O2 -Wall -Wextra -std=c99 -o melvin_display melvin_display.c

echo "Setting up services..."
chmod +x setup_display_autostart.sh start_melvin_display.sh
./setup_display_autostart.sh

echo ""
echo "Build complete!"
ls -lh melvind melvin_core melvin_display 2>/dev/null || echo "Check for build errors"
EOF
            
            rm /tmp/melvinos_deploy.tar.gz
        fi
        
        echo ""
        echo "╔══════════════════════════════════════════════════════╗"
        echo "║     DEPLOYMENT COMPLETE                              ║"
        echo "╚══════════════════════════════════════════════════════╝"
        echo ""
        echo "✅ Melvin is ready on Jetson!"
        echo ""
        echo "To start services:"
        echo "  ssh melvin@$JETSON_IP  (password: 123456)"
        echo "  cd ~/melvinos"
        echo "  sudo systemctl start melvind"
        echo "  sudo systemctl start melvin-display"
        echo ""
        echo "To view on DisplayPort:"
        echo "  Connect HDMI/DisplayPort monitor to Jetson"
        echo "  Display will show Melvin's consciousness"
        echo ""
        
    else
        echo "❌ Network connection failed"
        echo ""
        echo "Manual steps required:"
        echo "1. Connect to serial console:"
        echo "   screen $SERIAL_PORT 115200"
        echo ""
        echo "2. Login as: melvin / 123456"
        echo ""
        echo "3. Configure network:"
        echo "   sudo ip addr add 169.254.123.100/16 dev usb0"
        echo "   sudo ip link set usb0 up"
        echo ""
        echo "4. Then run this script again"
        exit 1
    fi
    
else
    echo "❌ 'expect' not installed"
    echo ""
    echo "Install with: brew install expect"
    echo ""
    echo "Or connect manually:"
    echo "  screen $SERIAL_PORT 115200"
    echo "  (Ctrl+A then :quit to exit)"
    exit 1
fi

