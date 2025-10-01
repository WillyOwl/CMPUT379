# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g

# Target executable name
TARGET = dragonshell

# Source files
SOURCES = dragonshell.c

# Object files (generated from source files)
OBJECTS = $(SOURCES:.c=.o)

# Default target
all: $(TARGET)

# Main target dragonshell - links all object files to produce executable
dragonshell: $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS)

# Target compile - compiles code and produces object file(s)
compile: $(OBJECTS)

# Rule to compile .c files to .o files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Target clean - removes object file(s) and executable file(s)
clean:
	rm -f $(OBJECTS) $(TARGET)

# Phony targets (targets that don't create files)
.PHONY: all dragonshell compile clean
