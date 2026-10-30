# Compiler
CC = gcc

# Compiler flags
# -Wall -Wextra: Enable helpful warning messages
# -std=c99: Use the C99 standard
CFLAGS = -Wall -Wextra -std=c99

# Linker flags (Links the SDL2 library)
LDFLAGS = -lSDL2

# Target executable names
TARGET = chip8
COMPILER_TARGET = compiler
DISASM_TARGET = disassembler

# Source files
SRCS = src/main.c src/chip8.c

# Object files (automatically generated from SRCS, e.g., main.o, chip8.o)
OBJS = $(SRCS:.c=.o)

# Default rule (runs when you just type 'make')
all: $(TARGET) $(COMPILER_TARGET) $(DISASM_TARGET)

# Rule to build the assembler/compiler
$(COMPILER_TARGET): compiler.c
	$(CC) $(CFLAGS) compiler.c -o $(COMPILER_TARGET)

# Rule to build the disassembler
$(DISASM_TARGET): disassembler.c
	$(CC) $(CFLAGS) disassembler.c -o $(DISASM_TARGET)

# Rule to link the executable
$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Rule to compile source files into object files
%.o: %.c src/chip8.h
	$(CC) $(CFLAGS) -c $< -o $@

# Clean rule to remove compiled binaries and object files
clean:
	rm -f $(OBJS) $(TARGET) $(COMPILER_TARGET) $(DISASM_TARGET)

# Phony targets (prevents conflicts if files named 'all' or 'clean' exist)
.PHONY: all clean compiler disassembler
