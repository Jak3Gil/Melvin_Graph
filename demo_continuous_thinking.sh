#!/bin/bash

# Demo: Melvin's continuous thinking
# Melvin will keep thinking as long as the terminal is open!

echo "=== MELVIN CONTINUOUS THINKING DEMO ==="
echo ""
echo "First, let's give Melvin some initial knowledge..."
echo ""

# Clean slate
rm -f graph.mmap

# Teach Melvin some patterns
echo "cat sat mat hat bat rat" | ./melvin
echo "dog log fog hog jog" | ./melvin
echo "book look cook took" | ./melvin

echo ""
echo "Now watch Melvin think continuously..."
echo "He will self-prompt, mutate, and explore even without input."
echo "(Press Ctrl+C to stop)"
echo ""
echo "=== START CONTINUOUS THINKING (DEBUG MODE) ==="
echo ""

# Run with debug to see internal thoughts
# This will run FOREVER until you press Ctrl+C
MELVIN_DEBUG=1 ./melvin

