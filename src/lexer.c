#include "lexer.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

Lexer *lexer_new(const char *source) {
    Lexer *l = malloc(sizeof(Lexer));
    l->src  = strdup(source);
    l->pos  = 0;
    l->line = 1;
    l->col  = 1;
    return l;
}

void lexer_free(Lexer *l) {
    if (!l) return;
    free(l->src);
    free(l);
}

void token_free(Token *t) {
    if (!t) return;
    free(t->value);
    free(t);
}

static Token *make_token(TokenType type, const char *val, int line, int col) {
    Token *t = malloc(sizeof(Token));
    t->type  = type;
    t->value = strdup(val ? val : "");
    t->line  = line;
    t->col   = col;
    return t;
}

static char cur(Lexer *l) { return l->src[l->pos]; }
static char peek(Lexer *l) { return l->src[l->pos + 1]; }
static char peek2(Lexer *l) { return l->src[l->pos + 2]; }

static void adv(Lexer *l) {
    if (l->src[l->pos] == '\0') return;
    if (l->src[l->pos] == '\n') {
        l->line++;
        l->col = 1;
    } else {
        l->col++;
    }
    l->pos++;
}

static void skip_spaces(Lexer *l) {
    while (cur(l) == ' ' || cur(l) == '\t' || cur(l) == '\r')
        adv(l);
}

static void skip_line_comment(Lexer *l) {
    while (cur(l) && cur(l) != '\n') adv(l);
}

static void skip_block_comment(Lexer *l) {
    adv(l); adv(l);
    while (cur(l)) {
        if (cur(l) == '*' && peek(l) == '/') {
            adv(l); adv(l);
            return;
        }
        adv(l);
    }
    error(l->line, "unterminated block comment");
}

static Token *read_number(Lexer *l) {
    int line = l->line, col = l->col;
    char buf[256]; int i = 0;
    int is_float = 0;
    while (isdigit(cur(l)) || (cur(l) == '.' && !is_float && isdigit(peek(l)))) {
        if (cur(l) == '.') is_float = 1;
        if (i < 255) buf[i++] = cur(l);
        adv(l);
    }
    buf[i] = '\0';
    return make_token(is_float ? TOKEN_FLOAT_LIT : TOKEN_INT_LIT, buf, line, col);
}

static Token *read_string(Lexer *l) {
    int line = l->line, col = l->col;
    adv(l);
    char buf[4096]; int i = 0;
    while (cur(l) && cur(l) != '"') {
        if (cur(l) == '\\') {
            adv(l);
            switch (cur(l)) {
                case 'n': buf[i++] = '\n'; break;
                case 't': buf[i++] = '\t'; break;
                case '"': buf[i++] = '"'; break;
                case '\\': buf[i++] = '\\'; break;
                default: buf[i++] = cur(l); break;
            }
        } else {
            if (i < 4095) buf[i++] = cur(l);
        }
        adv(l);
    }
    if (cur(l) == '"') adv(l);
    else error(line, "unterminated string literal");
    buf[i] = '\0';
    return make_token(TOKEN_STRING_LIT, buf, line, col);
}

static int is_operator_char(char c) {
    return c == '-' || c == '+' || c == '*' || c == '/' || 
           c == '%' || c == '&' || c == '|' || c == '^' ||
           c == '=' || c == '!' || c == '<' || c == '>';
}

static Token *read_operator(Lexer *l) {
    int line = l->line, col = l->col;
    
    /* Two-character operators */
    if (cur(l) == ':' && peek(l) == '=') {
        adv(l); adv(l);
        return make_token(TOKEN_ASSIGN, ":=", line, col);
    }
    if (cur(l) == '+' && peek(l) == '+') {
        adv(l); adv(l);
        return make_token(TOKEN_INC, "++", line, col);
    }
    if (cur(l) == '-' && peek(l) == '-') {
        adv(l); adv(l);
        return make_token(TOKEN_DEC, "--", line, col);
    }
    if (cur(l) == '&' && peek(l) == '&') {
        adv(l); adv(l);
        return make_token(TOKEN_AND, "&&", line, col);
    }
    if (cur(l) == '|' && peek(l) == '|') {
        adv(l); adv(l);
        return make_token(TOKEN_OR, "||", line, col);
    }
    
    /* Single-character */
    char c = cur(l);
    adv(l);
    switch (c) {
        case '+': return make_token(TOKEN_PLUS, "+", line, col);
        case '-': return make_token(TOKEN_MINUS, "-", line, col);
        case '*': return make_token(TOKEN_STAR, "*", line, col);
        case '/': return make_token(TOKEN_SLASH, "/", line, col);
        case '%': return make_token(TOKEN_PERCENT, "%", line, col);
        case '^': return make_token(TOKEN_XOR, "^", line, col);
        case '!': return make_token(TOKEN_NOT, "!", line, col);
        case '(': return make_token(TOKEN_LPAREN, "(", line, col);
        case ')': return make_token(TOKEN_RPAREN, ")", line, col);
        case '{': return make_token(TOKEN_LBRACE, "{", line, col);
        case '}': return make_token(TOKEN_RBRACE, "}", line, col);
        case '[': return make_token(TOKEN_LBRACKET, "[", line, col);
        case ']': return make_token(TOKEN_RBRACKET, "]", line, col);
        case ',': return make_token(TOKEN_COMMA, ",", line, col);
        case ':': return make_token(TOKEN_COLON, ":", line, col);
        case ';': return make_token(TOKEN_SEMICOLON, ";", line, col);
        case '.': return make_token(TOKEN_DOT, ".", line, col);
        case '<': return make_token(TOKEN_LT_ANGLE, "<", line, col);
        case '>': return make_token(TOKEN_GT_ANGLE, ">", line, col);
        case '=': return make_token(TOKEN_ASSIGN, "=", line, col);
        default: return make_token(TOKEN_UNKNOWN, &c, line, col);
    }
}

static Token *read_ident(Lexer *l) {
    int line = l->line, col = l->col;
    char buf[256]; int i = 0;
    while (isalnum(cur(l)) || cur(l) == '_') {
        if (i < 255) buf[i++] = cur(l);
        adv(l);
    }
    buf[i] = '\0';

    /* Keywords */
    static const struct { const char *w; TokenType t; } kw[] = {
        {"function", TOKEN_FUNCTION},
        {"return", TOKEN_RETURN},
        {"skip", TOKEN_SKIP},
        {"if", TOKEN_IF},
        {"else", TOKEN_ELSE},
        {"while", TOKEN_WHILE},
        {"for", TOKEN_FOR},
        {"break", TOKEN_BREAK},
        {"done", TOKEN_DONE},
        {"switch", TOKEN_SWITCH},
        {"case", TOKEN_CASE},
        {"default", TOKEN_DEFAULT},
        {"import", TOKEN_IMPORT},
        {"output", TOKEN_OUTPUT},
        {"input", TOKEN_INPUT},
        {"int", TOKEN_INT},
        {"float", TOKEN_FLOAT},
        {"double", TOKEN_DOUBLE},
        {"string", TOKEN_STRING_TYPE},
        {"array", TOKEN_ARRAY},
        {NULL, 0}
    };
    
    for (int k = 0; kw[k].w; k++)
        if (strcmp(buf, kw[k].w) == 0)
            return make_token(kw[k].t, buf, line, col);
    
    /* Comparison operators as identifiers */
    if (strcmp(buf, "eq") == 0) return make_token(TOKEN_EQ, "==", line, col);
    if (strcmp(buf, "ne") == 0) return make_token(TOKEN_NE, "!=", line, col);
    if (strcmp(buf, "gt") == 0) return make_token(TOKEN_GT, ">", line, col);
    if (strcmp(buf, "lt") == 0) return make_token(TOKEN_LT, "<", line, col);
    if (strcmp(buf, "ge") == 0) return make_token(TOKEN_GE, ">=", line, col);
    if (strcmp(buf, "le") == 0) return make_token(TOKEN_LE, "<=", line, col);

    return make_token(TOKEN_IDENT, buf, line, col);
}

Token *lexer_next(Lexer *l) {
restart:
    skip_spaces(l);
    
    int line = l->line, col = l->col;
    char c = cur(l);
    
    if (c == '\0') return make_token(TOKEN_EOF, "", line, col);
    if (c == '\n') { adv(l); return make_token(TOKEN_NEWLINE, "\\n", line, col); }
    
    if (c == '/' && peek(l) == '/') { skip_line_comment(l); goto restart; }
    if (c == '/' && peek(l) == '*') { skip_block_comment(l); goto restart; }
    
    if (isdigit(c)) return read_number(l);
    if (c == '"') return read_string(l);
    if (isalpha(c) || c == '_') return read_ident(l);
    if (is_operator_char(c) || c == ':' || c == '=' || c == ';' || 
        c == '(' || c == ')' || c == '{' || c == '}' || c == '[' || c == ']' ||
        c == ',' || c == '.' || c == '<' || c == '>')
        return read_operator(l);
    
    char buf[2] = {c, '\0'};
    adv(l);
    return make_token(TOKEN_UNKNOWN, buf, line, col);
}

const char *token_type_name(TokenType t) {
    switch (t) {
        case TOKEN_INT_LIT: return "INT_LIT";
        case TOKEN_FLOAT_LIT: return "FLOAT_LIT";
        case TOKEN_STRING_LIT: return "STRING_LIT";
        case TOKEN_INT: return "int";
        case TOKEN_FLOAT: return "float";
        case TOKEN_DOUBLE: return "double";
        case TOKEN_STRING_TYPE: return "string";
        case TOKEN_ARRAY: return "array";
        case TOKEN_FUNCTION: return "function";
        case TOKEN_RETURN: return "return";
        case TOKEN_SKIP: return "skip";
        case TOKEN_IF: return "if";
        case TOKEN_ELSE_IF: return "else if";
        case TOKEN_ELSE: return "else";
        case TOKEN_WHILE: return "while";
        case TOKEN_FOR: return "for";
        case TOKEN_BREAK: return "break";
        case TOKEN_DONE: return "done";
        case TOKEN_SWITCH: return "switch";
        case TOKEN_CASE: return "case";
        case TOKEN_DEFAULT: return "default";
        case TOKEN_IMPORT: return "import";
        case TOKEN_OUTPUT: return "output";
        case TOKEN_INPUT: return "input";
        case TOKEN_ASSIGN: return "=";
        case TOKEN_PLUS: return "+";
        case TOKEN_MINUS: return "-";
        case TOKEN_STAR: return "*";
        case TOKEN_SLASH: return "/";
        case TOKEN_PERCENT: return "%";
        case TOKEN_INC: return "++";
        case TOKEN_DEC: return "--";
        case TOKEN_EQ: return "==";
        case TOKEN_NE: return "!=";
        case TOKEN_GT: return ">";
        case TOKEN_LT: return "<";
        case TOKEN_GE: return ">=";
        case TOKEN_LE: return "<=";
        case TOKEN_AND: return "&&";
        case TOKEN_OR: return "||";
        case TOKEN_NOT: return "!";
        case TOKEN_XOR: return "^";
        case TOKEN_LPAREN: return "(";
        case TOKEN_RPAREN: return ")";
        case TOKEN_LBRACE: return "{";
        case TOKEN_RBRACE: return "}";
        case TOKEN_COMMA: return ",";
        case TOKEN_COLON: return ":";
        case TOKEN_SEMICOLON: return ";";
        case TOKEN_LT_ANGLE: return "<";
        case TOKEN_GT_ANGLE: return ">";
        case TOKEN_IDENT: return "IDENT";
        case TOKEN_NEWLINE: return "NEWLINE";
        case TOKEN_EOF: return "EOF";
        default: return "UNKNOWN";
    }
}