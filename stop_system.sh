#!/bin/bash
# Stop MelvinOS system

echo "=== Stopping MelvinOS System ==="

# Stop melvin_core
pkill -SIGTERM melvin_core && echo "[✓] Stopped melvin_core"

# Stop shm_reader
pkill -SIGTERM shm_reader && echo "[✓] Stopped shm_reader"

# Stop melvind
sudo pkill -SIGTERM melvind && echo "[✓] Stopped melvind"

# Clean shared memory
make -f Makefile_daemon clean-shm 2>/dev/null && echo "[✓] Cleaned shared memory"

echo ""
echo "=== MelvinOS Stopped ==="

