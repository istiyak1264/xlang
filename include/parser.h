#ifndef XLANG_PARSER_H
#define XLANG_PARSER_H
 
#include "lexer.h"
#include "ast.h"
 
typedef struct {
    Lexer   *lexer;
    Token   *current;
    Token   *peek;
    int      error_count;
} Parser;
 
Parser  *parser_new(Lexer *lexer);
void     parser_free(Parser *p);
ASTNode *parser_parse(Parser *p);
 
#endif 