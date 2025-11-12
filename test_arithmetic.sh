#!/bin/bash

echo "=========================================="
echo "Testing: Can Melvin Learn Arithmetic?"
echo "=========================================="
echo ""

rm -f graph.mmap

echo "=== TEACHING PHASE ==="
echo ""
echo "Teaching: 1 + 1 = 2"
echo "1 + 1 = 2" | ./melvin > /dev/null 2>&1

echo "Teaching: 2 + 2 = 4"  
echo "2 + 2 = 4" | ./melvin > /dev/null 2>&1

echo "Teaching: 3 + 3 = 6"
echo "3 + 3 = 6" | ./melvin > /dev/null 2>&1

echo ""
echo "Graph after teaching:"
echo "  Nodes: $(./show_activations | grep -c "^  [0-9]") number nodes"
echo "  Edges: $(grep "FINAL" <(echo "1 + 1" | MELVIN_DEBUG=1 ./melvin 2>&1) | awk '{print $4}')"
echo ""

echo "=== TESTING PHASE ==="
echo ""
echo "Query: '1 + 1'"
echo "Expected: Should activate '2' strongly"
echo ""
echo "Activations after query:"
echo "1 + 1" | ./melvin > /dev/null 2>&1
./show_activations | grep -E "^  [0-9]"
echo ""

echo "Output generated:"
echo "Result: '$(echo "1 + 1" | ./melvin)'"
echo ""

echo "=========================================="
echo "THE PROBLEM"
echo "=========================================="
echo ""
echo "All numbers saturate at same level (5.0)"
echo "Can't distinguish:"
echo "  - Direct input (1, +, 1) = 5.0"
echo "  - Predictions (2, 3, 4) = 5.0"
echo ""  
echo "Output filter looks for activation < 0.99"
echo "But everything is at 5.0 (saturated!)"
echo "So nothing passes the filter!"
echo ""
echo "SOLUTION NEEDED:"
echo "Track which nodes were DIRECT INPUT vs SPREAD"
echo "Or: Different saturation for input vs predictions"
echo "Or: Tag nodes with 'source' (input vs inferred)"
echo ""

