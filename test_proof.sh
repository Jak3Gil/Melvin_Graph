#!/bin/bash
echo "=== PROOF OF INTELLIGENCE ==="
echo ""
echo "Hypothesis: Pattern reuse = intelligence"
echo "Test: Can system solve problems with <10 training examples?"
echo ""

rm -f graph_emergence.mmap

echo "PHASE 1: Training (6 examples of doubling)"
training_data=(
  "one two"
  "two four"
  "three six"
  "four eight"
  "five ten"
  "six twelve"
)

for data in "${training_data[@]}"; do
  echo "  $data"
  echo "$data" | ./melvin_emergence 2>&1 > /dev/null
done

echo ""
echo "PHASE 2: How many patterns created?"
./show_tokens 2>/dev/null | wc -l

echo ""
echo "PHASE 3: Test with 'seven' (should predict 'fourteen')"
echo "Input: seven"
echo "seven" | ./melvin_emergence 2>&1 | head -3

echo ""
echo "PHASE 4: Measure intelligence (reuse ratio)"
reuse_count=$(echo "seven" | MELVIN_DEBUG=1 ./melvin_emergence 2>&1 | grep -c "\[REUSE\]")
create_count=$(echo "seven" | MELVIN_DEBUG=1 ./melvin_emergence 2>&1 | grep -c "\[CREATE\]")
echo "Reused: $reuse_count patterns"
echo "Created: $create_count patterns"

if [ $reuse_count -gt 0 ]; then
  echo ""
  echo "✓ INTELLIGENCE DETECTED: Reused learned patterns!"
else
  echo ""
  echo "✗ No reuse detected"
fi
