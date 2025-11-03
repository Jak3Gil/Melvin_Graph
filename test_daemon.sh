#!/bin/bash
# Test the daemon locally (macOS or Linux)

echo "=== Testing MelvinOS Daemon Locally ==="
echo ""

# Build if needed
if [ ! -f "melvind" ]; then
    echo "Building melvind..."
    make -f Makefile_daemon
fi

# Start daemon
echo "Starting melvind..."
./melvind > /tmp/melvind_test.log 2>&1 &
DAEMON_PID=$!

echo "Daemon PID: $DAEMON_PID"
sleep 2

# Check if running
if ps -p $DAEMON_PID > /dev/null; then
    echo "[✓] Daemon is running"
else
    echo "[✗] Daemon failed to start"
    cat /tmp/melvind_test.log
    exit 1
fi

# Check shared memory
if [ -e "/dev/shm/melvin_rx" ]; then
    echo "[✓] Shared memory created"
    ls -lh /dev/shm/melvin_*
else
    echo "[✗] Shared memory not found"
    kill $DAEMON_PID
    exit 1
fi

echo ""
echo "Daemon is running. Monitoring for 10 seconds..."
echo "Check /tmp/melvind_test.log for output"
echo ""

sleep 10

# Show stats
echo "Daemon log (last 20 lines):"
tail -20 /tmp/melvind_test.log

# Stop daemon
echo ""
echo "Stopping daemon..."
kill $DAEMON_PID
wait $DAEMON_PID 2>/dev/null

# Clean up
make -f Makefile_daemon clean-shm

echo ""
echo "=== Test Complete ==="

