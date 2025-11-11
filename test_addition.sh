#!/bin/bash
echo "=== ADDITION TEST ==="
echo ""

rm -f graph_emergence.mmap

echo "TRAINING (8 examples):"
for i in 1 2 3 4; do
  result=$((i + i))
  echo "  $i $i $result"
  echo "$i $i $result" | ./melvin_emergence 2>&1 > /dev/null
done

echo ""
echo "TEST: '5 5' (should output/predict '10')"
echo "5 5" | ./melvin_emergence 2>&1 | head -3

echo ""
echo "Checking if it reused number patterns:"
echo "5 5" | MELVIN_DEBUG=1 ./melvin_emergence 2>&1 | grep -c "\[REUSE\]"
