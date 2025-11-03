#!/bin/bash
# Enable DisplayPort output on Jetson

echo "Checking and enabling DisplayPort..."
echo ""

# Check framebuffer devices
echo "Framebuffer devices:"
ls -la /dev/fb*

echo ""
echo "Current framebuffer settings:"
fbset -i 2>/dev/null || echo "fbset not available"

echo ""
echo "Checking display outputs:"
cat /sys/class/drm/*/status 2>/dev/null || echo "DRM info not available"

echo ""
echo "Enabling framebuffer console..."

# Enable console on framebuffer
sudo sh -c 'echo 0 > /sys/class/vtconsole/vtcon1/bind' 2>/dev/null || true
sudo sh -c 'echo 1 > /sys/class/vtconsole/vtcon1/bind' 2>/dev/null || true

# Set framebuffer mode
sudo fbset -g 1920 1080 1920 1080 32 2>/dev/null || true

# Make sure console is on tty1
sudo chvt 1 2>/dev/null || true

# Clear and test framebuffer
echo ""
echo "Testing framebuffer output..."
sudo dd if=/dev/zero of=/dev/fb0 bs=1M count=8 2>/dev/null && echo "✅ Framebuffer is writable"

# Write test pattern
echo "Writing test pattern to display..."
sudo python3 << 'PYEOF'
try:
    with open('/dev/fb0', 'wb') as fb:
        # Write white pixels
        for i in range(1920 * 1080):
            fb.write(b'\xff\xff\xff\xff')  # BGRA white
    print("✅ Test pattern written to display")
except Exception as e:
    print(f"❌ Could not write to framebuffer: {e}")
PYEOF

echo ""
echo "Check your monitor - it should show a white screen!"
echo ""
echo "Press Enter to continue..."
read

# Clear to black
sudo dd if=/dev/zero of=/dev/fb0 bs=1M count=8 2>/dev/null

echo "Display cleared to black"

