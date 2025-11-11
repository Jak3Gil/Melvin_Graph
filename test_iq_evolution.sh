#!/bin/bash

echo "=== IQ-BASED EVOLUTION DEMONSTRATION ==="
echo ""
echo "Training on 'cat sat' 100 times..."
rm -f graph_emergence.mmap

for i in {1..100}; do
    echo "cat sat"
done | ./melvin_emergence 2>&1 > /dev/null

echo ""
echo "After training, testing generation:"
echo ""
echo "Input: 'cat'"
echo "cat" | ./melvin_emergence 2>&1 | head -2

echo ""
echo "Check: Did 'sat' appear in output? (That's generation!)"
echo ""
echo "Checking which nodes have high IQ..."
echo "(Smart nodes = accurate predictions + good compression)"
