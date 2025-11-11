#!/bin/bash
echo "=== DETAILED EXECUTION TRACE ==="
rm -f graph_exec.mmap

echo ""
echo "Step 1: Train"
echo "cat sat" | MELVIN_DEBUG=1 ./melvin_executable 2>&1 | grep -E "CREATE|RULE"

echo ""
echo "Step 2: Test (should execute cat→sat rule)"
echo "cat" | MELVIN_DEBUG=1 ./melvin_executable 2>&1 | grep -E "REUSE|RULE|OUTPUT"

echo ""
echo "Actual output:"
echo "cat" | ./melvin_executable 2>&1
