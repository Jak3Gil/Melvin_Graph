#!/bin/bash

echo "=========================================="
echo "Testing Accumulative Activation"
echo "=========================================="
echo ""

rm -f graph.mmap

echo "Building pattern network..."
echo "cat sat on a mat" | ./melvin > /dev/null 2>&1
echo "bat sat on a hat" | ./melvin > /dev/null 2>&1
echo "rat sat on a cat" | ./melvin > /dev/null 2>&1
echo ""

echo "Query: 'sat'"
echo "Expected: Words that co-occur with 'sat' get MULTIPLE"
echo "          activation paths and accumulate stronger!"
echo ""
echo "sat" | MELVIN_DEBUG=1 ./melvin
echo ""

echo "=========================================="
echo "What happened?"
echo "=========================================="
echo ""
echo "Without accumulation (old way):"
echo "  Each edge overwrites activation"
echo "  Only strongest single path matters"
echo "  Context ignored"
echo ""
echo "With accumulation (new way):"
echo "  'sat' activates"
echo "  → 'cat' gets activation from 'sat' path"
echo "  → 'cat' ALSO gets activation from 'mat' path (similarity)"
echo "  → 'cat' ALSO gets activation from 'rat' path (similarity)"
echo "  → 'cat' activation ACCUMULATES = very strong!"
echo ""
echo "Patterns with MULTIPLE supporting paths win!"
echo "This is emergent contextual filtering!"
echo ""

