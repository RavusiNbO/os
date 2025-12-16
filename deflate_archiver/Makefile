CC = gcc
CFLAGS = -Wall -Wextra -std=c11 -O2
TARGET = deflate_archiver
SOURCES = main.c archiver.c deflate.c huffman.c lz77.c bitops.c
OBJECTS = $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

%.o: %.c deflate.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean

