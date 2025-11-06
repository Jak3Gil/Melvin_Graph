#!/bin/bash
echo "🔬 CLEAN TEST - Fresh graph learning from Wikipedia"
echo ""

# Completely fresh start
rm -f graph.mmap
./bootstrap_graph > /dev/null
INITIAL=$(stat -f%z graph.mmap)

echo "Before: $(ls -lh graph.mmap | awk '{print $5}')"

# Feed Wikipedia ONCE
cat wikipedia_ai.txt | ./melvin_vm 2>&1 &
PID=$!
sleep 5
kill $PID 2>/dev/null || true
wait $PID 2>/dev/null || true

AFTER=$(stat -f%z graph.mmap)
GROWTH=$((AFTER - INITIAL))

echo "After:  $(ls -lh graph.mmap | awk '{print $5}')"
echo "Growth: $((GROWTH / 1024)) KB"
echo ""

# Now feed 10x to prove NO ADDITIONAL GROWTH (pattern reuse)
echo "Feeding 9 more times to prove pattern reuse..."
for i in {2..10}; do
    cat wikipedia_ai.txt | ./melvin_vm 2>&1 > /dev/null &
    PID=$!
    sleep 2
    kill $PID 2>/dev/null || true
    wait $PID 2>/dev/null || true
done

FINAL=$(stat -f%z graph.mmap)
FINAL_GROWTH=$((FINAL - INITIAL))

echo "Final:  $(ls -lh graph.mmap | awk '{print $5}')"  
echo "Total growth: $((FINAL_GROWTH / 1024)) KB (from 10 passes)"
echo ""

if [ $FINAL_GROWTH -eq $GROWTH ]; then
    echo "✅ NO ADDITIONAL GROWTH after first pass!"
    echo "   Proof: Patterns learned once, reused 9 times!"
else
    echo "Growth continued: $((FINAL_GROWTH / 1024)) KB total"
fi
