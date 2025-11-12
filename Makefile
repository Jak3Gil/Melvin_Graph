# Makefile for Melvin Organic - Intelligence from Simple Rules

CC = gcc
CFLAGS = -O2 -Wall -Wextra -std=c99
LDFLAGS = -lm -pthread

# Default: build melvin_organic
all: melvin_organic

# Organic learning (pattern-based, no frequency counting)
melvin_organic: melvin_organic.c
	$(CC) $(CFLAGS) -o melvin_organic melvin_organic.c $(LDFLAGS)

clean:
	rm -f melvin_organic organic.mmap

run: melvin_organic
	./demo_organic.sh

debug: CFLAGS = -g -Wall -Wextra -std=c99
debug: melvin_organic

.PHONY: all clean run debug
