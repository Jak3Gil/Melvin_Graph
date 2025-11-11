#!/bin/bash
echo "=== MATHEMATICAL INTELLIGENCE TEST ==="
echo ""
echo "Can the system learn patterns and solve problems?"
echo ""

rm -f graph_emergence.mmap

echo "TRAINING (5 inputs):"
echo "  1 2"
echo "  2 3"
echo "  3 4"
echo "  4 5"
echo "  5 6"
echo ""

{
  echo "1 2"
  echo "2 3"
  echo "3 4"
  echo "4 5"
  echo "5 6"
} | ./melvin_emergence 2>&1 > /dev/null

echo "LEARNED PATTERNS:"
./show_tokens 2>/dev/null | grep -E "\"1|\"2|\"3|\"4|\"5|\"6" | head -10

echo ""
echo "TEST: Input '6' (should predict '7' next)"
echo "6" | ./melvin_emergence 2>&1 | head -2

echo ""
echo "Intelligence check:"
echo "6" | MELVIN_DEBUG=1 ./melvin_emergence 2>&1 | grep "\[REUSE\]" | head -5
