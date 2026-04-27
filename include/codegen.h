#ifndef XLANG_CODEGEN_H
#define XLANG_CODEGEN_H

#include "ast.h"
#include <stdio.h>

typedef struct {
    FILE *out;
    int   indent;
    int   needs_string_h;
    int   needs_math_h;
} CodeGen;

CodeGen *codegen_new(FILE *out);
void     codegen_free(CodeGen *cg);
void     codegen_run(CodeGen *cg, ASTNode *root);

#endif