# Makefile for Melvin - All-in-one melvin_core.c

CC = gcc
CFLAGS = -O2 -Wall -Wextra -std=c99
LDFLAGS = -lm -pthread

# Default: build melvin_core (does everything - bootstrap, runtime, inspect)
all: melvin_core inspect_program melvin melvin_compute

# Main binary (auto-bootstraps if graph.mmap missing, accepts all commands)
melvin_core: melvin_core.c
	$(CC) $(CFLAGS) -o melvin_core melvin_core.c $(LDFLAGS)

# Spreading activation network (simple associative memory)
melvin: melvin.c
	$(CC) $(CFLAGS) -o melvin melvin.c $(LDFLAGS)

# Computational spreading activation (self-arranging computation)
melvin_compute: melvin_compute.c
	$(CC) $(CFLAGS) -o melvin_compute melvin_compute.c $(LDFLAGS)

# Program inspector (views graph as compiled code)
inspect_program: inspect_program.c
	$(CC) $(CFLAGS) -o inspect_program inspect_program.c $(LDFLAGS)

# Emergence system - Pure brain-like learning (information in connections)
melvin_emergence: melvin_emergence.c
	$(CC) $(CFLAGS) -o melvin_emergence melvin_emergence.c $(LDFLAGS)

# Optional: Minimal VM (separate lightweight implementation)
melvin_vm: melvin_vm.c
	$(CC) $(CFLAGS) -o melvin_vm melvin_vm.c $(LDFLAGS)

clean:
	rm -f melvin_vm melvin_core melvin_emergence inspect_program melvin melvin_compute graph.mmap graph_emergence.mmap compute.mmap nodes.bin edges.bin

run: melvin_core
	./melvin_core

debug: CFLAGS = -g -Wall -Wextra -std=c99
debug: melvin_core

.PHONY: all clean run debug

