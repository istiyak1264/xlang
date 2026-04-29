#include "parser.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static ASTNode *parse_statement(Parser *p);
static ASTNode *parse_block(Parser *p);
static ASTNode *parse_expr(Parser *p);
static ASTNode *parse_expr_or(Parser *p);

static void advance(Parser *p) {
    token_free(p->current);
    p->current = p->peek;
    p->peek    = lexer_next(p->lexer);
}

static int check(Parser *p, TokenType t) { return p->current->type == t; }

static void skip_nl(Parser *p) {
    while (p->current->type == TOKEN_NEWLINE) advance(p);
}

static Token *expect(Parser *p, TokenType t) {
    if (!check(p, t)) {
        error(p->current->line, "expected '%s' but got '%s'",
              token_type_name(t),
              token_type_name(p->current->type));
        p->error_count++;
    }
    Token *tok = p->current;
    p->current = NULL;
    advance(p);
    return tok;
}

static VarType parse_type_token(Parser *p) {
    switch (p->current->type) {
        case TOKEN_INT:         advance(p); return TYPE_INT;
        case TOKEN_FLOAT:       advance(p); return TYPE_FLOAT;
        case TOKEN_DOUBLE:      advance(p); return TYPE_DOUBLE;
        case TOKEN_STRING_TYPE: advance(p); return TYPE_STRING;
        case TOKEN_ARRAY:       advance(p); return TYPE_ARRAY;
        default:                            return TYPE_UNKNOWN;
    }
}

static int is_type_token(Parser *p) {
    TokenType t = p->current->type;
    return t == TOKEN_INT || t == TOKEN_FLOAT || t == TOKEN_DOUBLE ||
           t == TOKEN_STRING_TYPE || t == TOKEN_ARRAY;
}

Parser *parser_new(Lexer *lexer) {
    Parser *p = calloc(1, sizeof(Parser));
    p->lexer = lexer;
    p->current = lexer_next(lexer);
    p->peek = lexer_next(lexer);
    skip_nl(p);
    return p;
}

void parser_free(Parser *p) {
    if (!p) return;
    token_free(p->current);
    token_free(p->peek);
    free(p);
}

static ASTNode *parse_primary(Parser *p) {
    int line = p->current->line;

    if (check(p, TOKEN_INT_LIT)) {
        ASTNode *n = ast_node_new(NODE_INT_LIT, line);
        n->ival = atoll(p->current->value);
        advance(p);
        return n;
    }
    if (check(p, TOKEN_FLOAT_LIT)) {
        ASTNode *n = ast_node_new(NODE_FLOAT_LIT, line);
        n->fval = atof(p->current->value);
        advance(p);
        return n;
    }
    if (check(p, TOKEN_STRING_LIT)) {
        ASTNode *n = ast_node_new(NODE_STRING_LIT, line);
        n->sval = strdup(p->current->value);
        advance(p);
        return n;
    }
    if (check(p, TOKEN_INPUT)) {
        advance(p);
        expect(p, TOKEN_LPAREN);
        expect(p, TOKEN_RPAREN);
        return ast_node_new(NODE_INPUT, line);
    }
    if (check(p, TOKEN_LBRACE)) {
        ASTNode *arr = ast_node_new(NODE_ARRAY_LIT, line);
        advance(p);
        while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
            ast_add_child(arr, parse_expr(p));
            if (check(p, TOKEN_COMMA)) advance(p);
        }
        expect(p, TOKEN_RBRACE);
        return arr;
    }
    if (check(p, TOKEN_IDENT)) {
        char *name = strdup(p->current->value);
        advance(p);

        if (check(p, TOKEN_LPAREN)) {
            ASTNode *call = ast_node_new(NODE_FUNC_CALL, line);
            call->sval = name;
            advance(p);
            while (!check(p, TOKEN_RPAREN) && !check(p, TOKEN_EOF)) {
                ast_add_child(call, parse_expr(p));
                if (check(p, TOKEN_COMMA)) advance(p);
            }
            expect(p, TOKEN_RPAREN);
            return call;
        }

        ASTNode *ident = ast_node_new(NODE_IDENT, line);
        ident->sval = name;
        return ident;
    }
    if (check(p, TOKEN_LPAREN)) {
        advance(p);
        ASTNode *e = parse_expr(p);
        expect(p, TOKEN_RPAREN);
        return e;
    }
    if (check(p, TOKEN_NOT) || check(p, TOKEN_MINUS)) {
        char op = p->current->value[0];
        advance(p);
        ASTNode *u = ast_node_new(NODE_UNOP, line);
        u->sval = strdup(&op);
        ast_add_child(u, parse_primary(p));
        return u;
    }

    error(line, "unexpected token '%s' in expression", p->current->value);
    return ast_node_new(NODE_INT_LIT, line);
}

static ASTNode *parse_postfix(Parser *p) {
    ASTNode *left = parse_primary(p);
    for (;;) {
        int line = p->current->line;
        if (check(p, TOKEN_INC) || check(p, TOKEN_DEC)) {
            ASTNode *n = ast_node_new(NODE_INC_DEC, line);
            n->sval = strdup(p->current->value);
            ast_add_child(n, left);
            advance(p);
            left = n;
        } else if (check(p, TOKEN_LBRACKET)) {
            advance(p); /* consume '[' */
            ASTNode *n = ast_node_new(NODE_INDEX, line);
            ast_add_child(n, left);
            ast_add_child(n, parse_expr(p));
            expect(p, TOKEN_RBRACKET);
            left = n;
        } else {
            break;
        }
    }
    return left;
}

static ASTNode *parse_mul(Parser *p) {
    ASTNode *left = parse_postfix(p);
    while (check(p, TOKEN_STAR) || check(p, TOKEN_SLASH) || check(p, TOKEN_PERCENT)) {
        int line = p->current->line;
        char *op = strdup(p->current->value);
        advance(p);
        ASTNode *bin = ast_node_new(NODE_BINOP, line);
        bin->sval = op;
        ast_add_child(bin, left);
        ast_add_child(bin, parse_postfix(p));
        left = bin;
    }
    return left;
}

static ASTNode *parse_add(Parser *p) {
    ASTNode *left = parse_mul(p);
    while (check(p, TOKEN_PLUS) || check(p, TOKEN_MINUS)) {
        int line = p->current->line;
        char *op = strdup(p->current->value);
        advance(p);
        ASTNode *bin = ast_node_new(NODE_BINOP, line);
        bin->sval = op;
        ast_add_child(bin, left);
        ast_add_child(bin, parse_mul(p));
        left = bin;
    }
    return left;
}

static ASTNode *parse_cmp(Parser *p) {
    ASTNode *left = parse_add(p);
    while (check(p, TOKEN_EQ) || check(p, TOKEN_NE) || check(p, TOKEN_GT) ||
           check(p, TOKEN_LT) || check(p, TOKEN_GE) || check(p, TOKEN_LE)) {
        int line = p->current->line;
        char *op = strdup(p->current->value);
        advance(p);
        ASTNode *bin = ast_node_new(NODE_BINOP, line);
        bin->sval = op;
        ast_add_child(bin, left);
        ast_add_child(bin, parse_add(p));
        left = bin;
    }
    return left;
}

static ASTNode *parse_and(Parser *p) {
    ASTNode *left = parse_cmp(p);
    while (check(p, TOKEN_AND)) {
        int line = p->current->line;
        advance(p);
        ASTNode *bin = ast_node_new(NODE_BINOP, line);
        bin->sval = strdup("&&");
        ast_add_child(bin, left);
        ast_add_child(bin, parse_cmp(p));
        left = bin;
    }
    return left;
}

static ASTNode *parse_expr_or(Parser *p) {
    ASTNode *left = parse_and(p);
    while (check(p, TOKEN_OR)) {
        int line = p->current->line;
        advance(p);
        ASTNode *bin = ast_node_new(NODE_BINOP, line);
        bin->sval = strdup("||");
        ast_add_child(bin, left);
        ast_add_child(bin, parse_and(p));
        left = bin;
    }
    return left;
}

static ASTNode *parse_expr(Parser *p) { return parse_expr_or(p); }

static ASTNode *parse_block_internal(Parser *p, int start_col) {
    ASTNode *block = ast_node_new(NODE_BLOCK, p->current->line);

    while (!check(p, TOKEN_EOF)) {
        skip_nl(p);
        if (check(p, TOKEN_EOF)) break;
        if (p->current->col < start_col) break;

        TokenType t = p->current->type;
        if (t == TOKEN_ELSE_IF || t == TOKEN_ELSE ||
            t == TOKEN_CASE    || t == TOKEN_DEFAULT)
            break;

        ASTNode *s = parse_statement(p);
        if (s) ast_add_child(block, s);
        skip_nl(p);
    }
    return block;
}

static ASTNode *parse_block(Parser *p) {
    while (check(p, TOKEN_NEWLINE)) advance(p);
    int start_col = p->current->col;
    return parse_block_internal(p, start_col);
}

static ASTNode *parse_colon_block(Parser *p) {
    if (check(p, TOKEN_COLON)) advance(p);
    while (!check(p, TOKEN_NEWLINE) && !check(p, TOKEN_EOF)) advance(p);
    while (check(p, TOKEN_NEWLINE)) advance(p);
    if (check(p, TOKEN_EOF)) return ast_node_new(NODE_BLOCK, p->current->line);

    int block_col = p->current->col;
    return parse_block_internal(p, block_col);
}

static ASTNode *parse_import(Parser *p) {
    int line = p->current->line;
    advance(p); /* consume 'import' */
    ASTNode *imp = ast_node_new(NODE_IMPORT, line);

    while (!check(p, TOKEN_NEWLINE) && !check(p, TOKEN_EOF)) {
        if (check(p, TOKEN_STRING_LIT)) {
            ASTNode *h = ast_node_new(NODE_IMPORT, line);
            h->sval = strdup(p->current->value);
            ast_add_child(imp, h);
            advance(p);
        }
        if (check(p, TOKEN_COMMA)) advance(p);
    }
    return imp;
}

static ASTNode *parse_function(Parser *p) {
    int line = p->current->line;
    advance(p); /* consume 'function' */
    ASTNode *fn = ast_node_new(NODE_FUNCTION_DEF, line);
    fn->sval = strdup(p->current->value);
    expect(p, TOKEN_IDENT);
    expect(p, TOKEN_LPAREN);

    while (!check(p, TOKEN_RPAREN) && !check(p, TOKEN_EOF)) {
        ASTNode *param = ast_node_new(NODE_PARAM, p->current->line);
        param->vtype = parse_type_token(p);
        param->sval  = strdup(p->current->value);
        expect(p, TOKEN_IDENT);
        ast_add_child(fn, param);
        if (check(p, TOKEN_COMMA)) advance(p);
    }
    expect(p, TOKEN_RPAREN);

    ASTNode *body = parse_colon_block(p);
    ast_add_child(fn, body);
    return fn;
}

static ASTNode *parse_var_decl(Parser *p) {
    int line = p->current->line;
    VarType vt = parse_type_token(p);

    /* collect all names */
    char *names[64];
    int   ncount = 0;

    names[ncount++] = strdup(p->current->value);
    expect(p, TOKEN_IDENT);

    while (check(p, TOKEN_COMMA) && ncount < 64) {
        advance(p); /* consume ',' */
        /* If next token is a type keyword, stop (different decl) */
        if (is_type_token(p)) break;
        names[ncount++] = strdup(p->current->value);
        expect(p, TOKEN_IDENT);
    }

    /* Optional initialiser — applies to ALL declared names */
    ASTNode *init_expr = NULL;
    if (check(p, TOKEN_ASSIGN)) {
        advance(p);
        init_expr = parse_expr(p);
    }

    /* Build a wrapper BLOCK node if more than one name, else single decl */
    if (ncount == 1) {
        ASTNode *n = ast_node_new(NODE_VAR_DECL, line);
        n->vtype = vt;
        n->sval  = names[0];
        if (init_expr) ast_add_child(n, init_expr);
        return n;
    }

    /* Multiple names → a BLOCK of VAR_DECL nodes */
    ASTNode *blk = ast_node_new(NODE_BLOCK, line);
    for (int i = 0; i < ncount; i++) {
        ASTNode *n = ast_node_new(NODE_VAR_DECL, line);
        n->vtype = vt;
        n->sval  = names[i];
        /* Give each its own copy of the initialiser (if any) */
        if (init_expr && i == ncount - 1) {
            ast_add_child(n, init_expr); /* last one takes the real node */
        } else if (init_expr) {
            /* others get default zero — no child means no initialiser */
        }
        ast_add_child(blk, n);
    }
    return blk;
}

static ASTNode *parse_assign_or_expr(Parser *p) {
    int line = p->current->line;
    ASTNode *expr = parse_expr(p);

    if (check(p, TOKEN_ASSIGN) || check(p, TOKEN_ASSIGN_EQ)) {
        advance(p);
        ASTNode *n = ast_node_new(NODE_ASSIGN, line);
        ast_add_child(n, expr);
        ast_add_child(n, parse_expr(p));
        return n;
    }
    return expr;
}

static ASTNode *parse_if(Parser *p) {
    int line   = p->current->line;
    int if_col = p->current->col;
    advance(p); /* consume 'if' */
    ASTNode *n = ast_node_new(NODE_IF, line);

    expect(p, TOKEN_LPAREN);
    ast_add_child(n, parse_expr(p));
    expect(p, TOKEN_RPAREN);
    ast_add_child(n, parse_colon_block(p));

    while (check(p, TOKEN_NEWLINE)) advance(p);

    while (p->current->col == if_col && !check(p, TOKEN_EOF)) {
        if (check(p, TOKEN_ELSE_IF)) {
            int eli = p->current->line;
            advance(p); /* consume 'else if' */
            ASTNode *el = ast_node_new(NODE_IF, eli);
            el->else_kind = ELSE_IF;
            expect(p, TOKEN_LPAREN);
            ast_add_child(el, parse_expr(p));
            expect(p, TOKEN_RPAREN);
            ast_add_child(el, parse_colon_block(p));
            ast_add_child(n, el);
            while (check(p, TOKEN_NEWLINE)) advance(p);
        } else if (check(p, TOKEN_ELSE)) {
            int eli = p->current->line;
            advance(p); /* consume 'else' */
            ASTNode *el = ast_node_new(NODE_IF, eli);
            el->else_kind = ELSE_BARE;
            ast_add_child(el, parse_colon_block(p));
            ast_add_child(n, el);
            while (check(p, TOKEN_NEWLINE)) advance(p);
            break; /* nothing after bare else */
        } else {
            break;
        }
    }
    return n;
}

static ASTNode *parse_while(Parser *p) {
    int line = p->current->line;
    advance(p);
    ASTNode *n = ast_node_new(NODE_WHILE, line);
    expect(p, TOKEN_LPAREN);
    ast_add_child(n, parse_expr(p));
    expect(p, TOKEN_RPAREN);
    ast_add_child(n, parse_colon_block(p));
    return n;
}

static ASTNode *parse_for(Parser *p) {
    int line = p->current->line;
    advance(p);
    ASTNode *n = ast_node_new(NODE_FOR, line);
    expect(p, TOKEN_LPAREN);

    if (is_type_token(p)) ast_add_child(n, parse_var_decl(p));
    else                   ast_add_child(n, parse_assign_or_expr(p));

    expect(p, TOKEN_SEMICOLON);
    ast_add_child(n, parse_expr(p));
    expect(p, TOKEN_SEMICOLON);
    ast_add_child(n, parse_assign_or_expr(p));
    expect(p, TOKEN_RPAREN);
    ast_add_child(n, parse_colon_block(p));
    return n;
}

static ASTNode *parse_switch(Parser *p) {
    int line = p->current->line;
    advance(p);
    ASTNode *n = ast_node_new(NODE_SWITCH, line);
    expect(p, TOKEN_LPAREN);
    ast_add_child(n, parse_expr(p));
    expect(p, TOKEN_RPAREN);
    expect(p, TOKEN_COLON);
    while (check(p, TOKEN_NEWLINE)) advance(p);

    int case_col = p->current->col;

    while ((check(p, TOKEN_CASE) || check(p, TOKEN_DEFAULT)) &&
           p->current->col == case_col && !check(p, TOKEN_EOF)) {
        int cline = p->current->line;
        ASTNode *cas = ast_node_new(NODE_CASE, cline);

        if (check(p, TOKEN_DEFAULT)) {
            cas->sval = strdup("default");
            advance(p);
        } else {
            advance(p);
            ast_add_child(cas, parse_expr(p));
        }
        expect(p, TOKEN_COLON);
        while (check(p, TOKEN_NEWLINE)) advance(p);

        int body_col = p->current->col;
        ASTNode *body = parse_block_internal(p, body_col);
        ast_add_child(cas, body);
        ast_add_child(n, cas);
        while (check(p, TOKEN_NEWLINE)) advance(p);
    }
    return n;
}

static ASTNode *parse_print(Parser *p) {
    int line = p->current->line;
    advance(p);
    ASTNode *n = ast_node_new(NODE_PRINT, line);
    expect(p, TOKEN_LPAREN);
    while (!check(p, TOKEN_RPAREN) && !check(p, TOKEN_EOF)) {
        ast_add_child(n, parse_expr(p));
        if (check(p, TOKEN_COMMA)) advance(p);
    }
    expect(p, TOKEN_RPAREN);
    return n;
}

static ASTNode *parse_output(Parser *p) {
    int line = p->current->line;
    advance(p);
    ASTNode *n = ast_node_new(NODE_OUTPUT, line);
    expect(p, TOKEN_LPAREN);
    while (!check(p, TOKEN_RPAREN) && !check(p, TOKEN_EOF)) {
        ast_add_child(n, parse_expr(p));
        if (check(p, TOKEN_COMMA)) advance(p);
    }
    expect(p, TOKEN_RPAREN);
    return n;
}

static ASTNode *parse_statement(Parser *p) {
    skip_nl(p);
    int line = p->current->line;

    switch (p->current->type) {
        case TOKEN_IMPORT:   return parse_import(p);
        case TOKEN_FUNCTION: return parse_function(p);
        case TOKEN_IF:       return parse_if(p);
        case TOKEN_WHILE:    return parse_while(p);
        case TOKEN_FOR:      return parse_for(p);
        case TOKEN_SWITCH:   return parse_switch(p);
        case TOKEN_OUTPUT:   return parse_output(p);
        case TOKEN_PRINT:    return parse_print(p);

        case TOKEN_RETURN: {
            /* 'return' is forbidden at top scope in xlang — use 'done' */
            advance(p);
            ASTNode *n = ast_node_new(NODE_RETURN, line);
            if (!check(p, TOKEN_NEWLINE) && !check(p, TOKEN_EOF))
                ast_add_child(n, parse_expr(p));
            return n;
        }

        case TOKEN_SKIP:
            advance(p);
            return ast_node_new(NODE_SKIP, line);

        case TOKEN_BREAK:
            advance(p);
            return ast_node_new(NODE_BREAK, line);

        case TOKEN_DONE:
            advance(p);
            return ast_node_new(NODE_DONE, line);

        default:
            if (is_type_token(p)) return parse_var_decl(p);
            return parse_assign_or_expr(p);
    }
}

static int has_done(ASTNode *node) {
    if (!node) return 0;
    if (node->type == NODE_DONE) return 1;
    for (int i = 0; i < node->child_count; i++)
        if (has_done(node->children[i])) return 1;
    return 0;
}

ASTNode *parser_parse(Parser *p) {
    ASTNode *program = ast_node_new(NODE_PROGRAM, 0);

    while (!check(p, TOKEN_EOF)) {
        ASTNode *s = parse_statement(p);
        if (s) ast_add_child(program, s);
        skip_nl(p);
    }

    /* Validate: every program must have a 'main' function with 'done' */
    int found_main = 0, main_has_done = 0;
    for (int i = 0; i < program->child_count; i++) {
        ASTNode *node = program->children[i];
        if (node->type == NODE_FUNCTION_DEF && node->sval &&
            strcmp(node->sval, "main") == 0) {
            found_main = 1;
            main_has_done = has_done(node);
        }
    }

    if (!found_main) {
        error(0, "প্রোগ্রামটিতে কোনো main() ফাংশন ডিফাইন করা হয়নি।");
    }
    if (!main_has_done) {
        error(0, "main() ফাংশনের শেষে done কীওয়ার্ড খুঁজে পাওয়া যায়নি।প্রোগ্রামের শেষ বোঝাতে অবশ্যই done লিখতে হবে।");
    }

    return program;
}