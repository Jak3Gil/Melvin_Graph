# Makefile for Melvin - All-in-one melvin_core.c

CC = gcc
CFLAGS = -O2 -Wall -Wextra -std=c99
LDFLAGS = -lm -pthread

# Default: build all modes
all: melvin_core bootstrap_graph inspect_graph

# Runtime mode (default - no flags)
melvin_core: melvin_core.c
	$(CC) $(CFLAGS) -o melvin_core melvin_core.c $(LDFLAGS)

# Bootstrap mode (creates graph.mmap)
bootstrap_graph: melvin_core.c
	$(CC) $(CFLAGS) -DBOOTSTRAP_MODE -o bootstrap_graph melvin_core.c $(LDFLAGS)

# Inspect mode (reads graph.mmap)
inspect_graph: melvin_core.c
	$(CC) $(CFLAGS) -DINSPECT_MODE -o inspect_graph melvin_core.c $(LDFLAGS)

# Optional: Minimal VM (separate lightweight implementation)
melvin_vm: melvin_vm.c
	$(CC) $(CFLAGS) -o melvin_vm melvin_vm.c $(LDFLAGS)

clean:
	rm -f melvin_vm melvin_core bootstrap_graph inspect_graph graph.mmap nodes.bin edges.bin

run: melvin_core
	./melvin_core

debug: CFLAGS = -g -Wall -Wextra -std=c99
debug: melvin_core

.PHONY: all clean run debug

