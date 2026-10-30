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
#define MAX_BYTES_PER_LINE 32
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

// Parse integer with support for decimal, hex (0x...), and binary (0b...)
static bool parse_number(const char *str, long *out_val) {
    char *trimmed = trim_whitespace((char *)str);
    if (!trimmed || *trimmed == '\0') return false;

    // Binary literal (0b... or 0B...)
    if ((strncasecmp(trimmed, "0b", 2) == 0) && strlen(trimmed) > 2) {
        char *endptr = NULL;
        long val = strtol(trimmed + 2, &endptr, 2);
        if (*endptr == '\0') {
            *out_val = val;
            return true;
        }
        return false;
    }

    // Hex or decimal (strtol with base 0 automatically handles 0x for hex)
    char *endptr = NULL;
    long val = strtol(trimmed, &endptr, 0);
    if (endptr != trimmed && *endptr == '\0') {
        *out_val = val;
        return true;
    }

    return false;
}

// Resolves a target string (e.g. "loop", "0x200", "512") into a 12-bit address
static bool resolve_address(const char *target, const LabelTable *table, uint16_t *out_addr, int line_num) {
    long num;
    if (parse_number(target, &num)) {
        if (num < 0 || num > MAX_ADDRESS) {
            fprintf(stderr, "Error at line %d: Memory address 0x%lX out of bounds (0x000-0x%03X).\n",
                    line_num, num, MAX_ADDRESS);
            return false;
        }
        *out_addr = (uint16_t)num;
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
    long r = strtol(reg_str + 1, &endptr, 16);
    if (endptr != reg_str + 1 && *endptr == '\0' && r >= 0 && r <= 15) {
        return (int)r;
    }

    r = strtol(reg_str + 1, &endptr, 10);
    if (endptr != reg_str + 1 && *endptr == '\0' && r >= 0 && r <= 15) {
        return (int)r;
    }

    return -1;
}

// Checks if a line is a data byte directive ("db" or "byte") and counts bytes
static int get_data_byte_count(const char *instr, int line_num) {
    char copy[MAX_LINE_LEN];
    strncpy(copy, instr, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';

    char *tok = strtok(copy, " \t");
    if (!tok) return 0;

    if (strcasecmp(tok, "db") != 0 && strcasecmp(tok, "byte") != 0) {
        return -1; // Not a db directive
    }

    int count = 0;
    while ((tok = strtok(NULL, " ,\t")) != NULL) {
        tok = trim_whitespace(tok);
        if (*tok == '\0') continue;
        long val;
        if (!parse_number(tok, &val) || val < 0 || val > 255) {
            fprintf(stderr, "Error at line %d: Invalid byte value '%s' in db directive.\n", line_num, tok);
            return -2; // Parsing error
        }
        count++;
    }

    if (count == 0) {
        fprintf(stderr, "Error at line %d: 'db' directive requires at least one byte value.\n", line_num);
        return -2;
    }

    return count;
}

// Parses an instruction or directive into machine bytes (1 to MAX_BYTES_PER_LINE bytes)
// Returns number of bytes generated (> 0), or -1 on error.
static int assemble_line(char *line, const LabelTable *table, uint8_t *out_bytes, int line_num) {
    line = trim_whitespace(line);

    // ----------------------------------------------------
    // Directives: db / byte (raw data)
    // ----------------------------------------------------
    if (strncasecmp(line, "db ", 3) == 0 || strncasecmp(line, "byte ", 5) == 0 ||
        strcasecmp(line, "db") == 0 || strcasecmp(line, "byte") == 0) {
        char copy[MAX_LINE_LEN];
        strncpy(copy, line, sizeof(copy) - 1);
        copy[sizeof(copy) - 1] = '\0';

        char *tok = strtok(copy, " \t");
        int count = 0;
        while ((tok = strtok(NULL, " ,\t")) != NULL) {
            tok = trim_whitespace(tok);
            if (*tok == '\0') continue;
            long val;
            if (!parse_number(tok, &val) || val < 0 || val > 255) {
                fprintf(stderr, "Error at line %d: Invalid byte value '%s'.\n", line_num, tok);
                return -1;
            }
            out_bytes[count++] = (uint8_t)val;
        }
        if (count == 0) {
            fprintf(stderr, "Error at line %d: Empty db directive.\n", line_num);
            return -1;
        }
        return count;
    }

    uint16_t op = 0;

    // ----------------------------------------------------
    // 00E0 / 00EE
    // ----------------------------------------------------
    if (strcasecmp(line, "cls") == 0) {
        op = 0x00E0;
    } else if (strcasecmp(line, "ret") == 0 || strcasecmp(line, "return") == 0) {
        op = 0x00EE;
    }

    // ----------------------------------------------------
    // 1NNN: jump / jp <target>
    // ----------------------------------------------------
    else if (strncasecmp(line, "jump ", 5) == 0 || strncasecmp(line, "jp ", 3) == 0) {
        char target[MAX_LABEL_LEN];
        const char *arg = (strncasecmp(line, "jump ", 5) == 0) ? line + 5 : line + 3;
        if (sscanf(arg, "%63s", target) == 1) {
            uint16_t addr = 0;
            if (!resolve_address(target, table, &addr, line_num)) return -1;
            op = 0x1000 | (addr & 0x0FFF);
        } else {
            fprintf(stderr, "Syntax Error at line %d: Missing jump target.\n", line_num);
            return -1;
        }
    }

    // ----------------------------------------------------
    // 2NNN: call <target>
    // ----------------------------------------------------
    else if (strncasecmp(line, "call ", 5) == 0) {
        char target[MAX_LABEL_LEN];
        if (sscanf(line + 5, "%63s", target) == 1) {
            uint16_t addr = 0;
            if (!resolve_address(target, table, &addr, line_num)) return -1;
            op = 0x2000 | (addr & 0x0FFF);
        } else {
            fprintf(stderr, "Syntax Error at line %d: Missing call target.\n", line_num);
            return -1;
        }
    }

    // ----------------------------------------------------
    // BNNN: jump v0 + NNN / jp v0, NNN
    // ----------------------------------------------------
    else if (strncasecmp(line, "jump v0 +", 9) == 0 || strncasecmp(line, "jp v0,", 6) == 0) {
        char target[MAX_LABEL_LEN];
        const char *arg = strchr(line, (strchr(line, '+') ? '+' : ','));
        if (arg && sscanf(arg + 1, "%63s", target) == 1) {
            uint16_t addr = 0;
            if (!resolve_address(target, table, &addr, line_num)) return -1;
            op = 0xB000 | (addr & 0x0FFF);
        } else {
            fprintf(stderr, "Syntax Error at line %d: Invalid jump with V0 syntax.\n", line_num);
            return -1;
        }
    }

    // ----------------------------------------------------
    // Conditionals: 3XNN, 4XNN, 5XY0, 9XY0
    // "if vX == NN", "if vX != NN", "if vX == vY", "if vX != vY"
    // Also standard mnemonics: "se vX, NN/vY", "sne vX, NN/vY"
    // ----------------------------------------------------
    else if (strncasecmp(line, "if ", 3) == 0) {
        char r1[32], op_str[8], r2[64];
        if (sscanf(line + 3, "%31s %7s %63s", r1, op_str, r2) == 3) {
            int vx = parse_register(r1);
            if (vx == -1) {
                fprintf(stderr, "Error at line %d: Invalid register '%s'.\n", line_num, r1);
                return -1;
            }

            int vy = parse_register(r2);
            if (vy != -1) {
                // Register-register comparison: 5XY0 or 9XY0
                if (strcmp(op_str, "==") == 0) {
                    op = 0x5000 | (vx << 8) | (vy << 4);
                } else if (strcmp(op_str, "!=") == 0) {
                    op = 0x9000 | (vx << 8) | (vy << 4);
                } else {
                    fprintf(stderr, "Syntax Error at line %d: Expected '==' or '!=' in conditional.\n", line_num);
                    return -1;
                }
            } else {
                // Register-immediate comparison: 3XNN or 4XNN
                long val;
                if (!parse_number(r2, &val) || val < 0 || val > 255) {
                    fprintf(stderr, "Error at line %d: Value '%s' out of bounds (0-255).\n", line_num, r2);
                    return -1;
                }
                if (strcmp(op_str, "==") == 0) {
                    op = 0x3000 | (vx << 8) | ((uint8_t)val & 0xFF);
                } else if (strcmp(op_str, "!=") == 0) {
                    op = 0x4000 | (vx << 8) | ((uint8_t)val & 0xFF);
                } else {
                    fprintf(stderr, "Syntax Error at line %d: Expected '==' or '!=' in conditional.\n", line_num);
                    return -1;
                }
            }
        } else {
            fprintf(stderr, "Syntax Error at line %d: Invalid if statement '%s'.\n", line_num, line);
            return -1;
        }
    }

    // ----------------------------------------------------
    // Key-skip instructions: EX9E, EXA1
    // "skip_key vX", "skip_nkey vX", "skp vX", "sknp vX"
    // ----------------------------------------------------
    else if (strncasecmp(line, "skip_key ", 9) == 0 || strncasecmp(line, "skp ", 4) == 0) {
        const char *arg = (strncasecmp(line, "skip_key ", 9) == 0) ? line + 9 : line + 4;
        int vx = parse_register(trim_whitespace((char *)arg));
        if (vx == -1) {
            fprintf(stderr, "Error at line %d: Invalid register in skip_key.\n", line_num);
            return -1;
        }
        op = 0xE09E | (vx << 8);
    } else if (strncasecmp(line, "skip_nkey ", 10) == 0 || strncasecmp(line, "sknp ", 5) == 0) {
        const char *arg = (strncasecmp(line, "skip_nkey ", 10) == 0) ? line + 10 : line + 5;
        int vx = parse_register(trim_whitespace((char *)arg));
        if (vx == -1) {
            fprintf(stderr, "Error at line %d: Invalid register in skip_nkey.\n", line_num);
            return -1;
        }
        op = 0xE0A1 | (vx << 8);
    }

    // ----------------------------------------------------
    // Graphics: DXYN
    // "draw vX, vY, N" or "drw vX, vY, N"
    // ----------------------------------------------------
    else if (strncasecmp(line, "draw ", 5) == 0 || strncasecmp(line, "drw ", 4) == 0) {
        char rx[32], ry[32], rn[32];
        const char *args = (strncasecmp(line, "draw ", 5) == 0) ? line + 5 : line + 4;
        if (sscanf(args, " %31[^,], %31[^,], %31s", rx, ry, rn) == 3) {
            int vx = parse_register(trim_whitespace(rx));
            int vy = parse_register(trim_whitespace(ry));
            long n;
            if (vx == -1 || vy == -1 || !parse_number(rn, &n) || n < 0 || n > 15) {
                fprintf(stderr, "Syntax Error at line %d: Invalid arguments to draw (vX, vY, 0-15).\n", line_num);
                return -1;
            }
            op = 0xD000 | (vx << 8) | (vy << 4) | ((uint8_t)n & 0x0F);
        } else {
            fprintf(stderr, "Syntax Error at line %d: Usage: draw vX, vY, N\n", line_num);
            return -1;
        }
    }

    // ----------------------------------------------------
    // Timers & FX instructions where destination is timer:
    // "delay = vX" (FX15), "sound = vX" (FX18)
    // ----------------------------------------------------
    else if (strncasecmp(line, "delay = ", 8) == 0) {
        int vx = parse_register(trim_whitespace(line + 8));
        if (vx == -1) {
            fprintf(stderr, "Error at line %d: Invalid register in 'delay = vX'.\n", line_num);
            return -1;
        }
        op = 0xF015 | (vx << 8);
    } else if (strncasecmp(line, "sound = ", 8) == 0) {
        int vx = parse_register(trim_whitespace(line + 8));
        if (vx == -1) {
            fprintf(stderr, "Error at line %d: Invalid register in 'sound = vX'.\n", line_num);
            return -1;
        }
        op = 0xF018 | (vx << 8);
    }

    // ----------------------------------------------------
    // Index Register operations: ANNN, FX1E, FX29
    // "i = <addr|label>" (ANNN)
    // "i += vX" (FX1E)
    // "i = font vX" (FX29)
    // ----------------------------------------------------
    else if (strncasecmp(line, "i = font ", 9) == 0) {
        int vx = parse_register(trim_whitespace(line + 9));
        if (vx == -1) {
            fprintf(stderr, "Error at line %d: Invalid register for 'i = font vX'.\n", line_num);
            return -1;
        }
        op = 0xF029 | (vx << 8);
    } else if (strncasecmp(line, "i += ", 5) == 0) {
        int vx = parse_register(trim_whitespace(line + 5));
        if (vx == -1) {
            fprintf(stderr, "Error at line %d: Invalid register in 'i += vX'.\n", line_num);
            return -1;
        }
        op = 0xF01E | (vx << 8);
    } else if (strncasecmp(line, "i = ", 4) == 0) {
        char target[MAX_LABEL_LEN];
        if (sscanf(line + 4, "%63s", target) == 1) {
            uint16_t addr = 0;
            if (!resolve_address(target, table, &addr, line_num)) return -1;
            op = 0xA000 | (addr & 0x0FFF);
        } else {
            fprintf(stderr, "Syntax Error at line %d: Missing address for 'i = ...'.\n", line_num);
            return -1;
        }
    }

    // ----------------------------------------------------
    // Single-word helpers:
    // "bcd vX" (FX33)
    // "save vX" / "dump vX" (FX55)
    // "load vX" / "restore vX" (FX65)
    // ----------------------------------------------------
    else if (strncasecmp(line, "bcd ", 4) == 0) {
        int vx = parse_register(trim_whitespace(line + 4));
        if (vx == -1) {
            fprintf(stderr, "Error at line %d: Invalid register for bcd.\n", line_num);
            return -1;
        }
        op = 0xF033 | (vx << 8);
    } else if (strncasecmp(line, "save ", 5) == 0 || strncasecmp(line, "dump ", 5) == 0) {
        const char *arg = (strncasecmp(line, "save ", 5) == 0) ? line + 5 : line + 5;
        int vx = parse_register(trim_whitespace((char *)arg));
        if (vx == -1) {
            fprintf(stderr, "Error at line %d: Invalid register for save/dump.\n", line_num);
            return -1;
        }
        op = 0xF055 | (vx << 8);
    } else if (strncasecmp(line, "load ", 5) == 0 || strncasecmp(line, "restore ", 8) == 0) {
        const char *arg = (strncasecmp(line, "load ", 5) == 0) ? line + 5 : line + 8;
        int vx = parse_register(trim_whitespace((char *)arg));
        if (vx == -1) {
            fprintf(stderr, "Error at line %d: Invalid register for load/restore.\n", line_num);
            return -1;
        }
        op = 0xF065 | (vx << 8);
    }

    // ----------------------------------------------------
    // Register arithmetic, assignment, and special FX:
    // "vX = delay" (FX07)
    // "vX = key" (FX0A)
    // "vX = rand NN" (CXNN)
    // "vX = vY - vX" (8XY7)
    // "vX = vY" (8XY0)
    // "vX = NN" (6XNN)
    // "vX += NN" (7XNN)
    // "vX += vY" (8XY4)
    // "vX -= vY" (8XY5)
    // "vX |= vY" (8XY1)
    // "vX &= vY" (8XY2)
    // "vX ^= vY" (8XY3)
    // "vX >>= 1" or "vX >> 1" (8XY6)
    // "vX <<= 1" or "vX << 1" (8XYE)
    // ----------------------------------------------------
    else if ((line[0] == 'v' || line[0] == 'V')) {
        // Shift operators: vX >>= 1 / vX <<= 1
        char r_name[32], op_sym[16], shift_amt[16];
        if (sscanf(line, "%31s %15s %15s", r_name, op_sym, shift_amt) == 3 &&
            (strcmp(op_sym, ">>=") == 0 || strcmp(op_sym, ">>") == 0 ||
             strcmp(op_sym, "<<=") == 0 || strcmp(op_sym, "<<") == 0)) {
            int vx = parse_register(r_name);
            if (vx != -1) {
                if (strcmp(op_sym, ">>=") == 0 || strcmp(op_sym, ">>") == 0) {
                    op = 0x8006 | (vx << 8);
                } else {
                    op = 0x800E | (vx << 8);
                }
            } else {
                fprintf(stderr, "Error at line %d: Invalid register '%s'.\n", line_num, r_name);
                return -1;
            }
        }
        // Compound assignments: vX += ..., vX -= ..., vX |= ..., vX &= ..., vX ^= ...
        else if (sscanf(line, "%31s %15s %63[^\n]", r_name, op_sym, line + 150) >= 2) {
            // Read target RHS
            char rhs[64];
            char *eq_ptr = strchr(line, '=');
            char *op_ptr = NULL;

            if (strstr(line, "+=")) op_ptr = strstr(line, "+=");
            else if (strstr(line, "-=")) op_ptr = strstr(line, "-=");
            else if (strstr(line, "|=")) op_ptr = strstr(line, "|=");
            else if (strstr(line, "&=")) op_ptr = strstr(line, "&=");
            else if (strstr(line, "^=")) op_ptr = strstr(line, "^=");
            else if (eq_ptr) op_ptr = eq_ptr;

            if (op_ptr) {
                int vx = parse_register(r_name);
                if (vx == -1) {
                    fprintf(stderr, "Error at line %d: Invalid register '%s'.\n", line_num, r_name);
                    return -1;
                }

                char *rhs_raw = op_ptr + (op_ptr == eq_ptr ? 1 : 2);
                strncpy(rhs, trim_whitespace(rhs_raw), sizeof(rhs) - 1);
                rhs[sizeof(rhs) - 1] = '\0';

                // Check assignment operator
                if (op_ptr == eq_ptr) {
                    // "vX = delay" (FX07)
                    if (strcasecmp(rhs, "delay") == 0) {
                        op = 0xF007 | (vx << 8);
                    }
                    // "vX = key" (FX0A)
                    else if (strcasecmp(rhs, "key") == 0) {
                        op = 0xF00A | (vx << 8);
                    }
                    // "vX = rand NN" (CXNN)
                    else if (strncasecmp(rhs, "rand ", 5) == 0) {
                        long mask;
                        if (!parse_number(rhs + 5, &mask) || mask < 0 || mask > 255) {
                            fprintf(stderr, "Error at line %d: Invalid mask for rand.\n", line_num);
                            return -1;
                        }
                        op = 0xC000 | (vx << 8) | ((uint8_t)mask & 0xFF);
                    }
                    // "vX = vY - vX" (8XY7)
                    else if (strstr(rhs, "-")) {
                        char vy_name[32], vx_sub[32];
                        if (sscanf(rhs, "%31s - %31s", vy_name, vx_sub) == 2) {
                            int vy = parse_register(vy_name);
                            int vx2 = parse_register(vx_sub);
                            if (vy != -1 && vx2 == vx) {
                                op = 0x8007 | (vx << 8) | (vy << 4);
                            } else {
                                fprintf(stderr, "Syntax Error at line %d: Expected format 'vX = vY - vX'.\n", line_num);
                                return -1;
                            }
                        }
                    }
                    // "vX = vY" (8XY0)
                    else if (parse_register(rhs) != -1) {
                        int vy = parse_register(rhs);
                        op = 0x8000 | (vx << 8) | (vy << 4);
                    }
                    // "vX = NN" (6XNN)
                    else {
                        long imm;
                        if (!parse_number(rhs, &imm) || imm < 0 || imm > 255) {
                            fprintf(stderr, "Error at line %d: Value '%s' out of bounds (0-255).\n", line_num, rhs);
                            return -1;
                        }
                        op = 0x6000 | (vx << 8) | ((uint8_t)imm & 0xFF);
                    }
                } else if (strncmp(op_ptr, "+=", 2) == 0) {
                    int vy = parse_register(rhs);
                    if (vy != -1) {
                        // 8XY4: vX += vY
                        op = 0x8004 | (vx << 8) | (vy << 4);
                    } else {
                        // 7XNN: vX += NN
                        long imm;
                        if (!parse_number(rhs, &imm) || imm < 0 || imm > 255) {
                            fprintf(stderr, "Error at line %d: Value '%s' out of bounds (0-255).\n", line_num, rhs);
                            return -1;
                        }
                        op = 0x7000 | (vx << 8) | ((uint8_t)imm & 0xFF);
                    }
                } else if (strncmp(op_ptr, "-=", 2) == 0) {
                    int vy = parse_register(rhs);
                    if (vy != -1) {
                        // 8XY5: vX -= vY
                        op = 0x8005 | (vx << 8) | (vy << 4);
                    } else {
                        fprintf(stderr, "Error at line %d: CHIP-8 only supports register subtraction (vX -= vY).\n", line_num);
                        return -1;
                    }
                } else if (strncmp(op_ptr, "|=", 2) == 0) {
                    int vy = parse_register(rhs);
                    if (vy != -1) {
                        op = 0x8001 | (vx << 8) | (vy << 4);
                    } else {
                        fprintf(stderr, "Error at line %d: Expected register after '|='.\n", line_num);
                        return -1;
                    }
                } else if (strncmp(op_ptr, "&=", 2) == 0) {
                    int vy = parse_register(rhs);
                    if (vy != -1) {
                        op = 0x8002 | (vx << 8) | (vy << 4);
                    } else {
                        fprintf(stderr, "Error at line %d: Expected register after '&='.\n", line_num);
                        return -1;
                    }
                } else if (strncmp(op_ptr, "^=", 2) == 0) {
                    int vy = parse_register(rhs);
                    if (vy != -1) {
                        op = 0x8003 | (vx << 8) | (vy << 4);
                    } else {
                        fprintf(stderr, "Error at line %d: Expected register after '^='.\n", line_num);
                        return -1;
                    }
                }
            }
        }
    }

    if (op != 0) {
        out_bytes[0] = (op >> 8) & 0xFF;
        out_bytes[1] = op & 0xFF;
        return 2;
    }

    fprintf(stderr, "Syntax Error at line %d: Unknown instruction '%s'\n", line_num, line);
    return -1;
}

// Splits a line into an optional label and instruction
// Returns pointer to instruction string (may be empty if line was only a label)
static char *extract_label(char *line, char *label_buf, size_t label_buf_size) {
    label_buf[0] = '\0';
    char *colon = strchr(line, ':');
    if (!colon) {
        return trim_whitespace(line);
    }

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

        // If there's an instruction or data directive
        if (strlen(instr) > 0) {
            int db_count = get_data_byte_count(instr, line_num);
            int byte_size = (db_count > 0) ? db_count : 2;

            if (db_count == -2) {
                // Syntax error inside db
                fclose(source);
                return 1;
            }

            if (current_address + byte_size > MAX_ADDRESS) {
                fprintf(stderr, "Error at line %d: Program size exceeded CHIP-8 memory limit (0x%03X).\n",
                        line_num, MAX_ADDRESS);
                fclose(source);
                return 1;
            }
            current_address += byte_size;
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
        strncpy(raw_line, line_buf, sizeof(raw_line) - 1);
        raw_line[sizeof(raw_line) - 1] = '\0';
        strip_comments(raw_line);

        strip_comments(line_buf);
        char *instr = extract_label(line_buf, label, sizeof(label));

        if (strlen(instr) == 0) continue;

        uint8_t bytes[MAX_BYTES_PER_LINE];
        int num_bytes = assemble_line(instr, &table, bytes, line_num);

        if (num_bytes <= 0) {
            has_error = true;
            break;
        }

        if (num_bytes == 2 && get_data_byte_count(instr, line_num) <= 0) {
            uint16_t op = (bytes[0] << 8) | bytes[1];
            printf("  [0x%03X] %-25s -> Opcode: 0x%04X\n", current_address, trim_whitespace(raw_line), op);
        } else {
            printf("  [0x%03X] %-25s -> %d data bytes\n", current_address, trim_whitespace(raw_line), num_bytes);
        }

        if (fwrite(bytes, sizeof(uint8_t), num_bytes, output) != (size_t)num_bytes) {
            fprintf(stderr, "Error writing bytes to output file.\n");
            has_error = true;
            break;
        }

        current_address += num_bytes;
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
