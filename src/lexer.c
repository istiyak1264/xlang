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

static char cur(Lexer *l)  { return l->src[l->pos]; }
static char peek(Lexer *l) { return l->src[l->pos + 1]; }

static void adv(Lexer *l) {
    if (l->src[l->pos] == '\0') return;
    if (l->src[l->pos] == '\n') { l->line++; l->col = 1; }
    else                         { l->col++; }
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
        if (cur(l) == '*' && peek(l) == '/') { adv(l); adv(l); return; }
        adv(l);
    }
    error(l->line, "unterminated block comment");
}

static Token *read_number(Lexer *l) {
    int  line = l->line, col = l->col;
    int  cap = 64, i = 0, is_float = 0;
    char *buf = malloc(cap);

    while (isdigit(cur(l)) ||
           (cur(l) == '.' && !is_float && isdigit(peek(l)))) {
        if (cur(l) == '.') is_float = 1;
        if (i >= cap - 1) { cap *= 2; buf = realloc(buf, cap); }
        buf[i++] = cur(l);
        adv(l);
    }
    buf[i] = '\0';
    Token *t = make_token(is_float ? TOKEN_FLOAT_LIT : TOKEN_INT_LIT, buf, line, col);
    free(buf);
    return t;
}

static Token *read_string(Lexer *l) {
    int  line = l->line, col = l->col;
    int  cap = 256, i = 0;
    char *buf = malloc(cap);
    adv(l); /* skip opening " */

    while (cur(l) && cur(l) != '"') {
        if (i >= cap - 2) { cap *= 2; buf = realloc(buf, cap); }
        if (cur(l) == '\\') {
            adv(l);
            switch (cur(l)) {
                case 'n':  buf[i++] = '\n'; break;
                case 't':  buf[i++] = '\t'; break;
                case '"':  buf[i++] = '"';  break;
                case '\\': buf[i++] = '\\'; break;
                default:   buf[i++] = '\\'; buf[i++] = cur(l); break;
            }
        } else {
            buf[i++] = cur(l);
        }
        adv(l);
    }
    if (cur(l) == '"') adv(l);
    else { free(buf); error(line, "unterminated string literal"); }
    buf[i] = '\0';
    Token *t = make_token(TOKEN_STRING_LIT, buf, line, col);
    free(buf);
    return t;
}

static Token *read_angle_string(Lexer *l) {
    int line = l->line, col = l->col;
    int cap = 256, i = 0;
    char *buf = malloc(cap);
    adv(l); /* skip '<' */

    while (cur(l) && cur(l) != '>') {
        if (i >= cap - 1) { cap *= 2; buf = realloc(buf, cap); }
        buf[i++] = cur(l);
        adv(l);
    }
    if (cur(l) == '>') adv(l);
    else { free(buf); error(line, "unterminated angle-bracket import"); }
    buf[i] = '\0';
    Token *t = make_token(TOKEN_STRING_LIT, buf, line, col);
    free(buf);
    return t;
}

static Token *read_operator(Lexer *l) {
    int  line = l->line, col = l->col;

    if (cur(l) == ':' && peek(l) == '=') { adv(l); adv(l); return make_token(TOKEN_ASSIGN,    ":=", line, col); }
    if (cur(l) == '+' && peek(l) == '+') { adv(l); adv(l); return make_token(TOKEN_INC,       "++", line, col); }
    if (cur(l) == '-' && peek(l) == '-') { adv(l); adv(l); return make_token(TOKEN_DEC,       "--", line, col); }
    if (cur(l) == '&' && peek(l) == '&') { adv(l); adv(l); return make_token(TOKEN_AND,       "&&", line, col); }
    if (cur(l) == '|' && peek(l) == '|') { adv(l); adv(l); return make_token(TOKEN_OR,        "||", line, col); }
    if (cur(l) == '=' && peek(l) == '=') { adv(l); adv(l); return make_token(TOKEN_EQ,        "==", line, col); }
    if (cur(l) == '!' && peek(l) == '=') { adv(l); adv(l); return make_token(TOKEN_NE,        "!=", line, col); }
    if (cur(l) == '<' && peek(l) == '=') { adv(l); adv(l); return make_token(TOKEN_LE,        "<=", line, col); }
    if (cur(l) == '>' && peek(l) == '=') { adv(l); adv(l); return make_token(TOKEN_GE,        ">=", line, col); }

    char c = cur(l); adv(l);
    switch (c) {
        case '+': return make_token(TOKEN_PLUS,      "+", line, col);
        case '-': return make_token(TOKEN_MINUS,     "-", line, col);
        case '*': return make_token(TOKEN_STAR,      "*", line, col);
        case '/': return make_token(TOKEN_SLASH,     "/", line, col);
        case '%': return make_token(TOKEN_PERCENT,   "%", line, col);
        case '^': return make_token(TOKEN_XOR,       "^", line, col);
        case '!': return make_token(TOKEN_NOT,       "!", line, col);
        case '<': return make_token(TOKEN_LT,        "<", line, col);
        case '>': return make_token(TOKEN_GT,        ">", line, col);
        case '=': return make_token(TOKEN_ASSIGN_EQ, "=", line, col);
        case '(': return make_token(TOKEN_LPAREN,    "(", line, col);
        case ')': return make_token(TOKEN_RPAREN,    ")", line, col);
        case '{': return make_token(TOKEN_LBRACE,    "{", line, col);
        case '}': return make_token(TOKEN_RBRACE,    "}", line, col);
        case '[': return make_token(TOKEN_LBRACKET,  "[", line, col);
        case ']': return make_token(TOKEN_RBRACKET,  "]", line, col);
        case ',': return make_token(TOKEN_COMMA,     ",", line, col);
        case ':': return make_token(TOKEN_COLON,     ":", line, col);
        case ';': return make_token(TOKEN_SEMICOLON, ";", line, col);
        case '.': return make_token(TOKEN_DOT,       ".", line, col);
        default: {
            char buf[2] = {c, '\0'};
            return make_token(TOKEN_UNKNOWN, buf, line, col);
        }
    }
}

static const struct { const char *w; TokenType t; } kw[] = {
    {"function", TOKEN_FUNCTION},
    {"return",   TOKEN_RETURN},
    {"skip",     TOKEN_SKIP},
    {"if",       TOKEN_IF},
    {"else",     TOKEN_ELSE},
    {"while",    TOKEN_WHILE},
    {"for",      TOKEN_FOR},
    {"break",    TOKEN_BREAK},
    {"done",     TOKEN_DONE},
    {"switch",   TOKEN_SWITCH},
    {"case",     TOKEN_CASE},
    {"default",  TOKEN_DEFAULT},
    {"import",   TOKEN_IMPORT},
    {"output",   TOKEN_OUTPUT},
    {"input",    TOKEN_INPUT},
    {"int",      TOKEN_INT},
    {"float",    TOKEN_FLOAT},
    {"double",   TOKEN_DOUBLE},
    {"string",   TOKEN_STRING_TYPE},
    {"array",    TOKEN_ARRAY},
    {NULL, 0}
};

static Token *read_ident(Lexer *l) {
    int  line = l->line, col = l->col;
    int  cap = 64, i = 0;
    char *buf = malloc(cap);

    while (isalnum(cur(l)) || cur(l) == '_') {
        if (i >= cap - 1) { cap *= 2; buf = realloc(buf, cap); }
        buf[i++] = cur(l);
        adv(l);
    }
    buf[i] = '\0';

    /* keywords */
    for (int k = 0; kw[k].w; k++) {
        if (strcmp(buf, kw[k].w) == 0) {
            /* special: "else" may be followed by "if" */
            if (kw[k].t == TOKEN_ELSE) {
                int save_pos  = l->pos;
                int save_line = l->line;
                int save_col  = l->col;
                while (l->src[l->pos] == ' ' || l->src[l->pos] == '\t') l->pos++;
                if (l->src[l->pos] == 'i' && l->src[l->pos+1] == 'f' &&
                    !isalnum(l->src[l->pos+2]) && l->src[l->pos+2] != '_') {
                    l->pos += 2;
                    l->col  = save_col + (l->pos - save_pos);
                    Token *t = make_token(TOKEN_ELSE_IF, "else if", line, col);
                    free(buf);
                    return t;
                }
                l->pos  = save_pos;
                l->line = save_line;
                l->col  = save_col;
            }
            Token *t = make_token(kw[k].t, buf, line, col);
            free(buf);
            return t;
        }
    }

    TokenType alias = 0;
    if      (strcmp(buf, "eq") == 0) alias = TOKEN_EQ;
    else if (strcmp(buf, "ne") == 0) alias = TOKEN_NE;
    else if (strcmp(buf, "gt") == 0) alias = TOKEN_GT;
    else if (strcmp(buf, "lt") == 0) alias = TOKEN_LT;
    else if (strcmp(buf, "ge") == 0) alias = TOKEN_GE;
    else if (strcmp(buf, "le") == 0) alias = TOKEN_LE;

    if (alias) {
        Token *t = make_token(alias, buf, line, col);
        free(buf);
        return t;
    }

    Token *t = make_token(TOKEN_IDENT, buf, line, col);
    free(buf);
    return t;
}

Token *lexer_next(Lexer *l) {
restart:
    skip_spaces(l);
    int  line = l->line, col = l->col;
    char c    = cur(l);

    if (c == '\0') return make_token(TOKEN_EOF,     "",    line, col);
    if (c == '\n') { adv(l); return make_token(TOKEN_NEWLINE, "\\n", line, col); }

    if (c == '/' && peek(l) == '/') { skip_line_comment(l);  goto restart; }
    if (c == '/' && peek(l) == '*') { skip_block_comment(l); goto restart; }

    /*
     * Dash-word operators: -lt  -gt  -eq  -ne  -ge  -le
     * Check if '-' is followed immediately by one of the keyword letters.
     */
    if (c == '-') {
        /* peek ahead to see if it's -lt / -gt / -eq / -ne / -ge / -le */
        const char *s = l->src + l->pos + 1; /* char after '-' */
        struct { const char *word; TokenType tok; } dash_ops[] = {
            {"lt", TOKEN_LT}, {"gt", TOKEN_GT},
            {"eq", TOKEN_EQ}, {"ne", TOKEN_NE},
            {"ge", TOKEN_GE}, {"le", TOKEN_LE},
            {NULL, 0}
        };
        for (int i = 0; dash_ops[i].word; i++) {
            size_t wlen = strlen(dash_ops[i].word);
            if (strncmp(s, dash_ops[i].word, wlen) == 0 &&
                !isalnum(s[wlen]) && s[wlen] != '_') {
                /* consume '-' + word */
                adv(l);
                for (size_t j = 0; j < wlen; j++) adv(l);
                char buf[8];
                snprintf(buf, sizeof(buf), "-%s", dash_ops[i].word);
                return make_token(dash_ops[i].tok, buf, line, col);
            }
        }
        /* also handle '--' decrement */
        if (peek(l) == '-') { adv(l); adv(l); return make_token(TOKEN_DEC, "--", line, col); }
    }

    if (c == '<') {
        /* Look back (skipping spaces) on current line for "import" */
        int p = l->pos - 1;
        while (p >= 0 && (l->src[p] == ' ' || l->src[p] == '\t')) p--;
        /* check if src[p-5..p] == "import" */
        if (p >= 5 &&
            l->src[p-5]=='i' && l->src[p-4]=='m' && l->src[p-3]=='p' &&
            l->src[p-2]=='o' && l->src[p-1]=='r' && l->src[p]=='t') {
            return read_angle_string(l);
        }
    }

    if (isdigit(c))             return read_number(l);
    if (c == '"')               return read_string(l);
    if (isalpha(c) || c == '_') return read_ident(l);

    return read_operator(l);
}

const char *token_type_name(TokenType t) {
    switch (t) {
        case TOKEN_INT_LIT:     return "INT_LIT";
        case TOKEN_FLOAT_LIT:   return "FLOAT_LIT";
        case TOKEN_STRING_LIT:  return "STRING_LIT";
        case TOKEN_INT:         return "int";
        case TOKEN_FLOAT:       return "float";
        case TOKEN_DOUBLE:      return "double";
        case TOKEN_STRING_TYPE: return "string";
        case TOKEN_ARRAY:       return "array";
        case TOKEN_FUNCTION:    return "function";
        case TOKEN_RETURN:      return "return";
        case TOKEN_SKIP:        return "skip";
        case TOKEN_IF:          return "if";
        case TOKEN_ELSE_IF:     return "else if";
        case TOKEN_ELSE:        return "else";
        case TOKEN_WHILE:       return "while";
        case TOKEN_FOR:         return "for";
        case TOKEN_BREAK:       return "break";
        case TOKEN_DONE:        return "done";
        case TOKEN_SWITCH:      return "switch";
        case TOKEN_CASE:        return "case";
        case TOKEN_DEFAULT:     return "default";
        case TOKEN_IMPORT:      return "import";
        case TOKEN_OUTPUT:      return "output";
        case TOKEN_INPUT:       return "input";
        case TOKEN_ASSIGN:      return ":=";
        case TOKEN_ASSIGN_EQ:   return "=";
        case TOKEN_PLUS:        return "+";
        case TOKEN_MINUS:       return "-";
        case TOKEN_STAR:        return "*";
        case TOKEN_SLASH:       return "/";
        case TOKEN_PERCENT:     return "%";
        case TOKEN_INC:         return "++";
        case TOKEN_DEC:         return "--";
        case TOKEN_EQ:          return "==";
        case TOKEN_NE:          return "!=";
        case TOKEN_GT:          return ">";
        case TOKEN_LT:          return "<";
        case TOKEN_GE:          return ">=";
        case TOKEN_LE:          return "<=";
        case TOKEN_AND:         return "&&";
        case TOKEN_OR:          return "||";
        case TOKEN_NOT:         return "!";
        case TOKEN_XOR:         return "^";
        case TOKEN_LPAREN:      return "(";
        case TOKEN_RPAREN:      return ")";
        case TOKEN_LBRACE:      return "{";
        case TOKEN_RBRACE:      return "}";
        case TOKEN_COMMA:       return ",";
        case TOKEN_COLON:       return ":";
        case TOKEN_SEMICOLON:   return ";";
        case TOKEN_LBRACKET:    return "[";
        case TOKEN_RBRACKET:    return "]";
        case TOKEN_DOT:         return ".";
        case TOKEN_LT_ANGLE:    return "<";
        case TOKEN_GT_ANGLE:    return ">";
        case TOKEN_IDENT:       return "IDENT";
        case TOKEN_NEWLINE:     return "NEWLINE";
        case TOKEN_EOF:         return "EOF";
        default:                return "UNKNOWN";
    }
}