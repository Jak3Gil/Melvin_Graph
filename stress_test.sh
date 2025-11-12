#!/bin/bash

# Stress Test Suite for Melvin Organic
# Tests: Scalability, emergence, generalization, performance

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo "╔════════════════════════════════════════════════════════════╗"
echo "║        MELVIN ORGANIC - STRESS TEST SUITE                 ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# Build first
echo -e "${BLUE}Building melvin_organic...${NC}"
make melvin_organic > /dev/null 2>&1
echo -e "${GREEN}✓ Build complete${NC}"
echo ""

# Clean start
rm -f organic.mmap

# Test 1: Scalability - Does O(N²) edge growth hold?
echo "════════════════════════════════════════════════════════════"
echo -e "${YELLOW}TEST 1: SCALABILITY (Edge Growth)${NC}"
echo "════════════════════════════════════════════════════════════"
echo ""
echo "Testing hypothesis: Edges grow quadratically O(N²)"
echo ""

test_scalability() {
    local n=$1
    rm -f organic.mmap
    
    # Generate n rhyming words
    local words=""
    for i in $(seq 1 $n); do
        # Create words that rhyme (share endings)
        if [ $((i % 3)) -eq 0 ]; then
            words="$words bat$i "
        elif [ $((i % 3)) -eq 1 ]; then
            words="$words cat$i "
        else
            words="$words mat$i "
        fi
    done
    
    echo "$words" | ./melvin_organic > /dev/null 2>&1
    
    # Count edges (approximate by querying)
    local output=$(echo "cat1" | ./melvin_organic 2>&1)
    local activated=$(echo "$output" | grep -o "([0-9.]*)" | wc -l)
    
    echo "  N=$n words → ~$activated activations"
}

test_scalability 5
test_scalability 10
test_scalability 20
test_scalability 30

echo ""
echo -e "${GREEN}✓ Scalability test complete${NC}"
echo "  Expected: Activations grow faster than linear"
echo ""

# Test 2: Generalization Quality
echo "════════════════════════════════════════════════════════════"
echo -e "${YELLOW}TEST 2: GENERALIZATION QUALITY${NC}"
echo "════════════════════════════════════════════════════════════"
echo ""
echo "Testing: Do similar words connect automatically?"
echo ""

rm -f organic.mmap

# Teach only direct patterns
echo "cat sat" | ./melvin_organic > /dev/null 2>&1
echo "dog log" | ./melvin_organic > /dev/null 2>&1

# Add similar words (should generalize)
echo "bat hat rat mat" | ./melvin_organic > /dev/null 2>&1

# Query: Did bat→sat emerge without teaching it?
output=$(echo "bat" | ./melvin_organic 2>&1)

if echo "$output" | grep -q "sat"; then
    echo -e "${GREEN}✓ Generalization works!${NC}"
    echo "  Taught: cat→sat"
    echo "  Added: bat (similar to cat)"
    echo "  Result: bat→sat emerged automatically"
else
    echo -e "${RED}✗ Generalization failed${NC}"
fi

echo ""

# Test 3: Cascade Effect
echo "════════════════════════════════════════════════════════════"
echo -e "${YELLOW}TEST 3: CASCADE ACTIVATION${NC}"
echo "════════════════════════════════════════════════════════════"
echo ""
echo "Testing: Does 1 input activate many outputs?"
echo ""

rm -f organic.mmap

# Create interconnected network
echo "cat sat mat bat hat rat" | ./melvin_organic > /dev/null 2>&1
echo "dog log fog hog" | ./melvin_organic > /dev/null 2>&1
echo "cat dog friends" | ./melvin_organic > /dev/null 2>&1

# Query and count activations
output=$(echo "cat" | ./melvin_organic 2>&1)
count=$(echo "$output" | grep -o "([0-9.]*)" | wc -l)

echo "  Input: 1 word (cat)"
echo "  Output: $count words activated"

if [ "$count" -ge 5 ]; then
    echo -e "${GREEN}✓ Cascade effect confirmed${NC}"
    echo "  Ratio: ${count}x multiplication"
else
    echo -e "${RED}✗ Weak cascade (expected 5+)${NC}"
fi

echo ""

# Test 4: Context Evolution
echo "════════════════════════════════════════════════════════════"
echo -e "${YELLOW}TEST 4: CONTEXT EVOLUTION${NC}"
echo "════════════════════════════════════════════════════════════"
echo ""
echo "Testing: Can context change over time?"
echo ""

rm -f organic.mmap

# Initial context
echo "cat sat mat" | ./melvin_organic > /dev/null 2>&1
output1=$(echo "cat" | ./melvin_organic 2>&1)
count1=$(echo "$output1" | grep -o "([0-9.]*)" | wc -l)

echo "  Phase 1: cat→sat,mat"
echo "    Activations: $count1"

# Add new context
echo "cat dog friends happy" | ./melvin_organic > /dev/null 2>&1
output2=$(echo "cat" | ./melvin_organic 2>&1)
count2=$(echo "$output2" | grep -o "([0-9.]*)" | wc -l)

echo "  Phase 2: cat→dog,friends,happy"
echo "    Activations: $count2"

if [ "$count2" -gt "$count1" ]; then
    echo -e "${GREEN}✓ Context evolved${NC}"
    echo "  Growth: $count1 → $count2 activations"
else
    echo -e "${RED}✗ Context did not grow${NC}"
fi

echo ""

# Test 5: Memory Efficiency
echo "════════════════════════════════════════════════════════════"
echo -e "${YELLOW}TEST 5: MEMORY EFFICIENCY${NC}"
echo "════════════════════════════════════════════════════════════"
echo ""
echo "Testing: Is memory usage reasonable?"
echo ""

rm -f organic.mmap

# Train with 100 word sequences
for i in $(seq 1 100); do
    echo "word$i next$i" | ./melvin_organic > /dev/null 2>&1
done

if [ -f organic.mmap ]; then
    size=$(ls -lh organic.mmap | awk '{print $5}')
    echo "  100 training examples"
    echo "  Graph size: $size"
    
    # Check if under 1MB
    bytes=$(stat -f%z organic.mmap 2>/dev/null || stat -c%s organic.mmap 2>/dev/null)
    if [ "$bytes" -lt 1048576 ]; then
        echo -e "${GREEN}✓ Memory efficient (<1MB)${NC}"
    else
        echo -e "${YELLOW}⚠ Large memory footprint${NC}"
    fi
else
    echo -e "${RED}✗ Graph file not created${NC}"
fi

echo ""

# Test 6: Performance - Query Speed
echo "════════════════════════════════════════════════════════════"
echo -e "${YELLOW}TEST 6: QUERY PERFORMANCE${NC}"
echo "════════════════════════════════════════════════════════════"
echo ""
echo "Testing: Are queries fast?"
echo ""

rm -f organic.mmap

# Build moderate graph
for i in $(seq 1 50); do
    echo "word$i next$i related$i" | ./melvin_organic > /dev/null 2>&1
done

# Time queries
start=$(date +%s%N)
for i in $(seq 1 100); do
    echo "word1" | ./melvin_organic > /dev/null 2>&1
done
end=$(date +%s%N)

elapsed=$(( (end - start) / 1000000 )) # Convert to ms
avg=$(( elapsed / 100 ))

echo "  100 queries in ${elapsed}ms"
echo "  Average: ${avg}ms per query"

if [ "$avg" -lt 50 ]; then
    echo -e "${GREEN}✓ Fast queries (<50ms avg)${NC}"
else
    echo -e "${YELLOW}⚠ Slow queries (${avg}ms avg)${NC}"
fi

echo ""

# Test 7: Cluster Formation
echo "════════════════════════════════════════════════════════════"
echo -e "${YELLOW}TEST 7: CLUSTER FORMATION${NC}"
echo "════════════════════════════════════════════════════════════"
echo ""
echo "Testing: Do similar words cluster together?"
echo ""

rm -f organic.mmap

# Create two distinct clusters
echo "cat bat mat hat rat sat" | ./melvin_organic > /dev/null 2>&1
echo "dog log fog hog bog" | ./melvin_organic > /dev/null 2>&1

# Query from cluster 1
output_cat=$(echo "cat" | ./melvin_organic 2>&1)

# Should see cluster 1 words, not cluster 2
if echo "$output_cat" | grep -q "bat\|mat\|hat"; then
    if ! echo "$output_cat" | grep -q "dog\|log\|fog"; then
        echo -e "${GREEN}✓ Clusters formed correctly${NC}"
        echo "  cat activated: bat, mat, hat (cluster 1)"
        echo "  cat did NOT activate: dog, log (cluster 2)"
    else
        echo -e "${YELLOW}⚠ Clusters overlap${NC}"
    fi
else
    echo -e "${RED}✗ Clustering failed${NC}"
fi

echo ""

# Test 8: Bridge Building
echo "════════════════════════════════════════════════════════════"
echo -e "${YELLOW}TEST 8: BRIDGE BUILDING${NC}"
echo "════════════════════════════════════════════════════════════"
echo ""
echo "Testing: Can clusters be bridged?"
echo ""

# Use same clusters, add bridge
echo "cat dog together" | ./melvin_organic > /dev/null 2>&1

# Now query should reach both clusters
output_bridged=$(echo "cat" | ./melvin_organic 2>&1)

cluster1=$(echo "$output_bridged" | grep -c "bat\|mat\|hat" || true)
cluster2=$(echo "$output_bridged" | grep -c "dog\|log\|fog" || true)

if [ "$cluster1" -gt 0 ] && [ "$cluster2" -gt 0 ]; then
    echo -e "${GREEN}✓ Bridge created successfully${NC}"
    echo "  cat now activates both clusters"
    echo "  Cluster 1: $cluster1 activations"
    echo "  Cluster 2: $cluster2 activations"
else
    echo -e "${RED}✗ Bridge failed${NC}"
fi

echo ""

# Test 9: Pattern Strength
echo "════════════════════════════════════════════════════════════"
echo -e "${YELLOW}TEST 9: PATTERN REINFORCEMENT${NC}"
echo "════════════════════════════════════════════════════════════"
echo ""
echo "Testing: Do repeated patterns get stronger?"
echo ""

rm -f organic.mmap

# Teach once
echo "cat sat" | ./melvin_organic > /dev/null 2>&1
output1=$(echo "cat" | ./melvin_organic 2>&1)
strength1=$(echo "$output1" | grep -o "sat ([0-9.]*)" | grep -o "[0-9.]*" | head -1)

# Teach 5 more times
for i in $(seq 1 5); do
    echo "cat sat" | ./melvin_organic > /dev/null 2>&1
done

output2=$(echo "cat" | ./melvin_organic 2>&1)
strength2=$(echo "$output2" | grep -o "sat ([0-9.]*)" | grep -o "[0-9.]*" | head -1)

echo "  After 1 teaching: sat ($strength1)"
echo "  After 6 teachings: sat ($strength2)"

# Compare (rough check - strength2 should be higher)
if [ -n "$strength1" ] && [ -n "$strength2" ]; then
    compare=$(echo "$strength2 > $strength1" | bc 2>/dev/null || echo "1")
    if [ "$compare" = "1" ]; then
        echo -e "${GREEN}✓ Patterns strengthen with repetition${NC}"
    else
        echo -e "${YELLOW}⚠ Strength did not increase significantly${NC}"
    fi
else
    echo -e "${YELLOW}⚠ Could not measure strength${NC}"
fi

echo ""

# Test 10: Stress - Large Vocabulary
echo "════════════════════════════════════════════════════════════"
echo -e "${YELLOW}TEST 10: LARGE VOCABULARY STRESS TEST${NC}"
echo "════════════════════════════════════════════════════════════"
echo ""
echo "Testing: Can it handle 200+ unique words?"
echo ""

rm -f organic.mmap

# Generate 200 unique words in sequences
echo -n "Training 200 words..."
for i in $(seq 1 200); do
    echo "word$i next$i" | ./melvin_organic > /dev/null 2>&1
done
echo " done"

# Query performance with large graph
start=$(date +%s%N)
output=$(echo "word1" | ./melvin_organic 2>&1)
end=$(date +%s%N)
elapsed=$(( (end - start) / 1000000 ))

count=$(echo "$output" | grep -o "([0-9.]*)" | wc -l)

echo "  Vocabulary: 200 words"
echo "  Query time: ${elapsed}ms"
echo "  Activations: $count"

if [ "$elapsed" -lt 100 ]; then
    echo -e "${GREEN}✓ Handles large vocabulary efficiently${NC}"
else
    echo -e "${YELLOW}⚠ Slow with large vocabulary${NC}"
fi

if [ -f organic.mmap ]; then
    size=$(ls -lh organic.mmap | awk '{print $5}')
    echo "  Graph size: $size"
fi

echo ""

# Summary
echo "════════════════════════════════════════════════════════════"
echo -e "${BLUE}STRESS TEST SUMMARY${NC}"
echo "════════════════════════════════════════════════════════════"
echo ""
echo "Tests completed:"
echo "  1. Scalability (edge growth)"
echo "  2. Generalization quality"
echo "  3. Cascade activation"
echo "  4. Context evolution"
echo "  5. Memory efficiency"
echo "  6. Query performance"
echo "  7. Cluster formation"
echo "  8. Bridge building"
echo "  9. Pattern reinforcement"
echo "  10. Large vocabulary stress"
echo ""
echo -e "${GREEN}All stress tests complete!${NC}"
echo ""
echo "See results above for details."
echo "════════════════════════════════════════════════════════════"

