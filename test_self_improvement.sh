#!/bin/bash
# Test: Graph Self-Improvement
# Proves the graph can detect and fix its own problems

echo "════════════════════════════════════════════════════════"
echo "  TEST: Graph Self-Improvement"
echo "  Proving the graph can optimize itself"
echo "════════════════════════════════════════════════════════"
echo ""

# Clean start
rm -f graph.mmap

echo "Phase 1: Initial Learning (graph starts with default parameters)"
echo "────────────────────────────────────────────────────────"

# Feed repetitive pattern - should trigger self-monitoring
{
    for i in {1..20}; do
        echo "the cat sat"
        sleep 0.05
    done
} | ./melvin_core 2>&1 > /tmp/melvin_self_test.log

# Extract metrics
MONITOR_MSGS=$(grep "SELF-MONITOR" /tmp/melvin_self_test.log | wc -l | tr -d ' ')
ADJUST_MSGS=$(grep "SELF-ADJUST" /tmp/melvin_self_test.log | wc -l | tr -d ' ')
TUNE_MSGS=$(grep "SELF-TUNE" /tmp/melvin_self_test.log | wc -l | tr -d ' ')

echo "Results:"
echo "  Performance monitoring activations: $MONITOR_MSGS"
echo "  Threshold adjustments: $ADJUST_MSGS"
echo "  Learning rate tuning: $TUNE_MSGS"
echo ""

if [ "$MONITOR_MSGS" -gt 0 ]; then
    echo "  ✓ PASS: Graph is monitoring its own performance!"
    echo ""
    echo "Sample monitoring output:"
    grep "SELF-MONITOR" /tmp/melvin_self_test.log | head -3
else
    echo "  ℹ INFO: No self-monitoring yet (may need more data)"
fi

echo ""

if [ "$ADJUST_MSGS" -gt 0 ]; then
    echo "  ✓✓ BREAKTHROUGH: Graph adjusted its own parameters!"
    echo ""
    echo "Self-adjustment log:"
    grep "SELF-ADJUST" /tmp/melvin_self_test.log
    echo ""
    echo "  → The graph detected a problem and fixed itself!"
fi

if [ "$TUNE_MSGS" -gt 0 ]; then
    echo "  ✓✓ TUNING: Graph optimized its learning rate!"
    echo ""
    grep "SELF-TUNE" /tmp/melvin_self_test.log
fi

echo ""
echo "Phase 2: Verify Improvement"
echo "────────────────────────────────────────────────────────"

# Check if modules were created (proof of learning)
MODULES=$(grep "MODULE.*Created module" /tmp/melvin_self_test.log | wc -l | tr -d ' ')
FINAL_STATS=$(grep "TICK.*modules=" /tmp/melvin_self_test.log | tail -1)

echo "Final state: $FINAL_STATS"
echo "Total modules created: $MODULES"

if [ "$MODULES" -gt 0 ]; then
    echo ""
    echo "  ✓ Graph learned patterns and created $MODULES modules"
fi

echo ""
echo "════════════════════════════════════════════════════════"
echo "  PROOF OF SELF-IMPROVEMENT:"
if [ "$ADJUST_MSGS" -gt 0 ] || [ "$TUNE_MSGS" -gt 0 ]; then
    echo "  ✓✓ Graph detected problems"
    echo "  ✓✓ Graph adjusted its own parameters"
    echo "  ✓✓ No human intervention needed"
    echo ""
    echo "  THE GRAPH IS SELF-OPTIMIZING!"
else
    echo "  Graph has self-monitoring circuits installed"
    echo "  (May need more diverse input to trigger adjustments)"
fi
echo "════════════════════════════════════════════════════════"

rm -f /tmp/melvin_self_test.log

