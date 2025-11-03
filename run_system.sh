#!/bin/bash
# Run the complete MelvinOS system

echo "=== Starting MelvinOS System ==="
echo ""

# Check if melvind is running
if pgrep -x "melvind" > /dev/null; then
    echo "[✓] melvind is already running"
else
    echo "[!] Starting melvind..."
    sudo ./melvind > /tmp/melvind.log 2>&1 &
    MELVIND_PID=$!
    sleep 2
    
    if ps -p $MELVIND_PID > /dev/null; then
        echo "[✓] melvind started (PID: $MELVIND_PID)"
    else
        echo "[✗] melvind failed to start. Check /tmp/melvind.log"
        exit 1
    fi
fi

# Check shared memory
if [ -e "/dev/shm/melvin_rx" ]; then
    echo "[✓] Shared memory buffers exist"
else
    echo "[✗] Shared memory not found. melvind may not be running correctly."
    exit 1
fi

echo ""
echo "Starting melvin_core with shared memory input..."
echo "Press Ctrl+C to stop"
echo ""

# Build shm_reader if needed
if [ ! -f "shm_reader" ]; then
    echo "Building shm_reader..."
    gcc -O2 -Wall -Wextra -std=c99 -o shm_reader shm_reader.c -lrt 2>/dev/null || \
    gcc -O2 -Wall -Wextra -std=c99 -o shm_reader shm_reader.c
fi

# Run melvin_core with shared memory input
./shm_reader | ./melvin_core

echo ""
echo "=== MelvinOS Stopped ==="

