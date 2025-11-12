# Makefile for Melvin Organic - Intelligence from Simple Rules

CC = gcc
CFLAGS = -O2 -Wall -Wextra -std=c99
LDFLAGS = -lm -pthread

# Default: build melvin_organic
all: melvin_organic melvin_computational melvin_pure_compute

# Organic learning (pattern-based, no frequency counting)
melvin_organic: melvin_organic.c
	$(CC) $(CFLAGS) -o melvin_organic melvin_organic.c $(LDFLAGS)

# Computational learning (nodes execute, edges route)
melvin_computational: melvin_computational.c
	$(CC) $(CFLAGS) -o melvin_computational melvin_computational.c $(LDFLAGS)

# Pure graph computation (XOR/AND gates in graph)
melvin_pure_compute: melvin_pure_compute.c
	$(CC) $(CFLAGS) -o melvin_pure_compute melvin_pure_compute.c $(LDFLAGS)

clean:
	rm -f melvin_organic organic.mmap

run: melvin_organic
	./demo_organic.sh

debug: CFLAGS = -g -Wall -Wextra -std=c99
debug: melvin_organic

.PHONY: all clean run debug
