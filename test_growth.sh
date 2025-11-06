#!/bin/bash
rm -f graph.mmap
./bootstrap_graph > /dev/null

echo "Initial nodes: $(./inspect_graph | grep 'Nodes:' | awk '{print $2}')"

# Feed short input with MANY ticks
(echo "hello world test cat dog" | ./melvin_vm 2>&1) &
PID=$!
sleep 10  # 200 ticks at 50ms each
kill $PID 2>/dev/null || true
wait $PID 2>/dev/null || true

echo "After 10 sec: $(./inspect_graph | grep 'Nodes:' | awk '{print $2}')"
echo "Ticks run:    $(./inspect_graph | grep 'Tick:' | awk '{print $2}')"
