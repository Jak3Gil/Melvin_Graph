# Makefile for Melvin - Unified Intelligence System

CC = gcc
CFLAGS = -O2 -Wall -Wextra -std=c99
LDFLAGS = -lm -pthread

# Default: build unified melvin
all: melvin

# Unified Melvin (organic learning + bitwise computation + meta-learning)
melvin: melvin.c
	$(CC) $(CFLAGS) -o melvin melvin.c $(LDFLAGS)

clean:
	rm -f melvin *.mmap

run: melvin
	./demo.sh

test: melvin
	./test_all.sh

.PHONY: all clean run test
