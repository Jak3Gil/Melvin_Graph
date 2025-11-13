#!/bin/bash

echo "╔════════════════════════════════════════════════════════════╗"
echo "║     META-LEARNING: Melvin Discovers Operations            ║"
echo "║              From Data Using Simple Rules                  ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""
echo "Approach:"
echo "  • Same simple rules as 'cat sat mat' learning"
echo "  • Applied to truth table examples"
echo "  • Melvin DISCOVERS what the operation does"
echo "  • NO hardcoded XOR/AND logic!"
echo ""
echo "════════════════════════════════════════════════════════════"
echo ""

echo "PHASE 1: Teaching XOR (Truth Table)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo "Example 1: 0 XOR 0 = 0"
echo "0 XOR 0 = 0" | MELVIN_DEBUG=1 ./melvin_meta_learn
echo ""

echo "Example 2: 0 XOR 1 = 1"
echo "0 XOR 1 = 1" | MELVIN_DEBUG=1 ./melvin_meta_learn
echo ""

echo "Example 3: 1 XOR 0 = 1"
echo "1 XOR 0 = 1" | MELVIN_DEBUG=1 ./melvin_meta_learn
echo ""

echo "Example 4: 1 XOR 1 = 0"
echo "1 XOR 1 = 0" | MELVIN_DEBUG=1 ./melvin_meta_learn
echo ""

echo "✓ All 4 examples taught!"
echo ""

echo "PHASE 2: Querying (Using Discovered Pattern)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo "Query: 0 XOR 0"
echo "0 XOR 0" | MELVIN_DEBUG=1 ./melvin_meta_learn
echo ""

echo "Query: 1 XOR 1"
echo "1 XOR 1" | MELVIN_DEBUG=1 ./melvin_meta_learn
echo ""

echo "Query: 0 XOR 1"
echo "0 XOR 1" | MELVIN_DEBUG=1 ./melvin_meta_learn
echo ""

echo "PHASE 3: Teaching AND (Same Simple Rules)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo "Teaching AND truth table..."
echo "0 AND 0 = 0" | ./melvin_meta_learn > /dev/null 2>&1
echo "0 AND 1 = 0" | ./melvin_meta_learn > /dev/null 2>&1
echo "1 AND 0 = 0" | ./melvin_meta_learn > /dev/null 2>&1
echo "1 AND 1 = 1" | MELVIN_DEBUG=1 ./melvin_meta_learn
echo ""

echo "Query: 1 AND 1"
echo "1 AND 1" | MELVIN_DEBUG=1 ./melvin_meta_learn
echo ""

echo "Query: 0 AND 1"
echo "0 AND 1" | MELVIN_DEBUG=1 ./melvin_meta_learn
echo ""

echo "PHASE 4: Teaching OR (Same Simple Rules)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo "Teaching OR truth table..."
echo "0 OR 0 = 0" | ./melvin_meta_learn > /dev/null 2>&1
echo "0 OR 1 = 1" | ./melvin_meta_learn > /dev/null 2>&1  
echo "1 OR 0 = 1" | ./melvin_meta_learn > /dev/null 2>&1
echo "1 OR 1 = 1" | MELVIN_DEBUG=1 ./melvin_meta_learn
echo ""

echo "Query: 1 OR 0"
echo "1 OR 0" | MELVIN_DEBUG=1 ./melvin_meta_learn
echo ""

echo "════════════════════════════════════════════════════════════"
echo ""
echo "WHAT JUST HAPPENED:"
echo ""
echo "Same Simple Rules (from 'cat sat mat' learning):"
echo "  1. Observe co-occurrence"
echo "  2. Find similarities"
echo "  3. Discover patterns"
echo ""
echo "Applied to Truth Tables:"
echo "  • Saw 4 examples of XOR"
echo "  • Discovered: 'output = inputs differ'"
echo "  • Created: Executable XOR operation"
echo "  • NO hardcoded XOR logic!"
echo ""
echo "Same for AND and OR:"
echo "  • Saw 4 examples each"
echo "  • Discovered: AND = both true, OR = either true"
echo "  • Created: Executable operations"
echo ""
echo "DATA CODED THE SYSTEM!"
echo "════════════════════════════════════════════════════════════"

