#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

#define START_ADDRESS 0x200
#define MAX_ROM_SIZE (4096 - START_ADDRESS)

static void disassemble_opcode(uint16_t addr, uint16_t op, FILE *out) {
    uint8_t n1 = (op & 0xF000) >> 12;
    uint8_t x  = (op & 0x0F00) >> 8;
    uint8_t y  = (op & 0x00F0) >> 4;
    uint8_t n  =  op & 0x000F;
    uint8_t kk =  op & 0x00FF;
    uint16_t nnn = op & 0x0FFF;

    fprintf(out, "/* 0x%03X: 0x%04X */  ", addr, op);

    switch (n1) {
    case 0x0:
        if (op == 0x00E0) {
            fprintf(out, "cls\n");
        } else if (op == 0x00EE) {
            fprintf(out, "ret\n");
        } else {
            fprintf(out, "db 0x%02X, 0x%02X  ; unknown 0x0000 syscall\n", (op >> 8) & 0xFF, op & 0xFF);
        }
        break;

    case 0x1:
        fprintf(out, "jump 0x%03X\n", nnn);
        break;

    case 0x2:
        fprintf(out, "call 0x%03X\n", nnn);
        break;

    case 0x3:
        fprintf(out, "if v%X == 0x%02X\n", x, kk);
        break;

    case 0x4:
        fprintf(out, "if v%X != 0x%02X\n", x, kk);
        break;

    case 0x5:
        if (n == 0) {
            fprintf(out, "if v%X == v%X\n", x, y);
        } else {
            fprintf(out, "db 0x%02X, 0x%02X  ; invalid 0x5 opcode\n", (op >> 8) & 0xFF, op & 0xFF);
        }
        break;

    case 0x6:
        fprintf(out, "v%X = 0x%02X\n", x, kk);
        break;

    case 0x7:
        fprintf(out, "v%X += 0x%02X\n", x, kk);
        break;

    case 0x8:
        switch (n) {
        case 0x0:
            fprintf(out, "v%X = v%X\n", x, y);
            break;
        case 0x1:
            fprintf(out, "v%X |= v%X\n", x, y);
            break;
        case 0x2:
            fprintf(out, "v%X &= v%X\n", x, y);
            break;
        case 0x3:
            fprintf(out, "v%X ^= v%X\n", x, y);
            break;
        case 0x4:
            fprintf(out, "v%X += v%X\n", x, y);
            break;
        case 0x5:
            fprintf(out, "v%X -= v%X\n", x, y);
            break;
        case 0x6:
            fprintf(out, "v%X >>= 1\n", x);
            break;
        case 0x7:
            fprintf(out, "v%X = v%X - v%X\n", x, y, x);
            break;
        case 0xE:
            fprintf(out, "v%X <<= 1\n", x);
            break;
        default:
            fprintf(out, "db 0x%02X, 0x%02X  ; unknown 0x8 opcode\n", (op >> 8) & 0xFF, op & 0xFF);
            break;
        }
        break;

    case 0x9:
        if (n == 0) {
            fprintf(out, "if v%X != v%X\n", x, y);
        } else {
            fprintf(out, "db 0x%02X, 0x%02X  ; invalid 0x9 opcode\n", (op >> 8) & 0xFF, op & 0xFF);
        }
        break;

    case 0xA:
        fprintf(out, "i = 0x%03X\n", nnn);
        break;

    case 0xB:
        fprintf(out, "jump v0 + 0x%03X\n", nnn);
        break;

    case 0xC:
        fprintf(out, "v%X = rand 0x%02X\n", x, kk);
        break;

    case 0xD:
        fprintf(out, "draw v%X, v%X, %d\n", x, y, n);
        break;

    case 0xE:
        if (kk == 0x9E) {
            fprintf(out, "skip_key v%X\n", x);
        } else if (kk == 0xA1) {
            fprintf(out, "skip_nkey v%X\n", x);
        } else {
            fprintf(out, "db 0x%02X, 0x%02X  ; unknown 0xE opcode\n", (op >> 8) & 0xFF, op & 0xFF);
        }
        break;

    case 0xF:
        switch (kk) {
        case 0x07:
            fprintf(out, "v%X = delay\n", x);
            break;
        case 0x0A:
            fprintf(out, "v%X = key\n", x);
            break;
        case 0x15:
            fprintf(out, "delay = v%X\n", x);
            break;
        case 0x18:
            fprintf(out, "sound = v%X\n", x);
            break;
        case 0x1E:
            fprintf(out, "i += v%X\n", x);
            break;
        case 0x29:
            fprintf(out, "i = font v%X\n", x);
            break;
        case 0x33:
            fprintf(out, "bcd v%X\n", x);
            break;
        case 0x55:
            fprintf(out, "save v%X\n", x);
            break;
        case 0x65:
            fprintf(out, "load v%X\n", x);
            break;
        default:
            fprintf(out, "db 0x%02X, 0x%02X  ; unknown 0xF opcode\n", (op >> 8) & 0xFF, op & 0xFF);
            break;
        }
        break;

    default:
        fprintf(out, "db 0x%02X, 0x%02X\n", (op >> 8) & 0xFF, op & 0xFF);
        break;
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <input.ch8> [output.txt]\n", argv[0]);
        return 1;
    }

    FILE *file = fopen(argv[1], "rb");
    if (!file) {
        fprintf(stderr, "Error: Could not open ROM file '%s'.\n", argv[1]);
        return 1;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    if (file_size <= 0) {
        fprintf(stderr, "Error: ROM file is empty.\n");
        fclose(file);
        return 1;
    }

    if (file_size > MAX_ROM_SIZE) {
        fprintf(stderr, "Error: ROM size (%ld bytes) exceeds maximum CHIP-8 capacity (%d bytes).\n",
                file_size, MAX_ROM_SIZE);
        fclose(file);
        return 1;
    }

    uint8_t *buffer = (uint8_t *)malloc(file_size);
    if (!buffer) {
        fprintf(stderr, "Error: Memory allocation failed.\n");
        fclose(file);
        return 1;
    }

    size_t read_bytes = fread(buffer, 1, file_size, file);
    fclose(file);

    if (read_bytes != (size_t)file_size) {
        fprintf(stderr, "Error: Could not read entire ROM file.\n");
        free(buffer);
        return 1;
    }

    FILE *out = stdout;
    if (argc >= 3) {
        out = fopen(argv[2], "w");
        if (!out) {
            fprintf(stderr, "Error: Could not create output file '%s'.\n", argv[2]);
            free(buffer);
            return 1;
        }
    }

    fprintf(out, "; ==========================================\n");
    fprintf(out, "; Byte8 Disassembly of: %s (%ld bytes)\n", argv[1], file_size);
    fprintf(out, "; ==========================================\n\n");

    for (size_t i = 0; i < read_bytes; i += 2) {
        uint16_t current_addr = START_ADDRESS + (uint16_t)i;
        if (i + 1 < read_bytes) {
            uint16_t op = (buffer[i] << 8) | buffer[i + 1];
            disassemble_opcode(current_addr, op, out);
        } else {
            // Trailing odd byte
            fprintf(out, "/* 0x%03X: 0x%02X   */  db 0x%02X\n", current_addr, buffer[i], buffer[i]);
        }
    }

    if (out != stdout) {
        fclose(out);
        printf("Disassembly written to: %s\n", argv[2]);
    }

    free(buffer);
    return 0;
}
