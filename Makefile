# Makefile for melvin_core

CC = gcc
CFLAGS = -O2 -Wall -Wextra -std=c99
LDFLAGS = -lm

TARGET = melvin_core
SRC = melvin_core.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET) nodes.bin edges.bin

run: $(TARGET)
	./$(TARGET)

debug: CFLAGS = -g -Wall -Wextra -std=c99
debug: $(TARGET)

.PHONY: all clean run debug

