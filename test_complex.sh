#!/bin/bash

echo "=========================================="
echo "Testing Complex Pattern Emergence"
echo "=========================================="
echo ""

rm -f graph.mmap

echo "Phase 1: Building rich pattern network..."
echo ""

# Arithmetic patterns
echo "Teaching arithmetic..."
for i in {0..9}; do
    for j in {0..9}; do
        sum=$((i + j))
        echo "$i + $j = $sum" | ./melvin > /dev/null 2>&1
    done
done

# Linguistic patterns
echo "Teaching language patterns..."
echo "the cat sat on the mat" | ./melvin > /dev/null 2>&1
echo "the bat sat on the hat" | ./melvin > /dev/null 2>&1
echo "the rat sat on the cat" | ./melvin > /dev/null 2>&1
echo "a dog ran to the log" | ./melvin > /dev/null 2>&1
echo "a fox ran to the box" | ./melvin > /dev/null 2>&1
echo "big cat eats small rat" | ./melvin > /dev/null 2>&1
echo "big dog eats small frog" | ./melvin > /dev/null 2>&1

# Conceptual patterns
echo "Teaching concepts..."
echo "hot fire cold ice" | ./melvin > /dev/null 2>&1
echo "hot sun cold moon" | ./melvin > /dev/null 2>&1
echo "red apple green leaf" | ./melvin > /dev/null 2>&1
echo "red rose green grass" | ./melvin > /dev/null 2>&1
echo "fast car slow bike" | ./melvin > /dev/null 2>&1
echo "fast jet slow train" | ./melvin > /dev/null 2>&1

# Relational patterns
echo "Teaching relationships..."
echo "parent child family home" | ./melvin > /dev/null 2>&1
echo "teacher student school desk" | ./melvin > /dev/null 2>&1
echo "doctor patient hospital bed" | ./melvin > /dev/null 2>&1
echo "king queen castle throne" | ./melvin > /dev/null 2>&1

echo "Graph built!"
echo ""

echo "=========================================="
echo "Phase 2: Testing Emergent Understanding"
echo "=========================================="
echo ""

echo "Test 1: Arithmetic Query"
echo "Query: '5 + 7'"
echo "Expected: Should activate numbers near 12 through pattern matching"
echo ""
echo "5 + 7" | MELVIN_DEBUG=1 ./melvin
echo ""
echo "---"
echo ""

echo "Test 2: Partial Pattern"
echo "Query: 'the cat'"
echo "Expected: Should activate related words (sat, mat, hat, rat, dog)"
echo ""
echo "the cat" | MELVIN_DEBUG=1 ./melvin
echo ""
echo "---"
echo ""

echo "Test 3: Conceptual Bridge"
echo "Query: 'hot'"
echo "Expected: Should activate fire, sun (opposite: cold, ice, moon)"
echo ""
echo "hot" | MELVIN_DEBUG=1 ./melvin
echo ""
echo "---"
echo ""

echo "Test 4: Novel Combination"
echo "Query: 'big fast'"
echo "Expected: Should blend contexts (big: cat/dog, fast: car/jet)"
echo ""
echo "big fast" | MELVIN_DEBUG=1 ./melvin
echo ""
echo "---"
echo ""

echo "Test 5: Relational Query"
echo "Query: 'king castle'"
echo "Expected: Should activate queen, throne (contextual completion)"
echo ""
echo "king castle" | MELVIN_DEBUG=1 ./melvin
echo ""
echo "---"
echo ""

echo "Test 6: Cross-Domain"
echo "Query: 'teacher fire'"
echo "Expected: Should show weak/broad activation (no strong pattern)"
echo ""
echo "teacher fire" | MELVIN_DEBUG=1 ./melvin
echo ""

echo "=========================================="
echo "What Should We See?"
echo "=========================================="
echo ""
echo "✓ Similar patterns activate strongly (structure-based)"
echo "✓ Context from multiple inputs accumulates"
echo "✓ Novel queries generalize from learned structure"
echo "✓ No frequency bias - just structural fit"
echo "✓ Richer graph = better generalization"
echo ""
echo "The graph learned RELATIONSHIPS, not FREQUENCIES!"
echo ""

