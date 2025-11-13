#!/bin/bash

# Comprehensive test of unified Melvin

GREEN='\033[0;32m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m'

passed=0
failed=0

test() {
    local desc=$1
    local input=$2
    local expected=$3
    
    result=$(echo "$input" | ./melvin 2>/dev/null)
    
    if echo "$result" | grep -q "$expected"; then
        echo -e "${GREEN}✓${NC} $desc"
        ((passed++))
    else
        echo -e "${RED}✗${NC} $desc (got: $result)"
        ((failed++))
    fi
}

echo "╔════════════════════════════════════════════════════════════╗"
echo "║           UNIFIED MELVIN - COMPREHENSIVE TEST              ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

rm -f melvin.mmap

echo "TEST SUITE 1: Organic Learning"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo "cat sat mat" | ./melvin > /dev/null 2>&1
test "Pattern learning" "cat" "sat"

echo "dog log fog" | ./melvin > /dev/null 2>&1
test "Multiple patterns" "dog" "log"

echo ""

echo "TEST SUITE 2: Arithmetic"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

test "Addition" "5 + 7" "12"
test "Subtraction" "10 - 3" "7"
test "Multiplication" "6 * 7" "42"
test "Division" "144 / 12" "12"
test "Large numbers" "999 + 1" "1000"

echo ""

echo "TEST SUITE 3: Meta-Learning"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Teach XOR
echo "0 XOR 0 = 0" | ./melvin > /dev/null 2>&1
echo "0 XOR 1 = 1" | ./melvin > /dev/null 2>&1
echo "1 XOR 0 = 1" | ./melvin > /dev/null 2>&1
echo "1 XOR 1 = 0" | ./melvin > /dev/null 2>&1

test "XOR(0,0)" "0 XOR 0" "0"
test "XOR(0,1)" "0 XOR 1" "1"
test "XOR(1,1)" "1 XOR 1" "0"

echo ""

echo "TEST SUITE 4: Mixed Mode"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

echo "five plus three" | ./melvin > /dev/null 2>&1
test "Symbolic learning" "five" "plus"
test "Numeric computation" "5 + 3" "8"

echo ""
echo "═══════════════════════════════════════════════════════════"
echo -e "Results: ${GREEN}$passed passed${NC}, ${RED}$failed failed${NC}"
echo "═══════════════════════════════════════════════════════════"

if [ $failed -eq 0 ]; then
    echo ""
    echo -e "${GREEN}🎉 ALL TESTS PASSED! 🎉${NC}"
    echo ""
    echo "Unified Melvin can:"
    echo "  ✓ Learn patterns organically"
    echo "  ✓ Compute arithmetic"
    echo "  ✓ Discover operations from examples"
    echo "  ✓ Handle mixed symbolic/numeric data"
    echo ""
    echo "All in ONE system with 3 simple rules!"
fi

echo ""

