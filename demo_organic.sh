#!/bin/bash

# Demo: Organic Pattern Learning
# Shows how Melvin learns patterns without memorizing frequencies

echo "===================================================================="
echo "MELVIN ORGANIC - Pattern Learning Without Memorization"
echo "===================================================================="
echo ""
echo "Core Principle:"
echo "  Inputs are ephemeral → Extract patterns → Connect similar → Forget"
echo ""

# Clean start
rm -f organic.mmap

echo "--------------------------------------------------------------------"
echo "PHASE 1: Teaching patterns with rhyming words"
echo "--------------------------------------------------------------------"
echo ""

echo "Teaching: 'cat sat mat hat'"
echo "cat sat mat hat" | ./melvin_organic
echo ""

echo "Teaching: 'dog log fog'"
echo "dog log fog" | ./melvin_organic
echo ""

echo "Teaching: 'bat rat pat'"
echo "bat rat pat" | ./melvin_organic
echo ""

echo "--------------------------------------------------------------------"
echo "PHASE 2: Query - See organic connections"
echo "--------------------------------------------------------------------"
echo ""

echo "Query: 'cat' (should activate similar words AND sequence followers)"
echo "cat" | MELVIN_DEBUG=1 ./melvin_organic
echo ""

echo "Query: 'dog' (should activate log, fog)"
echo "dog" | MELVIN_DEBUG=1 ./melvin_organic
echo ""

echo "Query: 'bat' (should activate rat, pat, AND sat/mat/hat because similar!)"
echo "bat" | MELVIN_DEBUG=1 ./melvin_organic
echo ""

echo "--------------------------------------------------------------------"
echo "PHASE 3: Teach new context - Same words, different patterns"
echo "--------------------------------------------------------------------"
echo ""

echo "Teaching: 'cat dog friends' (new relationship!)"
echo "cat dog friends" | ./melvin_organic
echo ""

echo "Query: 'cat' (should now ALSO activate 'dog' and 'friends')"
echo "cat" | MELVIN_DEBUG=1 ./melvin_organic
echo ""

echo "--------------------------------------------------------------------"
echo "KEY INSIGHT:"
echo "--------------------------------------------------------------------"
echo "Notice:"
echo "  1. No frequency counting - just pattern connections"
echo "  2. Similar words get connected automatically (cat→bat→rat)"
echo "  3. Context changes over time - 'cat' learns NEW associations"
echo "  4. Input is forgotten - only the pattern graph remains"
echo ""
echo "This is ORGANIC GROWTH through similarity and co-occurrence!"
echo "===================================================================="

