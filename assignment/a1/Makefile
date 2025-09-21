# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g

# Target executable name
TARGET = dragonshell

# Source file (adjust if your file has a different name)
SOURCES = dragonshell.c

# Default target - this runs when you just type 'make'
all: $(TARGET)

# Rule to build the executable
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

# Clean up compiled files
clean:
	rm -f $(TARGET)

# Phony targets (targets that don't create files)
.PHONY: all clean
