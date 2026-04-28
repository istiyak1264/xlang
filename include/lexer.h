#ifndef XLANG_LEXER_H
#define XLANG_LEXER_H

typedef enum {
    /* Literals */
    TOKEN_INT_LIT, TOKEN_FLOAT_LIT, TOKEN_STRING_LIT,
    /* Types */
    TOKEN_INT, TOKEN_FLOAT, TOKEN_DOUBLE, TOKEN_STRING_TYPE, TOKEN_ARRAY,
    /* Keywords */
    TOKEN_FUNCTION, TOKEN_RETURN, TOKEN_SKIP,
    TOKEN_IF, TOKEN_ELSE_IF, TOKEN_ELSE,
    TOKEN_WHILE, TOKEN_FOR, TOKEN_BREAK, TOKEN_DONE,
    TOKEN_SWITCH, TOKEN_CASE, TOKEN_DEFAULT,
    TOKEN_IMPORT, TOKEN_OUTPUT, TOKEN_INPUT,
    /* Identifier */
    TOKEN_IDENT,
    /* Assignment */
    TOKEN_ASSIGN,       /* :=  */
    TOKEN_ASSIGN_EQ,    /* =   (reassignment) */
    /* Arithmetic */
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_PERCENT,
    TOKEN_INC, TOKEN_DEC,
    /* Comparison — symbolic and dash-word forms */
    TOKEN_EQ, TOKEN_NE, TOKEN_GT, TOKEN_LT, TOKEN_GE, TOKEN_LE,
    /* Logical */
    TOKEN_AND, TOKEN_OR, TOKEN_NOT, TOKEN_XOR,
    /* Delimiters */
    TOKEN_LPAREN, TOKEN_RPAREN,
    TOKEN_LBRACE, TOKEN_RBRACE,
    TOKEN_LBRACKET, TOKEN_RBRACKET,
    TOKEN_COMMA, TOKEN_COLON, TOKEN_SEMICOLON,
    TOKEN_DOT, TOKEN_LT_ANGLE, TOKEN_GT_ANGLE,
    /* Special */
    TOKEN_NEWLINE, TOKEN_EOF, TOKEN_UNKNOWN
} TokenType;

typedef struct {
    TokenType   type;
    char       *value;
    int         line;
    int         col;
} Token;

typedef struct {
    char *src;
    int   pos;
    int   line;
    int   col;
} Lexer;

Lexer      *lexer_new(const char *source);
void        lexer_free(Lexer *l);
Token      *lexer_next(Lexer *l);
void        token_free(Token *t);
const char *token_type_name(TokenType t);

#endif