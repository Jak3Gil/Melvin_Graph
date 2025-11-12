#!/bin/bash

# Test: Can Melvin learn arithmetic through patterns?

echo "╔════════════════════════════════════════════════════════════╗"
echo "║         ARITHMETIC LEARNING TEST                           ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

rm -f organic.mmap

echo "PHASE 1: Teaching basic addition"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Teach addition facts
echo "Teaching: 1 + 1 = 2"
echo "1 + 1 = 2" | ./melvin_organic > /dev/null 2>&1

echo "Teaching: 2 + 2 = 4"
echo "2 + 2 = 4" | ./melvin_organic > /dev/null 2>&1

echo "Teaching: 3 + 3 = 6"
echo "3 + 3 = 6" | ./melvin_organic > /dev/null 2>&1

echo "Teaching: 1 + 2 = 3"
echo "1 + 2 = 3" | ./melvin_organic > /dev/null 2>&1

echo "Teaching: 2 + 1 = 3"
echo "2 + 1 = 3" | ./melvin_organic > /dev/null 2>&1

echo "Teaching: 1 + 3 = 4"
echo "1 + 3 = 4" | ./melvin_organic > /dev/null 2>&1

echo "Teaching: 3 + 1 = 4"
echo "3 + 1 = 4" | ./melvin_organic > /dev/null 2>&1

echo ""
echo "PHASE 2: Testing pattern recall"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Test 1: Query "1"
echo "Query: '1' (what follows?)"
output1=$(echo "1" | ./melvin_organic 2>&1)
echo "$output1"
echo ""

# Test 2: Query "="
echo "Query: '=' (what follows equals sign?)"
output_eq=$(echo "=" | ./melvin_organic 2>&1)
echo "$output_eq"
echo ""

# Test 3: Query "2"
echo "Query: '2' (what patterns with 2?)"
output2=$(echo "2" | ./melvin_organic 2>&1)
echo "$output2"
echo ""

echo "PHASE 3: Analysis"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Check if it learned the sequence pattern
if echo "$output_eq" | grep -q "2\|3\|4\|6"; then
    echo "✓ Learned: '=' is followed by results (2, 3, 4, 6)"
else
    echo "✗ Did not learn '=' → result pattern"
fi

# Check if numbers connect to operators
if echo "$output1" | grep -q "+"; then
    echo "✓ Learned: numbers followed by '+'"
else
    echo "✗ Did not learn number → operator"
fi

# Check if it associates similar patterns
if echo "$output1" | grep -q "2"; then
    strength=$(echo "$output1" | grep -o "2 ([0-9.]*)" | grep -o "[0-9.]*" | head -1)
    echo "✓ Learned: 1 connects to 2 (strength: $strength)"
else
    echo "✗ Did not connect 1 to 2"
fi

echo ""
echo "PHASE 4: Limitations"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo "What Melvin DOES learn:"
echo "  • Sequence patterns: '1' → '+' → '1' → '=' → '2'"
echo "  • Token relationships: numbers connect to operators"
echo "  • Result patterns: '=' followed by specific numbers"
echo ""

echo "What Melvin DOESN'T learn:"
echo "  ✗ Actual computation (doesn't calculate 1+1)"
echo "  ✗ Semantic meaning of '+' or '='"
echo "  ✗ Mathematical rules (commutativity, etc.)"
echo ""

echo "Why it doesn't 'compute':"
echo "  • Melvin learns PATTERNS, not OPERATIONS"
echo "  • '1 + 1 = 2' is just a sequence of tokens"
echo "  • No concept of arithmetic, just co-occurrence"
echo ""

echo "What would be needed for real arithmetic:"
echo "  1. Treat '1+1' as a single compound token"
echo "  2. Create direct edge: '1+1' → '2'"
echo "  3. Or implement actual computation layer"
echo ""

echo "CONCLUSION:"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "Melvin Organic learns PATTERNS, not MATH."
echo ""
echo "It can learn:"
echo "  ✓ '1' is often followed by '+'"
echo "  ✓ '+' is often followed by numbers"  
echo "  ✓ '=' is followed by results like 2, 3, 4"
echo ""
echo "But it CANNOT:"
echo "  ✗ Compute 1+1 from first principles"
echo "  ✗ Generalize to unseen equations"
echo "  ✗ Understand mathematical operations"
echo ""
echo "This is expected! Melvin is a pattern matcher,"
echo "not a symbolic calculator."
echo ""
echo "For real arithmetic, you'd need:"
echo "  • Dedicated computation nodes"
echo "  • Symbolic operation handling"
echo "  • Or: Teach EVERY equation as a pattern"
echo "    (which works for memorization, not computation)"
echo ""
echo "════════════════════════════════════════════════════════════"

