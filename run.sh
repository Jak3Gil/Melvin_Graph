#!/bin/bash
# Simple runner for Melvin

if [ "$1" == "clean" ]; then
    rm -f graph.mmap
    echo "Deleted graph.mmap - fresh start"
fi

if [ "$1" == "build" ]; then
    make clean && make
    exit $?
fi

# Run Melvin with input from stdin or file
./melvin_core

