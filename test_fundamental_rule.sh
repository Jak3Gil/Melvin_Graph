#!/bin/bash
echo "=== THE FUNDAMENTAL RULE ==="
echo ""
echo "For SURVIVAL:"
echo "  energy <= 0 → DEATH"
echo ""
echo "For INTELLIGENCE:"
echo "  prediction_error > 0.7 → DEATH"
echo ""
echo "Testing..."
rm -f graph_emergence.mmap

for i in {1..100}; do
    echo "cat sat"
    echo "dog ran"
done | ./melvin_emergence 2>&1 > /dev/null

echo ""
echo "Input: 'cat' (should predict 'sat')"
echo "cat" | ./melvin_emergence 2>&1 | head -2
