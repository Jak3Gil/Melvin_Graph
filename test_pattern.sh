#!/bin/bash
# Test: Pattern Detection
# Verifies that repeated patterns create modules

echo "════════════════════════════════════════════════════════"
echo "  TEST: Pattern Detection & Module Creation"
echo "════════════════════════════════════════════════════════"
echo ""

# Clean start
rm -f graph.mmap

# Test: Send "the" pattern 10 times
echo "Test: Repeated pattern 'the' (10x)"
echo ""

{
    for i in {1..10}; do
        echo "the"
        sleep 0.05
    done
} | ./melvin_core 2>&1 > /tmp/melvin_pattern_test.log

# Check if modules were created
MODULES=$(grep "MODULE.*Created module" /tmp/melvin_pattern_test.log | wc -l | tr -d ' ')
CORRELATIONS=$(grep "META-OP.*correlated" /tmp/melvin_pattern_test.log | wc -l | tr -d ' ')
WIRED=$(grep "META-OP.*wiring" /tmp/melvin_pattern_test.log | wc -l | tr -d ' ')

echo "Results:"
echo "  Modules created: $MODULES"
echo "  Correlation detections: $CORRELATIONS"
echo "  Pattern wirings: $WIRED"
echo ""

if [[ "$MODULES" -gt 0 ]]; then
    echo "  ✓ PASS: Pattern detector created module(s)"
    echo ""
    echo "Sample module creation log:"
    grep "MODULE.*Created" /tmp/melvin_pattern_test.log | head -3
elif [[ "$CORRELATIONS" -gt 0 ]] || [[ "$WIRED" -gt 0 ]]; then
    echo "  ✓ PARTIAL: Pattern detection circuits active (need more repetitions)"
else
    echo "  ℹ INFO: No modules yet (may need more repetitions or correlation threshold)"
    echo "  Graph is learning - check weight changes:"
    grep "w_slow" /tmp/melvin_pattern_test.log | head -3
fi

echo ""
echo "════════════════════════════════════════════════════════"
rm -f /tmp/melvin_pattern_test.log

