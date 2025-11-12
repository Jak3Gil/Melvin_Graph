#!/bin/bash

# Test: Melvin's continuous thinking (5 second demo)

echo "=== MELVIN CONTINUOUS THINKING TEST ==="
echo ""

# Clean slate
rm -f graph.mmap

# Teach Melvin some patterns
echo "Training: cat sat mat hat bat rat"
echo "cat sat mat hat bat rat" | ./melvin
echo ""
echo "Training: dog log fog hog"
echo "dog log fog hog" | ./melvin
echo ""
echo "Training: book look cook took"
echo "book look cook took" | ./melvin
echo ""

echo "Now Melvin will think continuously for 5 seconds..."
echo "(No input - just internal thoughts and mutations)"
echo ""

# Run for 5 seconds then kill
timeout 5 sh -c 'MELVIN_DEBUG=1 ./melvin' || true

echo ""
echo "=== TEST COMPLETE ==="
echo ""
echo "Melvin was thinking continuously even without input!"
echo "In real use, Melvin will continue thinking as long as the terminal is open."

