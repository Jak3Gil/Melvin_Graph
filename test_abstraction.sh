#!/bin/bash
echo "=== CAN SIMILARITY LEAD TO ABSTRACTION? ==="
echo ""

rm -f graph_similarity.mmap

echo "Training on doubling pattern:"
{
  echo "1 1 2"
  echo "2 2 4"
  echo "3 3 6"
} | MELVIN_DEBUG=1 ./melvin_similarity 2>&1 | grep "\[SIMILAR\]"

echo ""
echo "Test: '4 4' (should activate '8'?)"
echo "4 4" | ./melvin_similarity 2>&1

echo ""
echo "Analysis:"
echo "  - If outputs '8': Learned abstraction!"
echo "  - If outputs random: No abstraction"
echo "  - If outputs '2,4,6': Memorization not abstraction"
