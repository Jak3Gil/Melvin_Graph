#!/bin/bash
echo "Threshold decay test:"
echo ""

rm -f graph.mmap

echo "Tick 0: threshold = 0.50 (initial)"
echo ""
echo "After training 'cat sat, bat sat, mat sat'..."
echo "(This takes ~3-9 ticks)"
echo ""

{
  echo "cat sat"
  echo "bat sat"  
  echo "mat sat"
} | ./melvin 2>&1 > /dev/null

echo "Current threshold in graph:"
xxd -s 12 -l 4 graph.mmap | head -1
