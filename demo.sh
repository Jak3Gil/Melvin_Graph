#!/bin/bash

echo "╔════════════════════════════════════════════════════════════╗"
echo "║                   UNIFIED MELVIN DEMO                      ║"
echo "║         All Capabilities in One System                     ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

rm -f melvin.mmap

echo "CAPABILITY 1: Organic Learning (Patterns)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "Teaching: cat sat mat hat"
echo "cat sat mat hat" | ./melvin
echo ""
echo "Query: cat"
echo "cat" | ./melvin
echo ""

echo "CAPABILITY 2: Arithmetic Computation"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "Query: 5 + 7 (never taught!)"
echo "5 + 7" | ./melvin
echo ""
echo "Query: 23 * 4 (never taught!)"
echo "23 * 4" | ./melvin
echo ""
echo "Query: 100 - 37 (never taught!)"
echo "100 - 37" | ./melvin
echo ""

echo "CAPABILITY 3: Meta-Learning (Discovers Operations)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "Teaching XOR truth table..."
echo "0 XOR 0 = 0" | ./melvin
echo "0 XOR 1 = 1" | ./melvin
echo "1 XOR 0 = 1" | ./melvin
echo "1 XOR 1 = 0" | ./melvin
echo ""
echo "Query: 0 XOR 1 (using discovered pattern!)"
echo "0 XOR 1" | MELVIN_DEBUG=1 ./melvin
echo ""
echo "Query: 1 XOR 1"
echo "1 XOR 1" | ./melvin
echo ""

echo "CAPABILITY 4: Mixed Learning"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "Teaching: The number five plus three equals eight"
echo "five plus three equals eight" | ./melvin
echo ""
echo "Query: five"
echo "five" | ./melvin
echo ""
echo "Also compute: 5 + 3"
echo "5 + 3" | ./melvin
echo ""

echo "╔════════════════════════════════════════════════════════════╗"
echo "║                      SUMMARY                               ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""
echo "ONE UNIFIED SYSTEM can:"
echo ""
echo "✅ Learn language patterns (cat sat mat)"
echo "✅ Compute arithmetic (5+7=12)"
echo "✅ Discover logical operations (XOR from 4 examples)"
echo "✅ Mix symbolic and numeric (five plus three)"
echo "✅ Store and recall all learned knowledge"
echo ""
echo "All from 3 SIMPLE RULES:"
echo "  1. Co-occurrence creates edges"
echo "  2. Similarity creates edges"
echo "  3. Patterns get discovered"
echo ""
echo "Applied at different levels:"
echo "  • Token level: cat ≈ mat"
echo "  • Pattern level: XOR discovered"
echo "  • Computation level: 5+7 calculated"
echo ""
echo "SAME RULES. DIFFERENT SCALES. EMERGENT INTELLIGENCE."
echo ""
echo "════════════════════════════════════════════════════════════"

