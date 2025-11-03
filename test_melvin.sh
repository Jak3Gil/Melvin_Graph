#!/bin/bash
# Test script for melvin_core
# Demonstrates basic I/O and observation

echo "=== Testing Melvin Core ==="
echo ""
echo "Starting Melvin for 10 seconds with sample input..."
echo ""

# Run melvin in background for 10 seconds with some input
(
    sleep 1
    echo "hello world"
    sleep 1
    echo "/dev/video0 detected"
    sleep 1
    echo "error occurred"
    sleep 1
    echo "success: created file.txt"
    sleep 1
    echo "system ready"
    sleep 5
) | timeout 10s ./melvin_core

echo ""
echo "=== Test Complete ==="
echo ""
echo "Check nodes.bin and edges.bin for persisted state."
echo "Run './melvin_core' again to restore from saved state."

