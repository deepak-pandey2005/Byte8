#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

#define MAX_TOKEN_LEN 128
#define MAX_CHILDREN 64
#define MAX_PARAMS 8
#define MAX_LOCALS 32
#define MAX_GLOBALS 64
#define MAX_SPRITES 32
#define MAX_SPRITE_BYTES 64

// ============================================================================
// TOKEN DEFINITIONS
// ============================================================================
typedef enum {
    TOK_EOF = 0,
    TOK_INT_LIT,
    TOK_IDENT,
    // Keywords
    TOK_BYTE,
    TOK_INT,
    TOK_VOID,
    TOK_SPRITE,
    TOK_IF,
    TOK_ELSE,
    TOK_WHILE,
    TOK_FOR,
    TOK_RETURN,
    // Hardware Intrinsics
    TOK_CLS,
    TOK_DRAW,
    TOK_DRAW_FONT,
    TOK_SYNC,
    TOK_KEY_DOWN,
    TOK_WAIT_KEY,
    TOK_RANDOM,
    TOK_SOUND_TIMER,
    TOK_DELAY_TIMER,
    TOK_COLLISION,
    // Operators
    TOK_ASSIGN,         // =
    TOK_PLUS,           // +
    TOK_MINUS,          // -
    TOK_STAR,           // *
    TOK_SLASH,          // /
    TOK_PERCENT,        // %
    TOK_AMP,            // &
    TOK_PIPE,           // |
    TOK_CARET,          // ^
    TOK_TILDE,          // ~
    TOK_EXCL,           // !
    TOK_LSHIFT,         // <<
    TOK_RSHIFT,         // >>
    TOK_EQ,             // ==
    TOK_NEQ,            // !=
    TOK_LT,             // <
    TOK_LTE,            // <=
    TOK_GT,             // >
    TOK_GTE,            // >=
    TOK_AND,            // &&
    TOK_OR,             // ||
    TOK_PLUS_ASSIGN,    // +=
    TOK_MINUS_ASSIGN,   // -=
    TOK_INC,            // ++
    TOK_DEC,            // --
    // Delimiters
    TOK_LPAREN,         // (
    TOK_RPAREN,         // )
    TOK_LBRACE,         // {
    TOK_RBRACE,         // }
    TOK_LBRACKET,       // [
    TOK_RBRACKET,       // ]
    TOK_COMMA,          // ,
    TOK_SEMICOLON       // ;
} TokenType;

typedef struct {
    TokenType type;
    char text[MAX_TOKEN_LEN];
    int int_val;
    int line;
    int col;
} Token;

// ============================================================================
// LEXER
// ============================================================================
typedef struct {
    const char *src;
    int pos;
    int line;
    int col;
} Lexer;

static void lexer_init(Lexer *l, const char *src) {
    l->src = src;
    l->pos = 0;
    l->line = 1;
    l->col = 1;
}

static char lexer_peek(Lexer *l) {
    return l->src[l->pos];
}

static char lexer_peek_ahead(Lexer *l, int offset) {
    int p = l->pos + offset;
    if (p < 0 || p >= (int)strlen(l->src)) return '\0';
    return l->src[p];
}

static char lexer_advance(Lexer *l) {
    char c = l->src[l->pos++];
    if (c == '\n') {
        l->line++;
        l->col = 1;
    } else {
        l->col++;
    }
    return c;
}

static void lexer_skip_whitespace_and_comments(Lexer *l) {
    while (lexer_peek(l) != '\0') {
        char c = lexer_peek(l);
        if (isspace(c)) {
            lexer_advance(l);
        } else if (c == '/' && lexer_peek_ahead(l, 1) == '/') {
            // Line comment
            lexer_advance(l);
            lexer_advance(l);
            while (lexer_peek(l) != '\0' && lexer_peek(l) != '\n') {
                lexer_advance(l);
            }
        } else if (c == '/' && lexer_peek_ahead(l, 1) == '*') {
            // Block comment
            lexer_advance(l);
            lexer_advance(l);
            while (lexer_peek(l) != '\0') {
                if (lexer_peek(l) == '*' && lexer_peek_ahead(l, 1) == '/') {
                    lexer_advance(l);
                    lexer_advance(l);
                    break;
                }
                lexer_advance(l);
            }
        } else if (c == '#') {
            // Preprocessor / shell style comment
            while (lexer_peek(l) != '\0' && lexer_peek(l) != '\n') {
                lexer_advance(l);
            }
        } else {
            break;
        }
    }
}

static Token lexer_next_token(Lexer *l) {
    lexer_skip_whitespace_and_comments(l);

    Token tok;
    memset(&tok, 0, sizeof(tok));
    tok.line = l->line;
    tok.col = l->col;

    char c = lexer_peek(l);
    if (c == '\0') {
        tok.type = TOK_EOF;
        strcpy(tok.text, "EOF");
        return tok;
    }

    // Number literals (hex 0x..., bin 0b..., decimal)
    if (isdigit(c) || (c == '0' && (lexer_peek_ahead(l, 1) == 'x' || lexer_peek_ahead(l, 1) == 'X' ||
                                    lexer_peek_ahead(l, 1) == 'b' || lexer_peek_ahead(l, 1) == 'B'))) {
        int i = 0;
        if (c == '0' && (lexer_peek_ahead(l, 1) == 'x' || lexer_peek_ahead(l, 1) == 'X')) {
            tok.text[i++] = lexer_advance(l);
            tok.text[i++] = lexer_advance(l);
            while (isxdigit(lexer_peek(l)) && i < MAX_TOKEN_LEN - 1) {
                tok.text[i++] = lexer_advance(l);
            }
            tok.text[i] = '\0';
            tok.int_val = (int)strtol(tok.text + 2, NULL, 16);
        } else if (c == '0' && (lexer_peek_ahead(l, 1) == 'b' || lexer_peek_ahead(l, 1) == 'B')) {
            tok.text[i++] = lexer_advance(l);
            tok.text[i++] = lexer_advance(l);
            while ((lexer_peek(l) == '0' || lexer_peek(l) == '1') && i < MAX_TOKEN_LEN - 1) {
                tok.text[i++] = lexer_advance(l);
            }
            tok.text[i] = '\0';
            tok.int_val = (int)strtol(tok.text + 2, NULL, 2);
        } else {
            while (isdigit(lexer_peek(l)) && i < MAX_TOKEN_LEN - 1) {
                tok.text[i++] = lexer_advance(l);
            }
            tok.text[i] = '\0';
            tok.int_val = atoi(tok.text);
        }
        tok.type = TOK_INT_LIT;
        return tok;
    }

    // Identifiers and Keywords
    if (isalpha(c) || c == '_') {
        int i = 0;
        while ((isalnum(lexer_peek(l)) || lexer_peek(l) == '_') && i < MAX_TOKEN_LEN - 1) {
            tok.text[i++] = lexer_advance(l);
        }
        tok.text[i] = '\0';

        // Check keywords
        if (strcmp(tok.text, "byte") == 0) tok.type = TOK_BYTE;
        else if (strcmp(tok.text, "int") == 0) tok.type = TOK_INT;
        else if (strcmp(tok.text, "void") == 0) tok.type = TOK_VOID;
        else if (strcmp(tok.text, "sprite") == 0) tok.type = TOK_SPRITE;
        else if (strcmp(tok.text, "if") == 0) tok.type = TOK_IF;
        else if (strcmp(tok.text, "else") == 0) tok.type = TOK_ELSE;
        else if (strcmp(tok.text, "while") == 0) tok.type = TOK_WHILE;
        else if (strcmp(tok.text, "for") == 0) tok.type = TOK_FOR;
        else if (strcmp(tok.text, "return") == 0) tok.type = TOK_RETURN;
        else if (strcmp(tok.text, "cls") == 0) tok.type = TOK_CLS;
        else if (strcmp(tok.text, "draw") == 0) tok.type = TOK_DRAW;
        else if (strcmp(tok.text, "draw_font") == 0) tok.type = TOK_DRAW_FONT;
        else if (strcmp(tok.text, "sync") == 0) tok.type = TOK_SYNC;
        else if (strcmp(tok.text, "key_down") == 0) tok.type = TOK_KEY_DOWN;
        else if (strcmp(tok.text, "wait_key") == 0) tok.type = TOK_WAIT_KEY;
        else if (strcmp(tok.text, "random") == 0) tok.type = TOK_RANDOM;
        else if (strcmp(tok.text, "sound_timer") == 0) tok.type = TOK_SOUND_TIMER;
        else if (strcmp(tok.text, "delay_timer") == 0) tok.type = TOK_DELAY_TIMER;
        else if (strcmp(tok.text, "collision") == 0) tok.type = TOK_COLLISION;
        else tok.type = TOK_IDENT;

        return tok;
    }

    // Two-character operators
    char next = lexer_peek_ahead(l, 1);
    if (c == '=' && next == '=') {
        lexer_advance(l); lexer_advance(l);
        tok.type = TOK_EQ; strcpy(tok.text, "=="); return tok;
    }
    if (c == '!' && next == '=') {
        lexer_advance(l); lexer_advance(l);
        tok.type = TOK_NEQ; strcpy(tok.text, "!="); return tok;
    }
    if (c == '<' && next == '=') {
        lexer_advance(l); lexer_advance(l);
        tok.type = TOK_LTE; strcpy(tok.text, "<="); return tok;
    }
    if (c == '>' && next == '=') {
        lexer_advance(l); lexer_advance(l);
        tok.type = TOK_GTE; strcpy(tok.text, ">="); return tok;
    }
    if (c == '<' && next == '<') {
        lexer_advance(l); lexer_advance(l);
        tok.type = TOK_LSHIFT; strcpy(tok.text, "<<"); return tok;
    }
    if (c == '>' && next == '>') {
        lexer_advance(l); lexer_advance(l);
        tok.type = TOK_RSHIFT; strcpy(tok.text, ">>"); return tok;
    }
    if (c == '+' && next == '+') {
        lexer_advance(l); lexer_advance(l);
        tok.type = TOK_INC; strcpy(tok.text, "++"); return tok;
    }
    if (c == '-' && next == '-') {
        lexer_advance(l); lexer_advance(l);
        tok.type = TOK_DEC; strcpy(tok.text, "--"); return tok;
    }
    if (c == '+' && next == '=') {
        lexer_advance(l); lexer_advance(l);
        tok.type = TOK_PLUS_ASSIGN; strcpy(tok.text, "+="); return tok;
    }
    if (c == '-' && next == '=') {
        lexer_advance(l); lexer_advance(l);
        tok.type = TOK_MINUS_ASSIGN; strcpy(tok.text, "-="); return tok;
    }
    if (c == '&' && next == '&') {
        lexer_advance(l); lexer_advance(l);
        tok.type = TOK_AND; strcpy(tok.text, "&&"); return tok;
    }
    if (c == '|' && next == '|') {
        lexer_advance(l); lexer_advance(l);
        tok.type = TOK_OR; strcpy(tok.text, "||"); return tok;
    }

    // Single character tokens
    tok.text[0] = lexer_advance(l);
    tok.text[1] = '\0';
    switch (c) {
        case '=': tok.type = TOK_ASSIGN; break;
        case '+': tok.type = TOK_PLUS; break;
        case '-': tok.type = TOK_MINUS; break;
        case '*': tok.type = TOK_STAR; break;
        case '/': tok.type = TOK_SLASH; break;
        case '%': tok.type = TOK_PERCENT; break;
        case '&': tok.type = TOK_AMP; break;
        case '|': tok.type = TOK_PIPE; break;
        case '^': tok.type = TOK_CARET; break;
        case '~': tok.type = TOK_TILDE; break;
        case '!': tok.type = TOK_EXCL; break;
        case '<': tok.type = TOK_LT; break;
        case '>': tok.type = TOK_GT; break;
        case '(': tok.type = TOK_LPAREN; break;
        case ')': tok.type = TOK_RPAREN; break;
        case '{': tok.type = TOK_LBRACE; break;
        case '}': tok.type = TOK_RBRACE; break;
        case '[': tok.type = TOK_LBRACKET; break;
        case ']': tok.type = TOK_RBRACKET; break;
        case ',': tok.type = TOK_COMMA; break;
        case ';': tok.type = TOK_SEMICOLON; break;
        default:
            fprintf(stderr, "Lexer error at line %d, col %d: Unknown character '%c'\n", tok.line, tok.col, c);
            exit(1);
    }
    return tok;
}

// ============================================================================
// ABSTRACT SYNTAX TREE (AST)
// ============================================================================
typedef enum {
    AST_PROGRAM,
    AST_VAR_DECL,
    AST_SPRITE_DECL,
    AST_FUNC_DECL,
    AST_BLOCK,
    AST_IF,
    AST_WHILE,
    AST_FOR,
    AST_RETURN,
    AST_EXPR_STMT,
    AST_ASSIGN,
    AST_BINARY_OP,
    AST_UNARY_OP,
    AST_VAR_REF,
    AST_INT_LIT,
    AST_CALL,
    AST_INTRINSIC
} ASTNodeType;

typedef struct ASTNode {
    ASTNodeType type;
    int line;

    // Attributes
    char name[MAX_TOKEN_LEN];
    int int_val;
    TokenType op;

    // For sprites
    uint8_t sprite_bytes[MAX_SPRITE_BYTES];
    int sprite_len;

    // Children nodes
    struct ASTNode *children[MAX_CHILDREN];
    int child_count;

    // For function declarations
    char param_names[MAX_PARAMS][MAX_TOKEN_LEN];
    int param_count;

    // Assigned register or memory address
    int reg_id;     // 2 to 14 (V2 - VE), or -1 if memory
    int mem_addr;   // e.g. 0x600 + offset
} ASTNode;

static ASTNode *ast_new(ASTNodeType type, int line) {
    ASTNode *node = (ASTNode *)calloc(1, sizeof(ASTNode));
    node->type = type;
    node->line = line;
    node->reg_id = -1;
    node->mem_addr = -1;
    return node;
}

static void ast_add_child(ASTNode *parent, ASTNode *child) {
    if (parent->child_count < MAX_CHILDREN) {
        parent->children[parent->child_count++] = child;
    } else {
        fprintf(stderr, "Error: AST child limit exceeded\n");
        exit(1);
    }
}

// ============================================================================
// PARSER
// ============================================================================
typedef struct {
    Lexer lexer;
    Token current;
} Parser;

static void parser_advance(Parser *p) {
    p->current = lexer_next_token(&p->lexer);
}

static bool parser_match(Parser *p, TokenType type) {
    if (p->current.type == type) {
        parser_advance(p);
        return true;
    }
    return false;
}

static void parser_expect(Parser *p, TokenType type, const char *msg) {
    if (p->current.type != type) {
        fprintf(stderr, "Syntax Error at line %d (col %d): %s (got '%s')\n",
                p->current.line, p->current.col, msg, p->current.text);
        exit(1);
    }
    parser_advance(p);
}

// Forward declarations for recursive descent
static ASTNode *parse_expression(Parser *p);
static ASTNode *parse_statement(Parser *p);
static ASTNode *parse_block(Parser *p);

// Expression parsing with precedence
static ASTNode *parse_primary(Parser *p) {
    Token tok = p->current;
    if (parser_match(p, TOK_INT_LIT)) {
        ASTNode *node = ast_new(AST_INT_LIT, tok.line);
        node->int_val = tok.int_val;
        return node;
    }

    if (parser_match(p, TOK_SOUND_TIMER)) {
        ASTNode *node = ast_new(AST_INTRINSIC, tok.line);
        node->op = TOK_SOUND_TIMER;
        strcpy(node->name, "sound_timer");
        return node;
    }

    if (parser_match(p, TOK_DELAY_TIMER)) {
        ASTNode *node = ast_new(AST_INTRINSIC, tok.line);
        node->op = TOK_DELAY_TIMER;
        strcpy(node->name, "delay_timer");
        return node;
    }

    if (parser_match(p, TOK_COLLISION)) {
        ASTNode *node = ast_new(AST_INTRINSIC, tok.line);
        node->op = TOK_COLLISION;
        strcpy(node->name, "collision");
        return node;
    }

    if (parser_match(p, TOK_KEY_DOWN)) {
        ASTNode *node = ast_new(AST_INTRINSIC, tok.line);
        node->op = TOK_KEY_DOWN;
        strcpy(node->name, "key_down");
        parser_expect(p, TOK_LPAREN, "Expected '(' after key_down");
        ast_add_child(node, parse_expression(p));
        parser_expect(p, TOK_RPAREN, "Expected ')' after key_down argument");
        return node;
    }

    if (parser_match(p, TOK_WAIT_KEY)) {
        ASTNode *node = ast_new(AST_INTRINSIC, tok.line);
        node->op = TOK_WAIT_KEY;
        strcpy(node->name, "wait_key");
        parser_expect(p, TOK_LPAREN, "Expected '(' after wait_key");
        parser_expect(p, TOK_RPAREN, "Expected ')' after wait_key()");
        return node;
    }

    if (parser_match(p, TOK_RANDOM)) {
        ASTNode *node = ast_new(AST_INTRINSIC, tok.line);
        node->op = TOK_RANDOM;
        strcpy(node->name, "random");
        parser_expect(p, TOK_LPAREN, "Expected '(' after random");
        ast_add_child(node, parse_expression(p));
        parser_expect(p, TOK_RPAREN, "Expected ')' after random argument");
        return node;
    }

    if (parser_match(p, TOK_IDENT)) {
        // Check for function call
        if (p->current.type == TOK_LPAREN) {
            parser_advance(p);
            ASTNode *call = ast_new(AST_CALL, tok.line);
            strcpy(call->name, tok.text);
            if (p->current.type != TOK_RPAREN) {
                while (1) {
                    ast_add_child(call, parse_expression(p));
                    if (parser_match(p, TOK_COMMA)) continue;
                    break;
                }
            }
            parser_expect(p, TOK_RPAREN, "Expected ')' after call arguments");
            return call;
        }

        // Variable reference
        ASTNode *node = ast_new(AST_VAR_REF, tok.line);
        strcpy(node->name, tok.text);
        return node;
    }

    if (parser_match(p, TOK_LPAREN)) {
        ASTNode *expr = parse_expression(p);
        parser_expect(p, TOK_RPAREN, "Expected ')' after parenthesized expression");
        return expr;
    }

    fprintf(stderr, "Syntax Error at line %d: Unexpected token '%s' in expression\n",
            p->current.line, p->current.text);
    exit(1);
}

static ASTNode *parse_unary(Parser *p) {
    if (p->current.type == TOK_EXCL || p->current.type == TOK_TILDE || p->current.type == TOK_MINUS) {
        Token tok = p->current;
        parser_advance(p);
        ASTNode *node = ast_new(AST_UNARY_OP, tok.line);
        node->op = tok.type;
        ast_add_child(node, parse_unary(p));
        return node;
    }
    return parse_primary(p);
}

static int get_binary_precedence(TokenType type) {
    switch (type) {
        case TOK_OR: return 1;
        case TOK_AND: return 2;
        case TOK_PIPE: return 3;
        case TOK_CARET: return 4;
        case TOK_AMP: return 5;
        case TOK_EQ:
        case TOK_NEQ: return 6;
        case TOK_LT:
        case TOK_LTE:
        case TOK_GT:
        case TOK_GTE: return 7;
        case TOK_LSHIFT:
        case TOK_RSHIFT: return 8;
        case TOK_PLUS:
        case TOK_MINUS: return 9;
        case TOK_STAR:
        case TOK_SLASH:
        case TOK_PERCENT: return 10;
        default: return 0;
    }
}

static ASTNode *parse_binary_rhs(Parser *p, int min_prec, ASTNode *lhs) {
    while (1) {
        TokenType op = p->current.type;
        int prec = get_binary_precedence(op);
        if (prec < min_prec) break;

        parser_advance(p);
        ASTNode *rhs = parse_unary(p);

        int next_prec = get_binary_precedence(p->current.type);
        if (prec < next_prec) {
            rhs = parse_binary_rhs(p, prec + 1, rhs);
        }

        ASTNode *bin = ast_new(AST_BINARY_OP, lhs->line);
        bin->op = op;
        ast_add_child(bin, lhs);
        ast_add_child(bin, rhs);
        lhs = bin;
    }
    return lhs;
}

static ASTNode *parse_assignment(Parser *p) {
    ASTNode *lhs = parse_unary(p);
    lhs = parse_binary_rhs(p, 1, lhs);

    if (p->current.type == TOK_ASSIGN || p->current.type == TOK_PLUS_ASSIGN || p->current.type == TOK_MINUS_ASSIGN) {
        Token op_tok = p->current;
        parser_advance(p);
        ASTNode *rhs = parse_assignment(p);

        ASTNode *assign = ast_new(AST_ASSIGN, op_tok.line);
        assign->op = op_tok.type;
        ast_add_child(assign, lhs);
        ast_add_child(assign, rhs);
        return assign;
    }

    return lhs;
}

static ASTNode *parse_expression(Parser *p) {
    return parse_assignment(p);
}

static ASTNode *parse_statement(Parser *p) {
    // Variable declaration: byte x = 10;
    if (p->current.type == TOK_BYTE || p->current.type == TOK_INT) {
        Token type_tok = p->current;
        parser_advance(p);

        Token name_tok = p->current;
        parser_expect(p, TOK_IDENT, "Expected variable name");

        ASTNode *decl = ast_new(AST_VAR_DECL, type_tok.line);
        strcpy(decl->name, name_tok.text);

        if (parser_match(p, TOK_ASSIGN)) {
            ast_add_child(decl, parse_expression(p));
        }
        parser_expect(p, TOK_SEMICOLON, "Expected ';' after variable declaration");
        return decl;
    }

    // Intrinsics as statements:
    // cls();
    if (parser_match(p, TOK_CLS)) {
        ASTNode *node = ast_new(AST_INTRINSIC, p->current.line);
        node->op = TOK_CLS;
        strcpy(node->name, "cls");
        parser_expect(p, TOK_LPAREN, "Expected '(' after cls");
        parser_expect(p, TOK_RPAREN, "Expected ')' after cls");
        parser_expect(p, TOK_SEMICOLON, "Expected ';' after cls()");
        return node;
    }

    // draw(x, y, sprite, height);
    if (parser_match(p, TOK_DRAW)) {
        ASTNode *node = ast_new(AST_INTRINSIC, p->current.line);
        node->op = TOK_DRAW;
        strcpy(node->name, "draw");
        parser_expect(p, TOK_LPAREN, "Expected '(' after draw");
        ast_add_child(node, parse_expression(p)); // x
        parser_expect(p, TOK_COMMA, "Expected ',' in draw()");
        ast_add_child(node, parse_expression(p)); // y
        parser_expect(p, TOK_COMMA, "Expected ',' in draw()");

        Token sprite_tok = p->current;
        parser_expect(p, TOK_IDENT, "Expected sprite name in draw()");
        ASTNode *snode = ast_new(AST_VAR_REF, sprite_tok.line);
        strcpy(snode->name, sprite_tok.text);
        ast_add_child(node, snode);

        parser_expect(p, TOK_COMMA, "Expected ',' in draw()");
        ast_add_child(node, parse_expression(p)); // height
        parser_expect(p, TOK_RPAREN, "Expected ')' after draw arguments");
        parser_expect(p, TOK_SEMICOLON, "Expected ';' after draw()");
        return node;
    }

    // draw_font(x, y, digit);
    if (parser_match(p, TOK_DRAW_FONT)) {
        ASTNode *node = ast_new(AST_INTRINSIC, p->current.line);
        node->op = TOK_DRAW_FONT;
        strcpy(node->name, "draw_font");
        parser_expect(p, TOK_LPAREN, "Expected '(' after draw_font");
        ast_add_child(node, parse_expression(p)); // x
        parser_expect(p, TOK_COMMA, "Expected ',' in draw_font()");
        ast_add_child(node, parse_expression(p)); // y
        parser_expect(p, TOK_COMMA, "Expected ',' in draw_font()");
        ast_add_child(node, parse_expression(p)); // digit
        parser_expect(p, TOK_RPAREN, "Expected ')' after draw_font arguments");
        parser_expect(p, TOK_SEMICOLON, "Expected ';' after draw_font()");
        return node;
    }

    // sync();
    if (parser_match(p, TOK_SYNC)) {
        ASTNode *node = ast_new(AST_INTRINSIC, p->current.line);
        node->op = TOK_SYNC;
        strcpy(node->name, "sync");
        parser_expect(p, TOK_LPAREN, "Expected '(' after sync");
        parser_expect(p, TOK_RPAREN, "Expected ')' after sync");
        parser_expect(p, TOK_SEMICOLON, "Expected ';' after sync()");
        return node;
    }

    // Block: { ... }
    if (p->current.type == TOK_LBRACE) {
        return parse_block(p);
    }

    // if (cond) stmt [else stmt]
    if (parser_match(p, TOK_IF)) {
        int line = p->current.line;
        parser_expect(p, TOK_LPAREN, "Expected '(' after 'if'");
        ASTNode *cond = parse_expression(p);
        parser_expect(p, TOK_RPAREN, "Expected ')' after if condition");

        ASTNode *then_branch = parse_statement(p);
        ASTNode *if_node = ast_new(AST_IF, line);
        ast_add_child(if_node, cond);
        ast_add_child(if_node, then_branch);

        if (parser_match(p, TOK_ELSE)) {
            ASTNode *else_branch = parse_statement(p);
            ast_add_child(if_node, else_branch);
        }
        return if_node;
    }

    // while (cond) stmt
    if (parser_match(p, TOK_WHILE)) {
        int line = p->current.line;
        parser_expect(p, TOK_LPAREN, "Expected '(' after 'while'");
        ASTNode *cond = parse_expression(p);
        parser_expect(p, TOK_RPAREN, "Expected ')' after while condition");

        ASTNode *body = parse_statement(p);
        ASTNode *while_node = ast_new(AST_WHILE, line);
        ast_add_child(while_node, cond);
        ast_add_child(while_node, body);
        return while_node;
    }

    // for (init; cond; step) stmt
    if (parser_match(p, TOK_FOR)) {
        int line = p->current.line;
        parser_expect(p, TOK_LPAREN, "Expected '(' after 'for'");

        ASTNode *init = NULL;
        if (p->current.type != TOK_SEMICOLON) {
            init = parse_statement(p); // parse_statement handles trailing ';'
        } else {
            parser_advance(p);
        }

        ASTNode *cond = NULL;
        if (p->current.type != TOK_SEMICOLON) {
            cond = parse_expression(p);
        }
        parser_expect(p, TOK_SEMICOLON, "Expected ';' after for condition");

        ASTNode *step = NULL;
        if (p->current.type != TOK_RPAREN) {
            step = parse_expression(p);
        }
        parser_expect(p, TOK_RPAREN, "Expected ')' after for clauses");

        ASTNode *body = parse_statement(p);

        ASTNode *for_node = ast_new(AST_FOR, line);
        ast_add_child(for_node, init ? init : ast_new(AST_BLOCK, line));
        ast_add_child(for_node, cond ? cond : ast_new(AST_INT_LIT, line));
        if (!cond) for_node->children[1]->int_val = 1; // default true
        ast_add_child(for_node, step ? step : ast_new(AST_BLOCK, line));
        ast_add_child(for_node, body);
        return for_node;
    }

    // return [expr];
    if (parser_match(p, TOK_RETURN)) {
        int line = p->current.line;
        ASTNode *ret_node = ast_new(AST_RETURN, line);
        if (p->current.type != TOK_SEMICOLON) {
            ast_add_child(ret_node, parse_expression(p));
        }
        parser_expect(p, TOK_SEMICOLON, "Expected ';' after return");
        return ret_node;
    }

    // Expression statement
    ASTNode *expr = parse_expression(p);
    parser_expect(p, TOK_SEMICOLON, "Expected ';' after expression");
    ASTNode *stmt = ast_new(AST_EXPR_STMT, expr->line);
    ast_add_child(stmt, expr);
    return stmt;
}

static ASTNode *parse_block(Parser *p) {
    int line = p->current.line;
    parser_expect(p, TOK_LBRACE, "Expected '{'");
    ASTNode *block = ast_new(AST_BLOCK, line);

    while (p->current.type != TOK_RBRACE && p->current.type != TOK_EOF) {
        ast_add_child(block, parse_statement(p));
    }

    parser_expect(p, TOK_RBRACE, "Expected '}'");
    return block;
}

// Global declarations: sprite, function, global variable
static ASTNode *parse_global_decl(Parser *p) {
    // Sprite declaration: sprite name = [ 0x18, 0x3C, ... ];
    if (parser_match(p, TOK_SPRITE)) {
        Token name_tok = p->current;
        parser_expect(p, TOK_IDENT, "Expected sprite identifier");

        ASTNode *snode = ast_new(AST_SPRITE_DECL, name_tok.line);
        strcpy(snode->name, name_tok.text);

        parser_expect(p, TOK_ASSIGN, "Expected '=' in sprite declaration");
        parser_expect(p, TOK_LBRACKET, "Expected '[' for sprite byte list");

        while (p->current.type != TOK_RBRACKET && p->current.type != TOK_EOF) {
            Token byte_tok = p->current;
            parser_expect(p, TOK_INT_LIT, "Expected byte literal in sprite array");
            if (snode->sprite_len < MAX_SPRITE_BYTES) {
                snode->sprite_bytes[snode->sprite_len++] = (uint8_t)byte_tok.int_val;
            }
            if (parser_match(p, TOK_COMMA)) continue;
            break;
        }

        parser_expect(p, TOK_RBRACKET, "Expected ']' after sprite bytes");
        parser_expect(p, TOK_SEMICOLON, "Expected ';' after sprite declaration");
        return snode;
    }

    // Function declaration: void func(byte a, byte b) { ... }
    if (p->current.type == TOK_VOID || p->current.type == TOK_BYTE || p->current.type == TOK_INT) {
        Token ret_type = p->current;
        parser_advance(p);

        Token name_tok = p->current;
        parser_expect(p, TOK_IDENT, "Expected function or variable name");

        // Check if it's a function or global variable
        if (p->current.type == TOK_LPAREN) {
            parser_advance(p);
            ASTNode *func = ast_new(AST_FUNC_DECL, name_tok.line);
            strcpy(func->name, name_tok.text);

            if (p->current.type != TOK_RPAREN) {
                while (1) {
                    if (p->current.type == TOK_BYTE || p->current.type == TOK_INT) {
                        parser_advance(p);
                    }
                    Token ptok = p->current;
                    parser_expect(p, TOK_IDENT, "Expected parameter name");
                    if (func->param_count < MAX_PARAMS) {
                        strcpy(func->param_names[func->param_count++], ptok.text);
                    }
                    if (parser_match(p, TOK_COMMA)) continue;
                    break;
                }
            }
            parser_expect(p, TOK_RPAREN, "Expected ')' after parameter list");

            ASTNode *body = parse_block(p);
            ast_add_child(func, body);
            return func;
        } else {
            // Global variable declaration
            ASTNode *var = ast_new(AST_VAR_DECL, ret_type.line);
            strcpy(var->name, name_tok.text);
            if (parser_match(p, TOK_ASSIGN)) {
                ast_add_child(var, parse_expression(p));
            }
            parser_expect(p, TOK_SEMICOLON, "Expected ';' after global variable");
            return var;
        }
    }

    fprintf(stderr, "Syntax Error at line %d: Expected top-level declaration (got '%s')\n",
            p->current.line, p->current.text);
    exit(1);
}

static ASTNode *parse_program(Parser *p) {
    ASTNode *prog = ast_new(AST_PROGRAM, 1);
    while (p->current.type != TOK_EOF) {
        ast_add_child(prog, parse_global_decl(p));
    }
    return prog;
}

// ============================================================================
// SYMBOL TABLE & REGISTER ALLOCATION
// ============================================================================
typedef struct {
    char name[MAX_TOKEN_LEN];
    int reg_id;     // 2 to 14 (V2 - VE), or -1 if memory
    int mem_addr;   // for globals/spills
    bool is_param;
} Symbol;

typedef struct {
    Symbol locals[MAX_LOCALS];
    int local_count;
    Symbol globals[MAX_GLOBALS];
    int global_count;
    int next_mem_addr;
    int next_reg; // 2..14
} SymbolTable;

static void symtab_init(SymbolTable *st) {
    memset(st, 0, sizeof(SymbolTable));
    st->next_mem_addr = 0x500; // CHIP-8 variables memory pool
    st->next_reg = 2;          // V2..VE
}

static void symtab_enter_func(SymbolTable *st) {
    st->local_count = 0;
    st->next_reg = 2; // Reset local register pool for function
}

static Symbol *symtab_add_global(SymbolTable *st, const char *name) {
    if (st->global_count >= MAX_GLOBALS) {
        fprintf(stderr, "Error: Maximum globals exceeded\n");
        exit(1);
    }
    Symbol *s = &st->globals[st->global_count++];
    strcpy(s->name, name);
    s->reg_id = -1;
    s->mem_addr = st->next_mem_addr++;
    s->is_param = false;
    return s;
}

static Symbol *symtab_add_local(SymbolTable *st, const char *name, bool is_param) {
    if (st->local_count >= MAX_LOCALS) {
        fprintf(stderr, "Error: Maximum locals exceeded in function\n");
        exit(1);
    }
    Symbol *s = &st->locals[st->local_count++];
    strcpy(s->name, name);
    s->is_param = is_param;

    if (st->next_reg <= 14) {
        s->reg_id = st->next_reg++;
        s->mem_addr = -1;
    } else {
        // Spill to memory
        s->reg_id = -1;
        s->mem_addr = st->next_mem_addr++;
    }
    return s;
}

static Symbol *symtab_lookup(SymbolTable *st, const char *name) {
    for (int i = 0; i < st->local_count; i++) {
        if (strcmp(st->locals[i].name, name) == 0) return &st->locals[i];
    }
    for (int i = 0; i < st->global_count; i++) {
        if (strcmp(st->globals[i].name, name) == 0) return &st->globals[i];
    }
    return NULL;
}

// ============================================================================
// CODE GENERATOR
// ============================================================================
typedef struct {
    FILE *out;
    SymbolTable symtab;
    int label_seq;
    bool has_main;
} CodeGen;

static int codegen_new_label(CodeGen *cg) {
    return ++cg->label_seq;
}

// Evaluate expression and place result in CHIP-8 register target_reg (usually V0 or V1)
static void codegen_expr(CodeGen *cg, ASTNode *node, int target_reg) {
    if (!node) return;

    if (node->type == AST_INT_LIT) {
        fprintf(cg->out, "    v%X = 0x%02X\n", target_reg, node->int_val & 0xFF);
        return;
    }

    if (node->type == AST_VAR_REF) {
        Symbol *sym = symtab_lookup(&cg->symtab, node->name);
        if (!sym) {
            // Could be a sprite address
            fprintf(cg->out, "    i = %s\n", node->name);
            return;
        }
        if (sym->reg_id != -1) {
            if (sym->reg_id != target_reg) {
                fprintf(cg->out, "    v%X = v%X\n", target_reg, sym->reg_id);
            }
        } else {
            // Load from memory into target_reg safely via V0
            fprintf(cg->out, "    i = 0x%03X\n", sym->mem_addr);
            fprintf(cg->out, "    load v0\n");
            if (target_reg != 0) {
                fprintf(cg->out, "    v%X = v0\n", target_reg);
            }
        }
        return;
    }

    if (node->type == AST_INTRINSIC) {
        if (node->op == TOK_SOUND_TIMER) {
            fprintf(cg->out, "    v%X = delay\n", target_reg); // CHIP-8 delay read
        } else if (node->op == TOK_DELAY_TIMER) {
            fprintf(cg->out, "    v%X = delay\n", target_reg);
        } else if (node->op == TOK_COLLISION) {
            fprintf(cg->out, "    v%X = vF\n", target_reg);
        } else if (node->op == TOK_WAIT_KEY) {
            fprintf(cg->out, "    v%X = key\n", target_reg);
        } else if (node->op == TOK_RANDOM) {
            int mask = 0xFF;
            if (node->child_count > 0 && node->children[0]->type == AST_INT_LIT) {
                mask = node->children[0]->int_val & 0xFF;
            }
            fprintf(cg->out, "    v%X = rand & 0x%02X\n", target_reg, mask);
        } else if (node->op == TOK_KEY_DOWN) {
            codegen_expr(cg, node->children[0], target_reg);
            int skip_lbl = codegen_new_label(cg);
            fprintf(cg->out, "    skip_key v%X\n", target_reg);
            fprintf(cg->out, "    jump _Lnot_pressed_%d\n", skip_lbl);
            fprintf(cg->out, "    v%X = 1\n", target_reg);
            fprintf(cg->out, "    jump _Lend_key_%d\n", skip_lbl);
            fprintf(cg->out, "_Lnot_pressed_%d:\n", skip_lbl);
            fprintf(cg->out, "    v%X = 0\n", target_reg);
            fprintf(cg->out, "_Lend_key_%d:\n", skip_lbl);
        }
        return;
    }

    if (node->type == AST_CALL) {
        // Evaluate arguments and assign to function parameter registers
        for (int i = 0; i < node->child_count; i++) {
            codegen_expr(cg, node->children[i], 2 + i); // V2, V3...
        }
        fprintf(cg->out, "    call %s\n", node->name);
        if (target_reg != 0) {
            fprintf(cg->out, "    v%X = v0\n", target_reg);
        }
        return;
    }

    if (node->type == AST_ASSIGN) {
        ASTNode *lhs = node->children[0];
        ASTNode *rhs = node->children[1];

        // Evaluate RHS into target_reg
        codegen_expr(cg, rhs, target_reg);

        if (lhs->type == AST_INTRINSIC) {
            if (lhs->op == TOK_SOUND_TIMER) {
                fprintf(cg->out, "    sound = v%X\n", target_reg);
            } else if (lhs->op == TOK_DELAY_TIMER) {
                fprintf(cg->out, "    delay = v%X\n", target_reg);
            }
            return;
        }

        Symbol *sym = symtab_lookup(&cg->symtab, lhs->name);
        if (!sym) {
            fprintf(stderr, "Error at line %d: Undefined variable '%s'\n", lhs->line, lhs->name);
            exit(1);
        }

        if (node->op == TOK_PLUS_ASSIGN) {
            if (sym->reg_id != -1) {
                fprintf(cg->out, "    v%X += v%X\n", sym->reg_id, target_reg);
            } else {
                // Global in memory: load, add, save
                fprintf(cg->out, "    i = 0x%03X\n", sym->mem_addr);
                fprintf(cg->out, "    load v0\n");
                fprintf(cg->out, "    v0 += v%X\n", target_reg);
                fprintf(cg->out, "    i = 0x%03X\n", sym->mem_addr);
                fprintf(cg->out, "    save v0\n");
            }
        } else if (node->op == TOK_MINUS_ASSIGN) {
            if (sym->reg_id != -1) {
                fprintf(cg->out, "    v%X -= v%X\n", sym->reg_id, target_reg);
            } else {
                // Global in memory: load, sub, save
                fprintf(cg->out, "    i = 0x%03X\n", sym->mem_addr);
                fprintf(cg->out, "    load v0\n");
                fprintf(cg->out, "    v0 -= v%X\n", target_reg);
                fprintf(cg->out, "    i = 0x%03X\n", sym->mem_addr);
                fprintf(cg->out, "    save v0\n");
            }
        } else {
            // Normal assignment '='
            if (sym->reg_id != -1) {
                fprintf(cg->out, "    v%X = v%X\n", sym->reg_id, target_reg);
            } else {
                if (target_reg != 0) {
                    fprintf(cg->out, "    v0 = v%X\n", target_reg);
                }
                fprintf(cg->out, "    i = 0x%03X\n", sym->mem_addr);
                fprintf(cg->out, "    save v0\n");
            }
        }
        return;
    }

    if (node->type == AST_UNARY_OP) {
        codegen_expr(cg, node->children[0], target_reg);
        if (node->op == TOK_EXCL) {
            // Logical NOT: if val == 0 -> 1, else 0
            int lbl = codegen_new_label(cg);
            fprintf(cg->out, "    if v%X == 0\n", target_reg);
            fprintf(cg->out, "    jump _Lnot_true_%d\n", lbl);
            fprintf(cg->out, "    v%X = 0\n", target_reg);
            fprintf(cg->out, "    jump _Lnot_end_%d\n", lbl);
            fprintf(cg->out, "_Lnot_true_%d:\n", lbl);
            fprintf(cg->out, "    v%X = 1\n", target_reg);
            fprintf(cg->out, "_Lnot_end_%d:\n", lbl);
        }
        return;
    }

    if (node->type == AST_BINARY_OP) {
        ASTNode *lhs = node->children[0];
        ASTNode *rhs = node->children[1];

        // Evaluate LHS into target_reg
        codegen_expr(cg, lhs, target_reg);

        // Evaluate RHS into scratch register (V1)
        int scratch_reg = (target_reg == 0) ? 1 : 0;
        codegen_expr(cg, rhs, scratch_reg);

        switch (node->op) {
            case TOK_PLUS:
                fprintf(cg->out, "    v%X += v%X\n", target_reg, scratch_reg);
                break;
            case TOK_MINUS:
                fprintf(cg->out, "    v%X -= v%X\n", target_reg, scratch_reg);
                break;
            case TOK_AMP:
                fprintf(cg->out, "    v%X &= v%X\n", target_reg, scratch_reg);
                break;
            case TOK_PIPE:
                fprintf(cg->out, "    v%X |= v%X\n", target_reg, scratch_reg);
                break;
            case TOK_CARET:
                fprintf(cg->out, "    v%X ^= v%X\n", target_reg, scratch_reg);
                break;
            case TOK_LSHIFT:
                fprintf(cg->out, "    v%X <<= 1\n", target_reg);
                break;
            case TOK_RSHIFT:
                fprintf(cg->out, "    v%X >>= 1\n", target_reg);
                break;
            case TOK_EQ: {
                int lbl = codegen_new_label(cg);
                fprintf(cg->out, "    if v%X == v%X\n", target_reg, scratch_reg);
                fprintf(cg->out, "    jump _Leq_true_%d\n", lbl);
                fprintf(cg->out, "    v%X = 0\n", target_reg);
                fprintf(cg->out, "    jump _Leq_end_%d\n", lbl);
                fprintf(cg->out, "_Leq_true_%d:\n", lbl);
                fprintf(cg->out, "    v%X = 1\n", target_reg);
                fprintf(cg->out, "_Leq_end_%d:\n", lbl);
                break;
            }
            case TOK_NEQ: {
                int lbl = codegen_new_label(cg);
                fprintf(cg->out, "    if v%X != v%X\n", target_reg, scratch_reg);
                fprintf(cg->out, "    jump _Lneq_true_%d\n", lbl);
                fprintf(cg->out, "    v%X = 0\n", target_reg);
                fprintf(cg->out, "    jump _Lneq_end_%d\n", lbl);
                fprintf(cg->out, "_Lneq_true_%d:\n", lbl);
                fprintf(cg->out, "    v%X = 1\n", target_reg);
                fprintf(cg->out, "_Lneq_end_%d:\n", lbl);
                break;
            }
            case TOK_LT: {
                int lbl = codegen_new_label(cg);
                fprintf(cg->out, "    if v%X == v%X\n", target_reg, scratch_reg);
                fprintf(cg->out, "    jump _Llt_false_%d\n", lbl);
                fprintf(cg->out, "    v%X -= v%X\n", scratch_reg, target_reg);
                fprintf(cg->out, "    if vF == 1\n");
                fprintf(cg->out, "    jump _Llt_true_%d\n", lbl);
                fprintf(cg->out, "_Llt_false_%d:\n", lbl);
                fprintf(cg->out, "    v%X = 0\n", target_reg);
                fprintf(cg->out, "    jump _Llt_end_%d\n", lbl);
                fprintf(cg->out, "_Llt_true_%d:\n", lbl);
                fprintf(cg->out, "    v%X = 1\n", target_reg);
                fprintf(cg->out, "_Llt_end_%d:\n", lbl);
                break;
            }
            case TOK_LTE: {
                int lbl = codegen_new_label(cg);
                fprintf(cg->out, "    v%X -= v%X\n", scratch_reg, target_reg);
                fprintf(cg->out, "    if vF == 1\n");
                fprintf(cg->out, "    jump _Llte_true_%d\n", lbl);
                fprintf(cg->out, "    v%X = 0\n", target_reg);
                fprintf(cg->out, "    jump _Llte_end_%d\n", lbl);
                fprintf(cg->out, "_Llte_true_%d:\n", lbl);
                fprintf(cg->out, "    v%X = 1\n", target_reg);
                fprintf(cg->out, "_Llte_end_%d:\n", lbl);
                break;
            }
            case TOK_GT: {
                int lbl = codegen_new_label(cg);
                fprintf(cg->out, "    if v%X == v%X\n", target_reg, scratch_reg);
                fprintf(cg->out, "    jump _Lgt_false_%d\n", lbl);
                fprintf(cg->out, "    v%X -= v%X\n", target_reg, scratch_reg);
                fprintf(cg->out, "    if vF == 1\n");
                fprintf(cg->out, "    jump _Lgt_true_%d\n", lbl);
                fprintf(cg->out, "_Lgt_false_%d:\n", lbl);
                fprintf(cg->out, "    v%X = 0\n", target_reg);
                fprintf(cg->out, "    jump _Lgt_end_%d\n", lbl);
                fprintf(cg->out, "_Lgt_true_%d:\n", lbl);
                fprintf(cg->out, "    v%X = 1\n", target_reg);
                fprintf(cg->out, "_Lgt_end_%d:\n", lbl);
                break;
            }
            case TOK_GTE: {
                int lbl = codegen_new_label(cg);
                fprintf(cg->out, "    v%X -= v%X\n", target_reg, scratch_reg);
                fprintf(cg->out, "    if vF == 1\n");
                fprintf(cg->out, "    jump _Lgte_true_%d\n", lbl);
                fprintf(cg->out, "    v%X = 0\n", target_reg);
                fprintf(cg->out, "    jump _Lgte_end_%d\n", lbl);
                fprintf(cg->out, "_Lgte_true_%d:\n", lbl);
                fprintf(cg->out, "    v%X = 1\n", target_reg);
                fprintf(cg->out, "_Lgte_end_%d:\n", lbl);
                break;
            }
            case TOK_AND: {
                int lbl = codegen_new_label(cg);
                fprintf(cg->out, "    if v%X == 0\n", target_reg);
                fprintf(cg->out, "    jump _Land_false_%d\n", lbl);
                fprintf(cg->out, "    if v%X == 0\n", scratch_reg);
                fprintf(cg->out, "    jump _Land_false_%d\n", lbl);
                fprintf(cg->out, "    v%X = 1\n", target_reg);
                fprintf(cg->out, "    jump _Land_end_%d\n", lbl);
                fprintf(cg->out, "_Land_false_%d:\n", lbl);
                fprintf(cg->out, "    v%X = 0\n", target_reg);
                fprintf(cg->out, "_Land_end_%d:\n", lbl);
                break;
            }
            case TOK_OR: {
                int lbl = codegen_new_label(cg);
                fprintf(cg->out, "    if v%X != 0\n", target_reg);
                fprintf(cg->out, "    jump _Lor_true_%d\n", lbl);
                fprintf(cg->out, "    if v%X != 0\n", scratch_reg);
                fprintf(cg->out, "    jump _Lor_true_%d\n", lbl);
                fprintf(cg->out, "    v%X = 0\n", target_reg);
                fprintf(cg->out, "    jump _Lor_end_%d\n", lbl);
                fprintf(cg->out, "_Lor_true_%d:\n", lbl);
                fprintf(cg->out, "    v%X = 1\n", target_reg);
                fprintf(cg->out, "_Lor_end_%d:\n", lbl);
                break;
            }
            default:
                break;
        }
        return;
    }
}

static void codegen_stmt(CodeGen *cg, ASTNode *node) {
    if (!node) return;

    switch (node->type) {
        case AST_VAR_DECL: {
            Symbol *sym = symtab_add_local(&cg->symtab, node->name, false);
            if (node->child_count > 0) {
                codegen_expr(cg, node->children[0], 0);
                if (sym->reg_id != -1) {
                    fprintf(cg->out, "    v%X = v0\n", sym->reg_id);
                } else {
                    fprintf(cg->out, "    i = 0x%03X\n", sym->mem_addr);
                    fprintf(cg->out, "    save v0\n");
                }
            }
            break;
        }

        case AST_BLOCK: {
            for (int i = 0; i < node->child_count; i++) {
                codegen_stmt(cg, node->children[i]);
            }
            break;
        }

        case AST_EXPR_STMT: {
            codegen_expr(cg, node->children[0], 0);
            break;
        }

        case AST_INTRINSIC: {
            if (node->op == TOK_CLS) {
                fprintf(cg->out, "    cls\n");
            } else if (node->op == TOK_DRAW) {
                // draw(x, y, sprite, height)
                codegen_expr(cg, node->children[0], 0); // x -> v0
                codegen_expr(cg, node->children[1], 1); // y -> v1
                fprintf(cg->out, "    i = %s\n", node->children[2]->name);

                int height = 5;
                if (node->children[3]->type == AST_INT_LIT) {
                    height = node->children[3]->int_val;
                }
                fprintf(cg->out, "    draw v0, v1, %d\n", height);
            } else if (node->op == TOK_DRAW_FONT) {
                // draw_font(x, y, digit)
                codegen_expr(cg, node->children[0], 0); // x
                codegen_expr(cg, node->children[1], 1); // y
                codegen_expr(cg, node->children[2], 2); // digit -> v2
                fprintf(cg->out, "    i = font v2\n");
                fprintf(cg->out, "    draw v0, v1, 5\n");
            } else if (node->op == TOK_SYNC) {
                int sync_lbl = codegen_new_label(cg);
                fprintf(cg->out, "_Lsync_wait_%d:\n", sync_lbl);
                fprintf(cg->out, "    v0 = delay\n");
                fprintf(cg->out, "    if v0 != 0\n");
                fprintf(cg->out, "    jump _Lsync_wait_%d\n", sync_lbl);
                fprintf(cg->out, "    v0 = 2\n");
                fprintf(cg->out, "    delay = v0\n");
            }
            break;
        }

        case AST_IF: {
            int lbl = codegen_new_label(cg);
            // Evaluate condition
            codegen_expr(cg, node->children[0], 0);
            fprintf(cg->out, "    if v0 == 0\n");
            fprintf(cg->out, "    jump _Lelse_%d\n", lbl);

            // Then branch
            codegen_stmt(cg, node->children[1]);

            if (node->child_count > 2) {
                fprintf(cg->out, "    jump _Lif_end_%d\n", lbl);
                fprintf(cg->out, "_Lelse_%d:\n", lbl);
                codegen_stmt(cg, node->children[2]);
                fprintf(cg->out, "_Lif_end_%d:\n", lbl);
            } else {
                fprintf(cg->out, "_Lelse_%d:\n", lbl);
            }
            break;
        }

        case AST_WHILE: {
            int start_lbl = codegen_new_label(cg);
            int end_lbl = codegen_new_label(cg);

            fprintf(cg->out, "_Lwhile_start_%d:\n", start_lbl);
            codegen_expr(cg, node->children[0], 0);
            fprintf(cg->out, "    if v0 == 0\n");
            fprintf(cg->out, "    jump _Lwhile_end_%d\n", end_lbl);

            codegen_stmt(cg, node->children[1]);
            fprintf(cg->out, "    jump _Lwhile_start_%d\n", start_lbl);
            fprintf(cg->out, "_Lwhile_end_%d:\n", end_lbl);
            break;
        }

        case AST_FOR: {
            int start_lbl = codegen_new_label(cg);
            int end_lbl = codegen_new_label(cg);

            // Init
            codegen_stmt(cg, node->children[0]);

            fprintf(cg->out, "_Lfor_start_%d:\n", start_lbl);
            codegen_expr(cg, node->children[1], 0); // Condition
            fprintf(cg->out, "    if v0 == 0\n");
            fprintf(cg->out, "    jump _Lfor_end_%d\n", end_lbl);

            // Body
            codegen_stmt(cg, node->children[3]);

            // Step
            codegen_expr(cg, node->children[2], 0);

            fprintf(cg->out, "    jump _Lfor_start_%d\n", start_lbl);
            fprintf(cg->out, "_Lfor_end_%d:\n", end_lbl);
            break;
        }

        case AST_RETURN: {
            if (node->child_count > 0) {
                codegen_expr(cg, node->children[0], 0);
            }
            fprintf(cg->out, "    ret\n");
            break;
        }

        default:
            break;
    }
}

static void codegen_func(CodeGen *cg, ASTNode *node) {
    symtab_enter_func(&cg->symtab);

    fprintf(cg->out, "\n; --- Function: %s ---\n", node->name);
    fprintf(cg->out, "%s:\n", node->name);

    // Register parameters
    for (int i = 0; i < node->param_count; i++) {
        symtab_add_local(&cg->symtab, node->param_names[i], true);
    }

    codegen_stmt(cg, node->children[0]); // Body block
    fprintf(cg->out, "    ret\n");
}

static void codegen_program(CodeGen *cg, ASTNode *prog) {
    fprintf(cg->out, "; =========================================\n");
    fprintf(cg->out, "; Byte8 Assembly generated by b8c compiler\n");
    fprintf(cg->out, "; =========================================\n\n");

    // First scan for global variables and initialize entry point
    fprintf(cg->out, "start:\n");
    fprintf(cg->out, "    jump _init_globals\n\n");

    // Global variable initializations
    fprintf(cg->out, "_init_globals:\n");
    for (int i = 0; i < prog->child_count; i++) {
        ASTNode *child = prog->children[i];
        if (child->type == AST_VAR_DECL) {
            Symbol *sym = symtab_add_global(&cg->symtab, child->name);
            if (child->child_count > 0) {
                codegen_expr(cg, child->children[0], 0);
                fprintf(cg->out, "    i = 0x%03X\n", sym->mem_addr);
                fprintf(cg->out, "    save v0\n");
            }
        }
    }
    fprintf(cg->out, "    call main\n");
    fprintf(cg->out, "_infinite_loop:\n");
    fprintf(cg->out, "    jump _infinite_loop\n");

    // Functions
    for (int i = 0; i < prog->child_count; i++) {
        ASTNode *child = prog->children[i];
        if (child->type == AST_FUNC_DECL) {
            if (strcmp(child->name, "main") == 0) cg->has_main = true;
            codegen_func(cg, child);
        }
    }

    if (!cg->has_main) {
        fprintf(stderr, "Error: 'main()' function not found in source file\n");
        exit(1);
    }

    // Sprites data section
    fprintf(cg->out, "\n; --- Sprites Section ---\n");
    for (int i = 0; i < prog->child_count; i++) {
        ASTNode *child = prog->children[i];
        if (child->type == AST_SPRITE_DECL) {
            fprintf(cg->out, "%s:\n", child->name);
            for (int b = 0; b < child->sprite_len; b++) {
                fprintf(cg->out, "    db 0x%02X\n", child->sprite_bytes[b]);
            }
        }
    }
}

// AST Tree dumper for --dump-ast
static void dump_ast(ASTNode *node, int indent) {
    if (!node) return;
    for (int i = 0; i < indent; i++) printf("  ");

    const char *type_names[] = {
        "PROGRAM", "VAR_DECL", "SPRITE_DECL", "FUNC_DECL",
        "BLOCK", "IF", "WHILE", "FOR", "RETURN", "EXPR_STMT",
        "ASSIGN", "BINARY_OP", "UNARY_OP", "VAR_REF", "INT_LIT",
        "CALL", "INTRINSIC"
    };
    printf("[%s] name='%s' val=%d\n", type_names[node->type], node->name, node->int_val);
    for (int i = 0; i < node->child_count; i++) {
        dump_ast(node->children[i], indent + 1);
    }
}

// ============================================================================
// MAIN DRIVER & CLI
// ============================================================================
static void print_usage(const char *prog) {
    printf("Byte8 High-Level C Compiler (b8c)\n");
    printf("Usage: %s <source.b8> [options]\n\n", prog);
    printf("Options:\n");
    printf("  -o <file>       Output file (default: <basename>.ch8, or <basename>.txt with -S)\n");
    printf("  -S              Output Byte8 symbolic assembly (.txt) instead of binary\n");
    printf("  --dump-ast      Print the parsed Abstract Syntax Tree\n");
    printf("  -v, --verbose   Verbose output during compilation\n");
    printf("  -h, --help      Display this help message\n");
}

int main(int argc, char **argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }

    const char *src_file = NULL;
    const char *out_file = NULL;
    bool emit_asm = false;
    bool dump_ast_flag = false;
    bool verbose = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "-S") == 0) {
            emit_asm = true;
        } else if (strcmp(argv[i], "--dump-ast") == 0) {
            dump_ast_flag = true;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            verbose = true;
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out_file = argv[++i];
        } else if (argv[i][0] != '-') {
            src_file = argv[i];
        } else {
            fprintf(stderr, "Unknown option '%s'\n", argv[i]);
            return 1;
        }
    }

    if (!src_file) {
        fprintf(stderr, "Error: No input .b8 source file specified\n");
        return 1;
    }

    // Read source file into memory
    FILE *f = fopen(src_file, "rb");
    if (!f) {
        perror("Error opening source file");
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *source_code = (char *)malloc(fsize + 2);
    size_t read_bytes = fread(source_code, 1, fsize, f);
    source_code[read_bytes] = '\0';
    fclose(f);

    if (verbose) {
        printf("[b8c] Lexing & Parsing '%s' (%ld bytes)...\n", src_file, fsize);
    }

    Parser parser;
    lexer_init(&parser.lexer, source_code);
    parser_advance(&parser);

    ASTNode *root = parse_program(&parser);

    if (dump_ast_flag) {
        printf("\n=== Abstract Syntax Tree ===\n");
        dump_ast(root, 0);
        printf("============================\n\n");
    }

    // Determine output file names
    char asm_path[512];
    char bin_path[512];

    char base_name[256];
    strncpy(base_name, src_file, sizeof(base_name) - 1);
    char *dot = strrchr(base_name, '.');
    if (dot) *dot = '\0';

    const char *slash = strrchr(base_name, '/');
    const char *pure_name = slash ? slash + 1 : base_name;

    if (emit_asm) {
        if (out_file) {
            snprintf(asm_path, sizeof(asm_path), "%s", out_file);
        } else {
            snprintf(asm_path, sizeof(asm_path), "%s.txt", base_name);
        }
    } else {
        snprintf(asm_path, sizeof(asm_path), "/tmp/%s_tmp.txt", pure_name);
        if (out_file) {
            snprintf(bin_path, sizeof(bin_path), "%s", out_file);
        } else {
            snprintf(bin_path, sizeof(bin_path), "%s.ch8", base_name);
        }
    }

    FILE *out = fopen(asm_path, "w");
    if (!out) {
        perror("Error opening assembly output file");
        free(source_code);
        return 1;
    }

    CodeGen cg;
    memset(&cg, 0, sizeof(cg));
    cg.out = out;
    symtab_init(&cg.symtab);

    codegen_program(&cg, root);
    fclose(out);

    if (verbose) {
        printf("[b8c] Emitted Byte8 assembly to: %s\n", asm_path);
    }

    if (emit_asm) {
        printf("Successfully compiled '%s' -> '%s' (Assembly)\n", src_file, asm_path);
    } else {
        // Run Byte8 assembler to produce .ch8 binary
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "./compiler \"%s\" \"%s\" > /dev/null", asm_path, bin_path);
        int ret = system(cmd);
        if (ret != 0) {
            fprintf(stderr, "Error: Assembler failed to assemble generated assembly code\n");
            free(source_code);
            return 1;
        }
        remove(asm_path); // clean up intermediate
        printf("Successfully compiled '%s' -> '%s' (CHIP-8 ROM)\n", src_file, bin_path);
    }

    free(source_code);
    return 0;
}
