#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Translates a line of assembly text into a 16-bit binary opcode
uint16_t assemble_line(char *line) {
    int reg;
    int val;
    int addr;

    // Rule A: Match "cls" -> Clear Screen (Opcode: 0x00E0)
    if (strcmp(line, "cls") == 0) {
        return 0x00E0;
    }

    // Rule B: Match "vX = NN" -> Set Register VX to value NN (Opcode: 6XNN)
    if (sscanf(line, "v%d = %d", &reg, &val) == 2) {
        if (reg > 15 || val > 255) {
            fprintf(stderr, "Error: Register or value out of bounds!\n");
            exit(1);
        }
        return 0x6000 | (reg << 8) | val;
    }

    // Rule C: Match "jump NNN" -> Jump to memory address NNN (Opcode: 1NNN)
    if (sscanf(line, "jump 0x%x", &addr) == 1) {
        if (addr > 0xFFF) {
            fprintf(stderr, "Error: Memory address out of bounds!\n");
            exit(1);
        }
        return 0x1000 | (addr & 0x0FFF);
    }

    // If text line doesn't match any known instruction pattern
    fprintf(stderr, "Syntax Error: Unknown instruction '%s'\n", line);
    exit(1);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: %s <source.txt> <output.ch8>\n", argv[0]);
        return 1;
    }

    FILE *source = fopen(argv[1], "r");
    FILE *output = fopen(argv[2], "wb");

    if (!source || !output) {
        printf("Error: Could not open source or destination files.\n");
        return 1;
    }

    char line[256];
    printf("Compiling source program...\n");

    while (fgets(line, sizeof(line), source)) {
        // Strip trailing newline character if present
        line[strcspn(line, "\r\n")] = 0;

        // Skip blank lines
        if (strlen(line) == 0) continue;

        // Process line and generate opcode
        uint16_t opcode = assemble_line(line);
        printf("Text: [%s] -> Opcode generated: 0x%04X\n", line, opcode);

        // Convert the 16-bit opcode into 2 sequential big-endian bytes
        uint8_t bytes[2];
        bytes[0] = (opcode >> 8) & 0xFF; // High byte
        bytes[1] = opcode & 0xFF;        // Low byte

        // Write the machine code directly into our binary file
        fwrite(bytes, sizeof(uint8_t), 2, output);
    }

    printf("Compilation completed successfully! Created ROM: %s\n", argv[2]);

    fclose(source);
    fclose(output);
    return 0;
}
