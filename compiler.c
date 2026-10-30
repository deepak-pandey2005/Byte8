#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdint.h>
#include <ctype.h>
#include <stdbool.h>

#define MAX_LABELS 512
#define MAX_LABEL_LEN 64
#define MAX_DEFINES 512
#define MAX_DEFINE_NAME_LEN 64
#define MAX_DEFINE_VAL_LEN 128
#define MAX_SOURCE_LINES 4096
#define MAX_LINE_LEN 512
#define MAX_BYTES_PER_LINE 32
#define START_ADDRESS 0x200
#define MAX_ADDRESS 0x0FFF
#define MAX_INCLUDE_DEPTH 16

typedef struct {
    char name[MAX_LABEL_LEN];
    uint16_t address;
    int defined_line;
    char defined_file[128];
} Label;

typedef struct {
    Label items[MAX_LABELS];
    int count;
} LabelTable;

typedef struct {
    char name[MAX_DEFINE_NAME_LEN];
    char value[MAX_DEFINE_VAL_LEN];
} Define;

typedef struct {
    Define items[MAX_DEFINES];
    int count;
} DefineTable;

typedef struct {
    char text[MAX_LINE_LEN];
    char filename[128];
    int line_number;
} SourceLine;

typedef struct {
    SourceLine lines[MAX_SOURCE_LINES];
    int count;
} PreprocessedSource;

// Helper: Trim leading and trailing whitespace
static char *trim_whitespace(char *str) {
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;

    char *end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

// Helper: Strip comments (starting with ;, #, //, or /* ... */)
// Preserves # when followed immediately by define or include
static void strip_comments(char *line) {
    char *trimmed = line;
    while (isspace((unsigned char)*trimmed)) trimmed++;

    // Check if line begins with preprocessor directive (#define, #include)
    bool is_hash_directive = false;
    if (*trimmed == '#') {
        if (strncasecmp(trimmed, "#define", 7) == 0 || strncasecmp(trimmed, "#include", 8) == 0) {
            is_hash_directive = true;
        }
    }

    // Handle /* ... */ block comments on a line
    char *block_start = strstr(line, "/*");
    while (block_start) {
        char *block_end = strstr(block_start + 2, "*/");
        if (block_end) {
            memset(block_start, ' ', (block_end + 2) - block_start);
            block_start = strstr(block_end + 2, "/*");
        } else {
            *block_start = '\0';
            break;
        }
    }

    char *comment = NULL;
    char *c1 = strchr(line, ';');
    char *c2 = is_hash_directive ? NULL : strchr(line, '#');
    char *c3 = strstr(line, "//");

    if (c1) comment = c1;
    if (c2 && (!comment || c2 < comment)) comment = c2;
    if (c3 && (!comment || c3 < comment)) comment = c3;

    if (comment) {
        *comment = '\0';
    }
}

// Find define symbol
static int find_define(const DefineTable *dt, const char *name) {
    for (int i = 0; i < dt->count; i++) {
        if (strcmp(dt->items[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

// Add or update a define
static bool add_define(DefineTable *dt, const char *name, const char *value, const char *file, int line_num) {
    if (strlen(name) == 0) {
        fprintf(stderr, "Error in %s:%d: Empty define name.\n", file, line_num);
        return false;
    }
    if (!isalpha((unsigned char)name[0]) && name[0] != '_') {
        fprintf(stderr, "Error in %s:%d: Define name '%s' must start with letter or underscore.\n",
                file, line_num, name);
        return false;
    }
    for (size_t i = 1; i < strlen(name); i++) {
        if (!isalnum((unsigned char)name[i]) && name[i] != '_') {
            fprintf(stderr, "Error in %s:%d: Invalid character in define name '%s'.\n",
                    file, line_num, name);
            return false;
        }
    }

    int idx = find_define(dt, name);
    if (idx != -1) {
        strncpy(dt->items[idx].value, value, MAX_DEFINE_VAL_LEN - 1);
        dt->items[idx].value[MAX_DEFINE_VAL_LEN - 1] = '\0';
        return true;
    }

    if (dt->count >= MAX_DEFINES) {
        fprintf(stderr, "Error in %s:%d: Maximum defines limit reached (%d).\n", file, line_num, MAX_DEFINES);
        return false;
    }

    strncpy(dt->items[dt->count].name, name, MAX_DEFINE_NAME_LEN - 1);
    dt->items[dt->count].name[MAX_DEFINE_NAME_LEN - 1] = '\0';
    strncpy(dt->items[dt->count].value, value, MAX_DEFINE_VAL_LEN - 1);
    dt->items[dt->count].value[MAX_DEFINE_VAL_LEN - 1] = '\0';
    dt->count++;
    return true;
}

// Substitute defines in a line (word-boundary replacement)
static void expand_defines_in_line(char *line, const DefineTable *dt) {
    if (dt->count == 0) return;

    char result[MAX_LINE_LEN];
    result[0] = '\0';

    char *src = line;
    char *dst = result;
    size_t remaining = sizeof(result) - 1;

    while (*src && remaining > 0) {
        if (isalpha((unsigned char)*src) || *src == '_') {
            char word[MAX_DEFINE_NAME_LEN];
            int wlen = 0;
            while ((isalnum((unsigned char)*src) || *src == '_') && wlen < (int)sizeof(word) - 1) {
                word[wlen++] = *src++;
            }
            word[wlen] = '\0';

            int def_idx = find_define(dt, word);
            if (def_idx != -1) {
                const char *val = dt->items[def_idx].value;
                size_t vlen = strlen(val);
                if (vlen > remaining) vlen = remaining;
                memcpy(dst, val, vlen);
                dst += vlen;
                remaining -= vlen;
            } else {
                if ((size_t)wlen > remaining) wlen = (int)remaining;
                memcpy(dst, word, wlen);
                dst += wlen;
                remaining -= wlen;
            }
        } else {
            *dst++ = *src++;
            remaining--;
        }
    }
    *dst = '\0';
    strncpy(line, result, MAX_LINE_LEN - 1);
    line[MAX_LINE_LEN - 1] = '\0';
}

// Preprocessor: handles #include, %include, #define, const
static bool preprocess_file(const char *filename, PreprocessedSource *ps, DefineTable *dt, int depth) {
    if (depth > MAX_INCLUDE_DEPTH) {
        fprintf(stderr, "Error: Exceeded maximum include recursion depth (%d) in '%s'.\n", MAX_INCLUDE_DEPTH, filename);
        return false;
    }

    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Error: Could not open file '%s'.\n", filename);
        return false;
    }

    char raw_buf[MAX_LINE_LEN];
    int line_num = 0;

    // Determine directory of current file for relative includes
    char base_dir[256] = "";
    const char *last_slash = strrchr(filename, '/');
    if (last_slash) {
        size_t dirlen = last_slash - filename + 1;
        if (dirlen < sizeof(base_dir)) {
            strncpy(base_dir, filename, dirlen);
            base_dir[dirlen] = '\0';
        }
    }

    while (fgets(raw_buf, sizeof(raw_buf), f)) {
        line_num++;
        strip_comments(raw_buf);
        char *line = trim_whitespace(raw_buf);
        if (*line == '\0') continue;

        // Check for #include "file" or %include "file"
        if (strncasecmp(line, "#include", 8) == 0 || strncasecmp(line, "%include", 8) == 0) {
            char *quote_start = strchr(line, '"');
            if (!quote_start) quote_start = strchr(line, '<');
            char *quote_end = quote_start ? strchr(quote_start + 1, (quote_start[0] == '<' ? '>' : '"')) : NULL;

            if (!quote_start || !quote_end) {
                fprintf(stderr, "Error in %s:%d: Expected #include \"filename.txt\"\n", filename, line_num);
                fclose(f);
                return false;
            }

            char inc_name[128];
            size_t nlen = quote_end - (quote_start + 1);
            if (nlen >= sizeof(inc_name)) nlen = sizeof(inc_name) - 1;
            strncpy(inc_name, quote_start + 1, nlen);
            inc_name[nlen] = '\0';

            char full_path[512];
            if (inc_name[0] == '/' || strlen(base_dir) == 0) {
                snprintf(full_path, sizeof(full_path), "%s", inc_name);
            } else {
                snprintf(full_path, sizeof(full_path), "%s%s", base_dir, inc_name);
            }

            if (!preprocess_file(full_path, ps, dt, depth + 1)) {
                fclose(f);
                return false;
            }
            continue;
        }

        // Check for #define NAME VALUE or const NAME = VALUE
        if (strncasecmp(line, "#define", 7) == 0) {
            char def_name[MAX_DEFINE_NAME_LEN];
            char def_val[MAX_DEFINE_VAL_LEN];
            def_name[0] = '\0';
            def_val[0] = '\0';
            if (sscanf(line + 7, "%63s %127[^\n]", def_name, def_val) >= 1) {
                char *v = trim_whitespace(def_val);
                expand_defines_in_line(v, dt);
                if (!add_define(dt, def_name, v, filename, line_num)) {
                    fclose(f);
                    return false;
                }
            } else {
                fprintf(stderr, "Error in %s:%d: Invalid #define syntax. Usage: #define NAME VALUE\n", filename, line_num);
                fclose(f);
                return false;
            }
            continue;
        }

        if (strncasecmp(line, "const ", 6) == 0) {
            char def_name[MAX_DEFINE_NAME_LEN];
            char *eq = strchr(line + 6, '=');
            if (eq) {
                int nlen = eq - (line + 6);
                if (nlen >= (int)sizeof(def_name)) nlen = (int)sizeof(def_name) - 1;
                memcpy(def_name, line + 6, nlen);
                def_name[nlen] = '\0';
                char *clean_name = trim_whitespace(def_name);

                char val_buf[MAX_DEFINE_VAL_LEN];
                strncpy(val_buf, trim_whitespace(eq + 1), sizeof(val_buf) - 1);
                val_buf[sizeof(val_buf) - 1] = '\0';
                expand_defines_in_line(val_buf, dt);

                if (!add_define(dt, clean_name, val_buf, filename, line_num)) {
                    fclose(f);
                    return false;
                }
            } else {
                fprintf(stderr, "Error in %s:%d: Invalid const syntax. Usage: const NAME = VALUE\n", filename, line_num);
                fclose(f);
                return false;
            }
            continue;
        }

        // Normal assembly line: expand defines
        expand_defines_in_line(line, dt);

        if (ps->count >= MAX_SOURCE_LINES) {
            fprintf(stderr, "Error: Maximum source line limit (%d) reached.\n", MAX_SOURCE_LINES);
            fclose(f);
            return false;
        }

        strncpy(ps->lines[ps->count].text, line, MAX_LINE_LEN - 1);
        ps->lines[ps->count].text[MAX_LINE_LEN - 1] = '\0';
        strncpy(ps->lines[ps->count].filename, filename, sizeof(ps->lines[0].filename) - 1);
        ps->lines[ps->count].line_number = line_num;
        ps->count++;
    }

    fclose(f);
    return true;
}

// Find label by name
static int find_label(const LabelTable *table, const char *name) {
    for (int i = 0; i < table->count; i++) {
        if (strcmp(table->items[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

// Add a label to symbol table
static bool add_label(LabelTable *table, const char *name, uint16_t address, const char *file, int line_num) {
    if (strlen(name) == 0) {
        fprintf(stderr, "Error in %s:%d: Empty label name.\n", file, line_num);
        return false;
    }
    if (strlen(name) >= MAX_LABEL_LEN) {
        fprintf(stderr, "Error in %s:%d: Label name '%s' exceeds max length of %d.\n",
                file, line_num, name, MAX_LABEL_LEN - 1);
        return false;
    }
    if (!isalpha((unsigned char)name[0]) && name[0] != '_') {
        fprintf(stderr, "Error in %s:%d: Label '%s' must start with a letter or underscore.\n",
                file, line_num, name);
        return false;
    }
    for (size_t i = 1; i < strlen(name); i++) {
        if (!isalnum((unsigned char)name[i]) && name[i] != '_') {
            fprintf(stderr, "Error in %s:%d: Label '%s' contains invalid character '%c'.\n",
                    file, line_num, name, name[i]);
            return false;
        }
    }

    if (find_label(table, name) != -1) {
        fprintf(stderr, "Error in %s:%d: Label '%s' already defined.\n", file, line_num, name);
        return false;
    }

    if (table->count >= MAX_LABELS) {
        fprintf(stderr, "Error in %s:%d: Maximum label count (%d) exceeded.\n",
                file, line_num, MAX_LABELS);
        return false;
    }

    strncpy(table->items[table->count].name, name, MAX_LABEL_LEN - 1);
    table->items[table->count].name[MAX_LABEL_LEN - 1] = '\0';
    table->items[table->count].address = address;
    table->items[table->count].defined_line = line_num;
    strncpy(table->items[table->count].defined_file, file, sizeof(table->items[0].defined_file) - 1);
    table->count++;
    return true;
}

// Parse integer with support for decimal, hex (0x...), and binary (0b...)
static bool parse_number(const char *str, long *out_val) {
    char *trimmed = trim_whitespace((char *)str);
    if (!trimmed || *trimmed == '\0') return false;

    if ((strncasecmp(trimmed, "0b", 2) == 0) && strlen(trimmed) > 2) {
        char *endptr = NULL;
        long val = strtol(trimmed + 2, &endptr, 2);
        if (*endptr == '\0') {
            *out_val = val;
            return true;
        }
        return false;
    }

    char *endptr = NULL;
    long val = strtol(trimmed, &endptr, 0);
    if (endptr != trimmed && *endptr == '\0') {
        *out_val = val;
        return true;
    }

    return false;
}

// Resolves a target string into a 12-bit address
static bool resolve_address(const char *target, const LabelTable *table, uint16_t *out_addr, const char *file, int line_num) {
    long num;
    if (parse_number(target, &num)) {
        if (num < 0 || num > MAX_ADDRESS) {
            fprintf(stderr, "Error in %s:%d: Memory address 0x%lX out of bounds (0x000-0x%03X).\n",
                    file, line_num, num, MAX_ADDRESS);
            return false;
        }
        *out_addr = (uint16_t)num;
        return true;
    }

    int idx = find_label(table, target);
    if (idx != -1) {
        *out_addr = table->items[idx].address;
        return true;
    }

    fprintf(stderr, "Error in %s:%d: Undefined label or invalid address '%s'.\n", file, line_num, target);
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
static int get_data_byte_count(const char *instr, const char *file, int line_num) {
    char copy[MAX_LINE_LEN];
    strncpy(copy, instr, sizeof(copy) - 1);
    copy[sizeof(copy) - 1] = '\0';

    char *tok = strtok(copy, " \t");
    if (!tok) return 0;

    if (strcasecmp(tok, "db") != 0 && strcasecmp(tok, "byte") != 0) {
        return -1;
    }

    int count = 0;
    while ((tok = strtok(NULL, " ,\t")) != NULL) {
        tok = trim_whitespace(tok);
        if (*tok == '\0') continue;
        long val;
        if (!parse_number(tok, &val) || val < 0 || val > 255) {
            fprintf(stderr, "Error in %s:%d: Invalid byte value '%s' in db directive.\n", file, line_num, tok);
            return -2;
        }
        count++;
    }

    if (count == 0) {
        fprintf(stderr, "Error in %s:%d: 'db' directive requires at least one byte value.\n", file, line_num);
        return -2;
    }

    return count;
}

// Parses an instruction or directive into machine bytes
static int assemble_line(char *line, const LabelTable *table, uint8_t *out_bytes, const char *file, int line_num) {
    line = trim_whitespace(line);

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
                fprintf(stderr, "Error in %s:%d: Invalid byte value '%s'.\n", file, line_num, tok);
                return -1;
            }
            out_bytes[count++] = (uint8_t)val;
        }
        if (count == 0) {
            fprintf(stderr, "Error in %s:%d: Empty db directive.\n", file, line_num);
            return -1;
        }
        return count;
    }

    uint16_t op = 0;

    if (strcasecmp(line, "cls") == 0) {
        op = 0x00E0;
    } else if (strcasecmp(line, "ret") == 0 || strcasecmp(line, "return") == 0) {
        op = 0x00EE;
    } else if (strncasecmp(line, "jump ", 5) == 0 || strncasecmp(line, "jp ", 3) == 0) {
        char target[MAX_LABEL_LEN];
        const char *arg = (strncasecmp(line, "jump ", 5) == 0) ? line + 5 : line + 3;
        if (sscanf(arg, "%63s", target) == 1) {
            uint16_t addr = 0;
            if (!resolve_address(target, table, &addr, file, line_num)) return -1;
            op = 0x1000 | (addr & 0x0FFF);
        } else {
            fprintf(stderr, "Syntax Error in %s:%d: Missing jump target.\n", file, line_num);
            return -1;
        }
    } else if (strncasecmp(line, "call ", 5) == 0) {
        char target[MAX_LABEL_LEN];
        if (sscanf(line + 5, "%63s", target) == 1) {
            uint16_t addr = 0;
            if (!resolve_address(target, table, &addr, file, line_num)) return -1;
            op = 0x2000 | (addr & 0x0FFF);
        } else {
            fprintf(stderr, "Syntax Error in %s:%d: Missing call target.\n", file, line_num);
            return -1;
        }
    } else if (strncasecmp(line, "jump v0 +", 9) == 0 || strncasecmp(line, "jp v0,", 6) == 0) {
        char target[MAX_LABEL_LEN];
        const char *arg = strchr(line, (strchr(line, '+') ? '+' : ','));
        if (arg && sscanf(arg + 1, "%63s", target) == 1) {
            uint16_t addr = 0;
            if (!resolve_address(target, table, &addr, file, line_num)) return -1;
            op = 0xB000 | (addr & 0x0FFF);
        } else {
            fprintf(stderr, "Syntax Error in %s:%d: Invalid jump with V0 syntax.\n", file, line_num);
            return -1;
        }
    } else if (strncasecmp(line, "if ", 3) == 0) {
        char r1[32], op_str[8], r2[64];
        if (sscanf(line + 3, "%31s %7s %63s", r1, op_str, r2) == 3) {
            int vx = parse_register(r1);
            if (vx == -1) {
                fprintf(stderr, "Error in %s:%d: Invalid register '%s'.\n", file, line_num, r1);
                return -1;
            }

            int vy = parse_register(r2);
            if (vy != -1) {
                if (strcmp(op_str, "==") == 0) {
                    op = 0x5000 | (vx << 8) | (vy << 4);
                } else if (strcmp(op_str, "!=") == 0) {
                    op = 0x9000 | (vx << 8) | (vy << 4);
                } else {
                    fprintf(stderr, "Syntax Error in %s:%d: Expected '==' or '!=' in conditional.\n", file, line_num);
                    return -1;
                }
            } else {
                long val;
                if (!parse_number(r2, &val) || val < 0 || val > 255) {
                    fprintf(stderr, "Error in %s:%d: Value '%s' out of bounds (0-255).\n", file, line_num, r2);
                    return -1;
                }
                if (strcmp(op_str, "==") == 0) {
                    op = 0x3000 | (vx << 8) | ((uint8_t)val & 0xFF);
                } else if (strcmp(op_str, "!=") == 0) {
                    op = 0x4000 | (vx << 8) | ((uint8_t)val & 0xFF);
                } else {
                    fprintf(stderr, "Syntax Error in %s:%d: Expected '==' or '!=' in conditional.\n", file, line_num);
                    return -1;
                }
            }
        } else {
            fprintf(stderr, "Syntax Error in %s:%d: Invalid if statement '%s'.\n", file, line_num, line);
            return -1;
        }
    } else if (strncasecmp(line, "skip_key ", 9) == 0 || strncasecmp(line, "skp ", 4) == 0) {
        const char *arg = (strncasecmp(line, "skip_key ", 9) == 0) ? line + 9 : line + 4;
        int vx = parse_register(trim_whitespace((char *)arg));
        if (vx == -1) {
            fprintf(stderr, "Error in %s:%d: Invalid register in skip_key.\n", file, line_num);
            return -1;
        }
        op = 0xE09E | (vx << 8);
    } else if (strncasecmp(line, "skip_nkey ", 10) == 0 || strncasecmp(line, "sknp ", 5) == 0) {
        const char *arg = (strncasecmp(line, "skip_nkey ", 10) == 0) ? line + 10 : line + 5;
        int vx = parse_register(trim_whitespace((char *)arg));
        if (vx == -1) {
            fprintf(stderr, "Error in %s:%d: Invalid register in skip_nkey.\n", file, line_num);
            return -1;
        }
        op = 0xE0A1 | (vx << 8);
    } else if (strncasecmp(line, "draw ", 5) == 0 || strncasecmp(line, "drw ", 4) == 0) {
        char rx[32], ry[32], rn[32];
        const char *args = (strncasecmp(line, "draw ", 5) == 0) ? line + 5 : line + 4;
        if (sscanf(args, " %31[^,], %31[^,], %31s", rx, ry, rn) == 3) {
            int vx = parse_register(trim_whitespace(rx));
            int vy = parse_register(trim_whitespace(ry));
            long n;
            if (vx == -1 || vy == -1 || !parse_number(rn, &n) || n < 0 || n > 15) {
                fprintf(stderr, "Syntax Error in %s:%d: Invalid arguments to draw (vX, vY, 0-15).\n", file, line_num);
                return -1;
            }
            op = 0xD000 | (vx << 8) | (vy << 4) | ((uint8_t)n & 0x0F);
        } else {
            fprintf(stderr, "Syntax Error in %s:%d: Usage: draw vX, vY, N\n", file, line_num);
            return -1;
        }
    } else if (strncasecmp(line, "delay = ", 8) == 0) {
        int vx = parse_register(trim_whitespace(line + 8));
        if (vx == -1) {
            fprintf(stderr, "Error in %s:%d: Invalid register in 'delay = vX'.\n", file, line_num);
            return -1;
        }
        op = 0xF015 | (vx << 8);
    } else if (strncasecmp(line, "sound = ", 8) == 0) {
        int vx = parse_register(trim_whitespace(line + 8));
        if (vx == -1) {
            fprintf(stderr, "Error in %s:%d: Invalid register in 'sound = vX'.\n", file, line_num);
            return -1;
        }
        op = 0xF018 | (vx << 8);
    } else if (strncasecmp(line, "i = font ", 9) == 0) {
        int vx = parse_register(trim_whitespace(line + 9));
        if (vx == -1) {
            fprintf(stderr, "Error in %s:%d: Invalid register for 'i = font vX'.\n", file, line_num);
            return -1;
        }
        op = 0xF029 | (vx << 8);
    } else if (strncasecmp(line, "i += ", 5) == 0) {
        int vx = parse_register(trim_whitespace(line + 5));
        if (vx == -1) {
            fprintf(stderr, "Error in %s:%d: Invalid register in 'i += vX'.\n", file, line_num);
            return -1;
        }
        op = 0xF01E | (vx << 8);
    } else if (strncasecmp(line, "i = ", 4) == 0) {
        char target[MAX_LABEL_LEN];
        if (sscanf(line + 4, "%63s", target) == 1) {
            uint16_t addr = 0;
            if (!resolve_address(target, table, &addr, file, line_num)) return -1;
            op = 0xA000 | (addr & 0x0FFF);
        } else {
            fprintf(stderr, "Syntax Error in %s:%d: Missing address for 'i = ...'.\n", file, line_num);
            return -1;
        }
    } else if (strcasecmp(line, "bcd") == 0 || strncasecmp(line, "bcd ", 4) == 0) {
        int vx = parse_register(trim_whitespace(line + 3));
        if (vx == -1) {
            fprintf(stderr, "Error in %s:%d: Invalid register for bcd.\n", file, line_num);
            return -1;
        }
        op = 0xF033 | (vx << 8);
    } else if (strncasecmp(line, "save ", 5) == 0 || strncasecmp(line, "dump ", 5) == 0) {
        const char *arg = (strncasecmp(line, "save ", 5) == 0) ? line + 5 : line + 5;
        int vx = parse_register(trim_whitespace((char *)arg));
        if (vx == -1) {
            fprintf(stderr, "Error in %s:%d: Invalid register for save/dump.\n", file, line_num);
            return -1;
        }
        op = 0xF055 | (vx << 8);
    } else if (strncasecmp(line, "load ", 5) == 0 || strncasecmp(line, "restore ", 8) == 0) {
        const char *arg = (strncasecmp(line, "load ", 5) == 0) ? line + 5 : line + 8;
        int vx = parse_register(trim_whitespace((char *)arg));
        if (vx == -1) {
            fprintf(stderr, "Error in %s:%d: Invalid register for load/restore.\n", file, line_num);
            return -1;
        }
        op = 0xF065 | (vx << 8);
    } else if ((line[0] == 'v' || line[0] == 'V')) {
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
                fprintf(stderr, "Error in %s:%d: Invalid register '%s'.\n", file, line_num, r_name);
                return -1;
            }
        } else if (sscanf(line, "%31s %15s %63[^\n]", r_name, op_sym, line + 150) >= 2) {
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
                    fprintf(stderr, "Error in %s:%d: Invalid register '%s'.\n", file, line_num, r_name);
                    return -1;
                }

                char *rhs_raw = op_ptr + (op_ptr == eq_ptr ? 1 : 2);
                strncpy(rhs, trim_whitespace(rhs_raw), sizeof(rhs) - 1);
                rhs[sizeof(rhs) - 1] = '\0';

                if (op_ptr == eq_ptr) {
                    if (strcasecmp(rhs, "delay") == 0) {
                        op = 0xF007 | (vx << 8);
                    } else if (strcasecmp(rhs, "key") == 0) {
                        op = 0xF00A | (vx << 8);
                    } else if (strncasecmp(rhs, "rand ", 5) == 0) {
                        long mask;
                        if (!parse_number(rhs + 5, &mask) || mask < 0 || mask > 255) {
                            fprintf(stderr, "Error in %s:%d: Invalid mask for rand.\n", file, line_num);
                            return -1;
                        }
                        op = 0xC000 | (vx << 8) | ((uint8_t)mask & 0xFF);
                    } else if (strstr(rhs, "-")) {
                        char vy_name[32], vx_sub[32];
                        if (sscanf(rhs, "%31s - %31s", vy_name, vx_sub) == 2) {
                            int vy = parse_register(vy_name);
                            int vx2 = parse_register(vx_sub);
                            if (vy != -1 && vx2 == vx) {
                                op = 0x8007 | (vx << 8) | (vy << 4);
                            } else {
                                fprintf(stderr, "Syntax Error in %s:%d: Expected format 'vX = vY - vX'.\n", file, line_num);
                                return -1;
                            }
                        }
                    } else if (parse_register(rhs) != -1) {
                        int vy = parse_register(rhs);
                        op = 0x8000 | (vx << 8) | (vy << 4);
                    } else {
                        long imm;
                        if (!parse_number(rhs, &imm) || imm < 0 || imm > 255) {
                            fprintf(stderr, "Error in %s:%d: Value '%s' out of bounds (0-255).\n", file, line_num, rhs);
                            return -1;
                        }
                        op = 0x6000 | (vx << 8) | ((uint8_t)imm & 0xFF);
                    }
                } else if (strncmp(op_ptr, "+=", 2) == 0) {
                    int vy = parse_register(rhs);
                    if (vy != -1) {
                        op = 0x8004 | (vx << 8) | (vy << 4);
                    } else {
                        long imm;
                        if (!parse_number(rhs, &imm) || imm < 0 || imm > 255) {
                            fprintf(stderr, "Error in %s:%d: Value '%s' out of bounds (0-255).\n", file, line_num, rhs);
                            return -1;
                        }
                        op = 0x7000 | (vx << 8) | ((uint8_t)imm & 0xFF);
                    }
                } else if (strncmp(op_ptr, "-=", 2) == 0) {
                    int vy = parse_register(rhs);
                    if (vy != -1) {
                        op = 0x8005 | (vx << 8) | (vy << 4);
                    } else {
                        long imm;
                        if (parse_number(rhs, &imm) && imm >= 0 && imm <= 255) {
                            uint8_t sub_val = (uint8_t)((256 - imm) & 0xFF);
                            op = 0x7000 | (vx << 8) | sub_val;
                        } else {
                            fprintf(stderr, "Error in %s:%d: Invalid operand for '-=': %s\n", file, line_num, rhs);
                            return -1;
                        }
                    }
                } else if (strncmp(op_ptr, "|=", 2) == 0) {
                    int vy = parse_register(rhs);
                    if (vy != -1) {
                        op = 0x8001 | (vx << 8) | (vy << 4);
                    } else {
                        fprintf(stderr, "Error in %s:%d: Expected register after '|='.\n", file, line_num);
                        return -1;
                    }
                } else if (strncmp(op_ptr, "&=", 2) == 0) {
                    int vy = parse_register(rhs);
                    if (vy != -1) {
                        op = 0x8002 | (vx << 8) | (vy << 4);
                    } else {
                        fprintf(stderr, "Error in %s:%d: Expected register after '&='.\n", file, line_num);
                        return -1;
                    }
                } else if (strncmp(op_ptr, "^=", 2) == 0) {
                    int vy = parse_register(rhs);
                    if (vy != -1) {
                        op = 0x8003 | (vx << 8) | (vy << 4);
                    } else {
                        fprintf(stderr, "Error in %s:%d: Expected register after '^='.\n", file, line_num);
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

    fprintf(stderr, "Syntax Error in %s:%d: Unknown instruction '%s'\n", file, line_num, line);
    return -1;
}

// Extracts optional label from line
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

    printf("=======================================\n");
    printf("Compiling source: %s\n", argv[1]);
    printf("=======================================\n");

    // ==========================================
    // PREPROCESS STAGE: #include, #define, const
    // ==========================================
    printf("[Preprocess] Resolving includes and macros...\n");
    PreprocessedSource ps = { .count = 0 };
    DefineTable dt = { .count = 0 };

    if (!preprocess_file(argv[1], &ps, &dt, 0)) {
        return 1;
    }
    printf("[Preprocess] Done. Total lines: %d, active defines: %d\n", ps.count, dt.count);

    // ==========================================
    // PASS 1: Symbol resolution & address sizing
    // ==========================================
    printf("[Pass 1] Scanning labels and computing addresses...\n");
    LabelTable table = { .count = 0 };
    uint16_t current_address = START_ADDRESS;
    char label[MAX_LABEL_LEN];

    for (int i = 0; i < ps.count; i++) {
        char line_copy[MAX_LINE_LEN];
        strncpy(line_copy, ps.lines[i].text, sizeof(line_copy) - 1);
        line_copy[sizeof(line_copy) - 1] = '\0';

        char *instr = extract_label(line_copy, label, sizeof(label));

        if (strlen(label) > 0) {
            if (!add_label(&table, label, current_address, ps.lines[i].filename, ps.lines[i].line_number)) {
                return 1;
            }
            printf("  Defined label '%s' at 0x%03X (%s:%d)\n",
                   label, current_address, ps.lines[i].filename, ps.lines[i].line_number);
        }

        if (strlen(instr) > 0) {
            int db_count = get_data_byte_count(instr, ps.lines[i].filename, ps.lines[i].line_number);
            int byte_size = (db_count > 0) ? db_count : 2;

            if (db_count == -2) {
                return 1;
            }

            if (current_address + byte_size > MAX_ADDRESS) {
                fprintf(stderr, "Error in %s:%d: Program size exceeded CHIP-8 memory limit (0x%03X).\n",
                        ps.lines[i].filename, ps.lines[i].line_number, MAX_ADDRESS);
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
    FILE *output = fopen(argv[2], "wb");
    if (!output) {
        fprintf(stderr, "Error: Could not open output file '%s'.\n", argv[2]);
        return 1;
    }

    current_address = START_ADDRESS;
    bool has_error = false;

    for (int i = 0; i < ps.count; i++) {
        char line_copy[MAX_LINE_LEN];
        strncpy(line_copy, ps.lines[i].text, sizeof(line_copy) - 1);
        line_copy[sizeof(line_copy) - 1] = '\0';

        char *instr = extract_label(line_copy, label, sizeof(label));
        if (strlen(instr) == 0) continue;

        uint8_t bytes[MAX_BYTES_PER_LINE];
        int num_bytes = assemble_line(instr, &table, bytes, ps.lines[i].filename, ps.lines[i].line_number);

        if (num_bytes <= 0) {
            has_error = true;
            break;
        }

        if (num_bytes == 2 && get_data_byte_count(instr, ps.lines[i].filename, ps.lines[i].line_number) <= 0) {
            uint16_t op = (bytes[0] << 8) | bytes[1];
            printf("  [0x%03X] %-25s -> Opcode: 0x%04X\n", current_address, ps.lines[i].text, op);
        } else {
            printf("  [0x%03X] %-25s -> %d data bytes\n", current_address, ps.lines[i].text, num_bytes);
        }

        if (fwrite(bytes, sizeof(uint8_t), num_bytes, output) != (size_t)num_bytes) {
            fprintf(stderr, "Error writing bytes to output file.\n");
            has_error = true;
            break;
        }

        current_address += num_bytes;
    }

    fclose(output);

    if (has_error) {
        remove(argv[2]);
        printf("\nCompilation FAILED due to errors.\n");
        return 1;
    }

    printf("\nCompilation SUCCESSFUL! Created ROM: %s (%d bytes)\n",
           argv[2], current_address - START_ADDRESS);
    printf("=======================================\n");

    return 0;
}
