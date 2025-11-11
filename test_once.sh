#!/bin/bash
rm -f graph.mmap

echo "Train..."
{
  echo "cat sat"
  echo "bat sat"
  echo "mat sat"
} | ./melvin 2>&1 > /dev/null

echo ""
echo "Test 'beans' - should output NOTHING:"
echo ""
echo "beans" | ./melvin 2>&1

echo ""
echo "If it outputs 'sat', there's a BUG!"
