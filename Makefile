# Makefile for Melvin - Unified Intelligence System

CC = gcc
CFLAGS = -O2 -Wall -Wextra -std=c99
LDFLAGS = -lm -pthread

# Default: build both versions
all: melvin melvin_self

# Unified Melvin (organic learning + bitwise computation + meta-learning)
melvin: melvin.c
	$(CC) $(CFLAGS) -o melvin melvin.c $(LDFLAGS)

# Self-Modifying Melvin (graph operates on itself)
melvin_self: melvin_self.c
	$(CC) $(CFLAGS) -o melvin_self melvin_self.c $(LDFLAGS)

clean:
	rm -f melvin melvin_self *.mmap

run: melvin
	./demo.sh

run_self: melvin_self
	./demo_self.sh

test: melvin
	./test_all.sh

.PHONY: all clean run run_self test
