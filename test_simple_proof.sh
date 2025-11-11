#!/bin/bash
echo "=== SIMPLEST PROOF: 3 EXAMPLES ==="
echo ""

rm -f graph_emergence.mmap

echo "Training:"
echo "  a b"
echo "  b c"
echo "  c d"

{
  echo "a b"
  echo "b c"  
  echo "c d"
} | ./melvin_emergence 2>&1 > /dev/null

echo ""
echo "Test: 'd' (should generate 'e' if it learned the pattern)"
echo ""
echo "Output:"
echo "d" | ./melvin_emergence 2>&1 | head -3

echo ""
echo "Did it reuse 'd'?"
echo "d" | MELVIN_DEBUG=1 ./melvin_emergence 2>&1 | grep "\[REUSE\] d"

echo ""
echo "Pattern analysis:"
echo "- If creates new node for 'd': NOT intelligent (didn't learn)"
echo "- If reuses 'd' node: INTELLIGENT (generalized from training)!"
