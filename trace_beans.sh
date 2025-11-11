#!/bin/bash
echo "=== TRACE WHY BEANS OUTPUTS SAT ===="
echo ""

rm -f graph.mmap

echo "Train:"
{
  echo "cat sat"
  echo "bat sat"
  echo "mat sat"
} | ./melvin 2>&1 > /dev/null

echo ""
echo "First test: 'beans' with FULL debug"
echo ""

# Let me look at the generalize_rules logic more carefully
cat melvin.c | grep -A 30 "void generalize_rules"
