#!/bin/bash
echo "═══════════════════════════════════════════════════════"
echo "  MELVIN - Circuits Coded With Data"
echo "═══════════════════════════════════════════════════════"
echo ""

rm -f melvin.mmap
echo "" | ./melvin > /dev/null  # Initialize

echo "What's in the graph after init:"
./show_graph | head -3
echo ""

echo "DEMO 1: Teach Addition (Real Computation)"
echo "══════════════════════════════════════════"
echo ""
echo "Input: '5 + 7'"
echo "5 + 7" | ./melvin > /dev/null
echo ""
echo "What happened (check debug):"
echo "5 + 7" | MELVIN_DEBUG=1 ./melvin 2>&1 | grep -E "ROUTING|RESULT"
echo ""
echo "Graph structure created:"
./show_computation
echo ""

echo "DEMO 2: Query the Computation"
echo "══════════════════════════════════════════"
echo ""
echo "Query '+' (routes through graph to result):"
echo "+" | ./melvin
echo ""

echo "DEMO 3: Multiple Computations"
echo "══════════════════════════════════════════"
echo ""
echo "Teach: 3 + 4"
echo "3 + 4" | ./melvin > /dev/null
echo ""
echo "Teach: 10 + 20"
echo "10 + 20" | ./melvin > /dev/null
echo ""
echo "Query '+' (should show all results: 7, 12, 30):"
echo "+" | ./melvin
echo ""

echo "DEMO 4: Build Other Circuits"
echo "══════════════════════════════════════════"
echo ""
echo "Multiplication (lookup table):"
echo "2*3 6" | ./melvin > /dev/null
echo "5*5 25" | ./melvin > /dev/null
echo "Query '2*3':"
echo "2*3" | ./melvin
echo ""

echo "═══════════════════════════════════════════════════════"
echo "WHAT JUST HAPPENED:"
echo "═══════════════════════════════════════════════════════"
echo ""
echo "✓ Numbers decompose to bit nodes (5 → 5_bit0, 5_bit1, ...)"
echo "✓ Bits route through bit patterns (1+1+0 → 0,1)"
echo "✓ Result computed and stored (+ → 12)"
echo "✓ Query routes through entire circuit"
echo ""
echo "All structure. No hardcoded math (except bit patterns)."
echo "Computation happens by ROUTING through nodes."
echo ""
echo "Final graph size:"
./show_graph | head -1
echo "═══════════════════════════════════════════════════════"
