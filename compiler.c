#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <ctype.h>
#include <stdbool.h>

#define MAX_LABELS 256
#define MAX_LABEL_LEN 64
#define MAX_LINE_LEN 256
#define START_ADDRESS 0x200
#define MAX_ADDRESS 0x0FFF

typedef struct {
    char name[MAX_LABEL_LEN];
    uint16_t address;
    int defined_line;
} Label;

typedef struct {
    Label items[MAX_LABELS];
    int count;
} LabelTable;

// Helper: Trim leading and trailing whitespace
static char *trim_whitespace(char *str) {
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;

    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

// Helper: Strip comments (starting with ;, #, or //)
static void strip_comments(char *line) {
    char *comment = NULL;
    char *c1 = strchr(line, ';');
    char *c2 = strchr(line, '#');
    char *c3 = strstr(line, "//");

    if (c1) comment = c1;
    if (c2 && (!comment || c2 < comment)) comment = c2;
    if (c3 && (!comment || c3 < comment)) comment = c3;

    if (comment) {
        *comment = '\0';
    }
}

// Find label by name; returns index or -1 if not found
static int find_label(const LabelTable *table, const char *name) {
    for (int i = 0; i < table->count; i++) {
        if (strcmp(table->items[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

// Add a label to the symbol table
static bool add_label(LabelTable *table, const char *name, uint16_t address, int line_num) {
    if (strlen(name) == 0) {
        fprintf(stderr, "Error at line %d: Empty label name.\n", line_num);
        return false;
    }
    if (strlen(name) >= MAX_LABEL_LEN) {
        fprintf(stderr, "Error at line %d: Label name '%s' exceeds max length of %d.\n",
                line_num, name, MAX_LABEL_LEN - 1);
        return false;
    }
    // Check validity of label characters (must start with alpha/underscore)
    if (!isalpha((unsigned char)name[0]) && name[0] != '_') {
        fprintf(stderr, "Error at line %d: Label '%s' must start with a letter or underscore.\n",
                line_num, name);
        return false;
    }
    for (size_t i = 1; i < strlen(name); i++) {
        if (!isalnum((unsigned char)name[i]) && name[i] != '_') {
            fprintf(stderr, "Error at line %d: Label '%s' contains invalid character '%c'.\n",
                    line_num, name, name[i]);
            return false;
        }
    }

    if (find_label(table, name) != -1) {
        fprintf(stderr, "Error at line %d: Label '%s' already defined.\n", line_num, name);
        return false;
    }

    if (table->count >= MAX_LABELS) {
        fprintf(stderr, "Error at line %d: Maximum label count (%d) exceeded.\n",
                line_num, MAX_LABELS);
        return false;
    }

    strncpy(table->items[table->count].name, name, MAX_LABEL_LEN - 1);
    table->items[table->count].name[MAX_LABEL_LEN - 1] = '\0';
    table->items[table->count].address = address;
    table->items[table->count].defined_line = line_num;
    table->count++;
    return true;
}

// Resolves a target string (e.g. "loop", "0x200", "512") into a 12-bit address
static bool resolve_address(const char *target, const LabelTable *table, uint16_t *out_addr, int line_num) {
    char *endptr = NULL;
    long val = -1;

    // Check if target is a numeric hex address ("0x..." or "0X...")
    if (strncasecmp(target, "0x", 2) == 0) {
        val = strtol(target, &endptr, 16);
        if (*endptr == '\0') {
            if (val < 0 || val > MAX_ADDRESS) {
                fprintf(stderr, "Error at line %d: Memory address 0x%lX out of bounds (0x000-0x%03X).\n",
                        line_num, val, MAX_ADDRESS);
                return false;
            }
            *out_addr = (uint16_t)val;
            return true;
        }
    }

    // Check if target is decimal numeric
    val = strtol(target, &endptr, 10);
    if (endptr != target && *endptr == '\0') {
        if (val < 0 || val > MAX_ADDRESS) {
            fprintf(stderr, "Error at line %d: Memory address %ld out of bounds (0-4095).\n",
                    line_num, val);
            return false;
        }
        *out_addr = (uint16_t)val;
        return true;
    }

    // Look up in label table
    int idx = find_label(table, target);
    if (idx != -1) {
        *out_addr = table->items[idx].address;
        return true;
    }

    fprintf(stderr, "Error at line %d: Undefined label or invalid address '%s'.\n", line_num, target);
    return false;
}

// Parses register name like "v0"..."v15" or "va"..."vf"
static int parse_register(const char *reg_str) {
    if (reg_str[0] != 'v' && reg_str[0] != 'V') return -1;

    char *endptr = NULL;
    // Try hex (vA-vF or v0-v9)
    long r = strtol(reg_str + 1, &endptr, 16);
    if (endptr != reg_str + 1 && *endptr == '\0' && r >= 0 && r <= 15) {
        return (int)r;
    }

    // Try decimal (v0-v15)
    r = strtol(reg_str + 1, &endptr, 10);
    if (endptr != reg_str + 1 && *endptr == '\0' && r >= 0 && r <= 15) {
        return (int)r;
    }

    return -1;
}

// Parses an instruction line and returns a 16-bit opcode (pass 2)
static bool assemble_line(char *line, const LabelTable *table, uint16_t *opcode, int line_num) {
    line = trim_whitespace(line);

    // Rule A: Match "cls" -> Clear Screen (Opcode: 0x00E0)
    if (strcasecmp(line, "cls") == 0) {
        *opcode = 0x00E0;
        return true;
    }

    // Rule B: Match "vX = NN" -> Set Register VX to value NN (Opcode: 6XNN)
    char reg_buf[32];
    char val_buf[64];
    if (sscanf(line, "%31s = %63s", reg_buf, val_buf) == 2) {
        int reg = parse_register(reg_buf);
        if (reg != -1) {
            char *endptr = NULL;
            long val = strtol(val_buf, &endptr, 0); // handles 0x hex and decimal
            if (*endptr == '\0') {
                if (val < 0 || val > 255) {
                    fprintf(stderr, "Error at line %d: Value %ld out of bounds (0-255) for %s!\n",
                            line_num, val, reg_buf);
                    return false;
                }
                *opcode = 0x6000 | (reg << 8) | ((uint8_t)val & 0xFF);
                return true;
            }
        }
    }

    // Rule C: Match "jump <target>" -> Jump to memory address or label (Opcode: 1NNN)
    char target[MAX_LABEL_LEN];
    if (sscanf(line, "jump %63s", target) == 1) {
        uint16_t addr = 0;
        if (!resolve_address(target, table, &addr, line_num)) {
            return false;
        }
        *opcode = 0x1000 | (addr & 0x0FFF);
        return true;
    }

    // If text line doesn't match any known instruction pattern
    fprintf(stderr, "Syntax Error at line %d: Unknown instruction '%s'\n", line_num, line);
    return false;
}

// Splits a line into an optional label and instruction
// Returns pointer to instruction string (may be empty if line was only a label)
static char *extract_label(char *line, char *label_buf, size_t label_buf_size) {
    label_buf[0] = '\0';
    char *colon = strchr(line, ':');
    if (!colon) {
        return trim_whitespace(line);
    }

    // Found colon: text before it is label, text after is instruction
    *colon = '\0';
    char *label_part = trim_whitespace(line);
    strncpy(label_buf, label_part, label_buf_size - 1);
    label_buf[label_buf_size - 1] = '\0';

    return trim_whitespace(colon + 1);
}

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("Usage: %s <source.txt> <output.ch8>\n", argv[0]);
        return 1;
    }

    FILE *source = fopen(argv[1], "r");
    if (!source) {
        fprintf(stderr, "Error: Could not open source file '%s'.\n", argv[1]);
        return 1;
    }

    LabelTable table;
    table.count = 0;

    char line_buf[MAX_LINE_LEN];
    char raw_line[MAX_LINE_LEN];
    char label[MAX_LABEL_LEN];

    printf("=======================================\n");
    printf("Compiling source: %s\n", argv[1]);
    printf("=======================================\n");

    // ==========================================
    // PASS 1: Symbol resolution & address sizing
    // ==========================================
    printf("[Pass 1] Scanning labels and computing addresses...\n");
    uint16_t current_address = START_ADDRESS;
    int line_num = 0;

    while (fgets(line_buf, sizeof(line_buf), source)) {
        line_num++;
        strip_comments(line_buf);

        char *instr = extract_label(line_buf, label, sizeof(label));

        // If there was a label definition on this line
        if (strlen(label) > 0) {
            if (!add_label(&table, label, current_address, line_num)) {
                fclose(source);
                return 1;
            }
            printf("  Defined label '%s' at 0x%03X (line %d)\n", label, current_address, line_num);
        }

        // If there's an instruction following or standalone
        if (strlen(instr) > 0) {
            if (current_address + 2 > MAX_ADDRESS) {
                fprintf(stderr, "Error at line %d: Program size exceeded CHIP-8 memory limit (0x%03X).\n",
                        line_num, MAX_ADDRESS);
                fclose(source);
                return 1;
            }
            current_address += 2; // Each CHIP-8 instruction is 2 bytes
        }
    }

    printf("[Pass 1] Complete. Found %d label(s). Code size: %d bytes.\n\n",
           table.count, current_address - START_ADDRESS);

    // ==========================================
    // PASS 2: Assemble instructions into opcodes
    // ==========================================
    printf("[Pass 2] Generating machine code...\n");
    rewind(source);

    FILE *output = fopen(argv[2], "wb");
    if (!output) {
        fprintf(stderr, "Error: Could not open output file '%s'.\n", argv[2]);
        fclose(source);
        return 1;
    }

    line_num = 0;
    current_address = START_ADDRESS;
    bool has_error = false;

    while (fgets(line_buf, sizeof(line_buf), source)) {
        line_num++;
        // Keep a copy of cleaned line for logging
        strncpy(raw_line, line_buf, sizeof(raw_line) - 1);
        raw_line[sizeof(raw_line) - 1] = '\0';
        strip_comments(raw_line);

        strip_comments(line_buf);
        char *instr = extract_label(line_buf, label, sizeof(label));

        // If no instruction on this line, skip code generation
        if (strlen(instr) == 0) continue;

        uint16_t opcode = 0;
        if (!assemble_line(instr, &table, &opcode, line_num)) {
            has_error = true;
            break;
        }

        printf("  [0x%03X] %-20s -> Opcode: 0x%04X\n", current_address, trim_whitespace(raw_line), opcode);

        // Convert the 16-bit opcode into 2 sequential big-endian bytes
        uint8_t bytes[2];
        bytes[0] = (opcode >> 8) & 0xFF; // High byte
        bytes[1] = opcode & 0xFF;        // Low byte

        if (fwrite(bytes, sizeof(uint8_t), 2, output) != 2) {
            fprintf(stderr, "Error writing opcode to output file.\n");
            has_error = true;
            break;
        }

        current_address += 2;
    }

    fclose(source);
    fclose(output);

    if (has_error) {
        remove(argv[2]); // Delete incomplete/corrupt binary
        printf("\nCompilation FAILED due to errors.\n");
        return 1;
    }

    printf("\nCompilation SUCCESSFUL! Created ROM: %s (%d bytes)\n",
           argv[2], current_address - START_ADDRESS);
    printf("=======================================\n");

    return 0;
}
