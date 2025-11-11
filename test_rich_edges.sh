#!/bin/bash
echo "=== RICH EDGES VERIFICATION ==="
echo ""

rm -f graph_edges.mmap

echo "Phase 1: Training"
{
  echo "cat sat"
  echo "bat sat"
  echo "mat sat"
} | ./melvin_edges > /dev/null

echo ""
echo "Edges created:"
./analyze_edges 2>/dev/null | grep -A 15 "EDGES BY TYPE"

echo ""
echo "Phase 2: Test generalization"
echo "Input: 'cat'"
echo ""
echo "Expected: Should activate"
echo "  - bat, mat (via similarity edges)"
echo "  - sat (via sequential edges)"
echo ""
echo "Actual output:"
echo "cat" | ./melvin_edges

echo ""
echo "Does it work? (Should see bat, mat, sat in output)"
