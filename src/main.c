#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "codegen.h"
#include "error.h"

/* Installed at: /usr/local/lib/xlang  (set by 'make install') */
#ifndef XLANG_RUNTIME_DIR
#define XLANG_RUNTIME_DIR "/usr/local/lib/xlang"
#endif

static char *read_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) { perror(path); return NULL; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);
    char *buf = malloc(sz + 2);
    if (fread(buf, 1, sz, f) != (size_t)sz) { /* ignore short reads */ }
    buf[sz] = '\0';
    fclose(f);
    return buf;
}

static void print_usage(const char *prog) {
    fprintf(stderr,
        "\033[1mXLang Compiler v1.0\033[0m\n"
        "Usage:\n"
        "  %s <source.x>              Compile to executable\n"
        "  %s <source.x> -o <out>     Specify output binary\n"
        "  %s <source.x> --emit-c     Only emit generated C code\n"
        "  %s <source.x> --ast        Print the AST\n"
        "  %s <source.x> --tokens     Print all tokens\n",
        prog, prog, prog, prog, prog);
}

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "--version") == 0) {
        printf("XLang Compiler v2.0\n");
        return 0;
    }
    if (argc < 2) { print_usage(argv[0]); return 1; }

    const char *src_path = NULL;
    const char *out_name = "a.out";
    int emit_c = 0, emit_ast = 0, emit_tok = 0;

    for (int i = 1; i < argc; i++) {
        if      (strcmp(argv[i], "--emit-c")  == 0) emit_c   = 1;
        else if (strcmp(argv[i], "--ast")     == 0) emit_ast = 1;
        else if (strcmp(argv[i], "--tokens")  == 0) emit_tok = 1;
        else if (strcmp(argv[i], "-o") == 0 && i+1 < argc) out_name = argv[++i];
        else if (argv[i][0] != '-') src_path = argv[i];
    }
    if (!src_path) { print_usage(argv[0]); return 1; }

    char *source = read_file(src_path);
    if (!source) return 1;

    Lexer *lexer = lexer_new(source);

    if (emit_tok) {
        Token *t;
        while ((t = lexer_next(lexer))->type != TOKEN_EOF) {
            printf("line %3d  %-15s  '%s'\n", t->line,
                   token_type_name(t->type), t->value);
            token_free(t);
        }
        token_free(t);
        lexer_free(lexer);
        free(source);
        return 0;
    }

    Parser  *parser = parser_new(lexer);
    ASTNode *ast    = parser_parse(parser);

    if (emit_ast) {
        ast_print(ast, 0);
        ast_free(ast); parser_free(parser); lexer_free(lexer); free(source);
        return 0;
    }
    if (parser->error_count > 0) {
        fprintf(stderr, "%d parse error(s). Aborting.\n", parser->error_count);
        ast_free(ast); parser_free(parser); lexer_free(lexer); free(source);
        return 1;
    }

    if (emit_c) {
        CodeGen *cg = codegen_new(stdout);
        codegen_run(cg, ast);
        codegen_free(cg);
    } else {
        /* Write generated C to /tmp */
        char c_path[512];
        snprintf(c_path, sizeof(c_path), "/tmp/xlang_%ld.c", (long)getpid());
        FILE *cf = fopen(c_path, "w");
        if (!cf) { perror("cannot write temp file"); return 1; }
        CodeGen *cg = codegen_new(cf);
        codegen_run(cg, ast);
        codegen_free(cg);
        fclose(cf);

        /* Compile: runtime.c and runtime.h are in XLANG_RUNTIME_DIR */
        char cmd[2048];
        snprintf(cmd, sizeof(cmd),
            "gcc -o \"%s\" \"%s\" \"%s/runtime.c\" -I\"%s\" -lm 2>&1",
            out_name, c_path, XLANG_RUNTIME_DIR, XLANG_RUNTIME_DIR);

        int ret = system(cmd);
        remove(c_path);

        if (ret != 0) {
            fprintf(stderr, "Compilation Failed.\n");
            ast_free(ast); parser_free(parser); lexer_free(lexer); free(source);
            return 1;
        }
        fprintf(stderr, "Compilation completed successfully. Output file:%s\n", out_name);
    }

    ast_free(ast); parser_free(parser); lexer_free(lexer); free(source);
    return 0;
}