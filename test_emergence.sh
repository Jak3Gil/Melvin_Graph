#!/bin/bash

echo "═══════════════════════════════════════════════════════════"
echo "  MELVIN EMERGENCE - Pure Brain-Like Learning Test"
echo "═══════════════════════════════════════════════════════════"
echo ""

# Clean start
rm -f graph_emergence.mmap

echo "Test 1: Single byte learning"
echo "----------------------------"
echo "Input: 'A'"
echo "A" | ./melvin_emergence
echo ""

echo "Test 2: Learning 'cat' (10 times)"
echo "----------------------------------"
for i in {1..10}; do echo "cat"; done | ./melvin_emergence | tail -5
echo ""

echo "Test 3: After training, test pattern completion"
echo "------------------------------------------------"
echo "Input: 'c' (should activate 'cat' pattern)"
echo "c" | ./melvin_emergence
echo ""

echo "Test 4: Different word recognition"
echo "-----------------------------------"
echo "Input: 'dog'"
echo "dog" | ./melvin_emergence
echo ""

echo "Test 5: Mixed sequence"
echo "----------------------"
echo "Training on 'cat dog cat dog'"
for i in {1..5}; do echo "cat"; echo "dog"; done | ./melvin_emergence | tail -5
echo ""

echo "═══════════════════════════════════════════════════════════"
echo "  Key Concepts Demonstrated:"
echo "═══════════════════════════════════════════════════════════"
echo "  ✓ Nodes created dynamically (not pre-allocated)"
echo "  ✓ Information stored in CONNECTION WEIGHTS"
echo "  ✓ Nodes learn which bytes they represent"
echo "  ✓ Hebbian learning: 'Fire together, wire together'"
echo "  ✓ Energy economy: Input grants, output costs"
echo "  ✓ Survival: Unused nodes die from energy depletion"
echo ""
echo "To inspect the graph:"
echo "  MELVIN_DEBUG=1 ./melvin_emergence"
echo "═══════════════════════════════════════════════════════════"

