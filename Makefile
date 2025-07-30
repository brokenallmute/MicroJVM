# Compiler selection
COMPILER ?= clang
CC = $(COMPILER)

# Compiler flags
CFLAGS = -Wall -Wextra -O2 -std=c99 -g

# Target executable
TARGET = jvm_runner

# Source files
SOURCES = jvm.c class_loader.c native_methods.c main.c

# Default target
all: $(TARGET)

# Build main program
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

# Clean build artifacts
clean:
	rm -f $(TARGET) *.o

.PHONY: all clean