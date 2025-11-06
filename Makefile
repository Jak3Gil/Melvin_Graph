# Makefile for Melvin - Ultra-minimal VM + Legacy core

CC = gcc
CFLAGS = -O2 -Wall -Wextra -std=c99
LDFLAGS = -lm -pthread

# Default: build minimal VM (93% smaller!)
all: melvin_vm bootstrap_graph

# Minimal VM (286 lines, 34KB binary)
melvin_vm: melvin_vm.c
	$(CC) $(CFLAGS) -o melvin_vm melvin_vm.c $(LDFLAGS)

# Bootstrap compiler (creates graph.mmap)
bootstrap_graph: bootstrap_graph.c
	$(CC) $(CFLAGS) -o bootstrap_graph bootstrap_graph.c $(LDFLAGS)

# Legacy full implementation (4110 lines, 89KB binary)
melvin_core: melvin_core.c
	$(CC) $(CFLAGS) -o melvin_core melvin_core.c $(LDFLAGS)

clean:
	rm -f melvin_vm melvin_core bootstrap_graph graph.mmap nodes.bin edges.bin

run: melvin_vm
	./melvin_vm

debug: CFLAGS = -g -Wall -Wextra -std=c99
debug: $(TARGET)

.PHONY: all clean run debug

