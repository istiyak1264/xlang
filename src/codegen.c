#include "codegen.h"
#include "ast.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define SYM_CAP 256
typedef struct { char name[64]; VarType vtype; } Symbol;

static Symbol sym_table[SYM_CAP];
static int    sym_count = 0;

static void sym_push(const char *name, VarType vt) {
    if (sym_count < SYM_CAP) {
        strncpy(sym_table[sym_count].name, name, 63);
        sym_table[sym_count].vtype = vt;
        sym_count++;
    }
}

static VarType sym_lookup(const char *name) {
    for (int i = sym_count - 1; i >= 0; i--)
        if (strcmp(sym_table[i].name, name) == 0)
            return sym_table[i].vtype;
    return TYPE_INT; /* default */
}

static void sym_reset(void) { sym_count = 0; }

CodeGen *codegen_new(FILE *out) {
    CodeGen *cg = calloc(1, sizeof(CodeGen));
    cg->out     = out;
    return cg;
}

void codegen_free(CodeGen *cg) { if (cg) free(cg); }

static void do_indent(CodeGen *cg) {
    for (int i = 0; i < cg->indent; i++)
        fprintf(cg->out, "    ");
}

static const char *vtype_to_c(VarType t) {
    switch (t) {
        case TYPE_INT:    return "long long";
        case TYPE_FLOAT:  return "float";
        case TYPE_DOUBLE: return "double";
        case TYPE_STRING: return "char*";
        case TYPE_ARRAY:  return "XLangArray*";
        case TYPE_VOID:   return "void";
        default:          return "long long";
    }
}

/* printf format for a VarType */
static const char *vtype_fmt(VarType t) {
    switch (t) {
        case TYPE_FLOAT:
        case TYPE_DOUBLE: return "%g";
        case TYPE_STRING: return "%s";
        default:          return "%lld";
    }
}

static const char *op_to_c(const char *op) {
    if (!op) return "";
    if (strcmp(op, "==") == 0 || strcmp(op, "-eq") == 0) return "==";
    if (strcmp(op, "!=") == 0 || strcmp(op, "-ne") == 0) return "!=";
    if (strcmp(op, ">")  == 0 || strcmp(op, "-gt") == 0) return ">";
    if (strcmp(op, "<")  == 0 || strcmp(op, "-lt") == 0) return "<";
    if (strcmp(op, ">=") == 0 || strcmp(op, "-ge") == 0) return ">=";
    if (strcmp(op, "<=") == 0 || strcmp(op, "-le") == 0) return "<=";
    if (strcmp(op, "&&") == 0) return "&&";
    if (strcmp(op, "||") == 0) return "||";
    return op;
}

/*
 * Determine expression type — now consults symbol table for identifiers.
 */
static VarType expr_type(ASTNode *node) {
    if (!node) return TYPE_INT;
    switch (node->type) {
        case NODE_INT_LIT:    return TYPE_INT;
        case NODE_FLOAT_LIT:  return TYPE_FLOAT;
        case NODE_STRING_LIT: return TYPE_STRING;
        case NODE_INPUT:      return TYPE_STRING;
        case NODE_IDENT:      return sym_lookup(node->sval);
        case NODE_BINOP:      return expr_type(node->children[0]);
        case NODE_FUNC_CALL:  return TYPE_INT; /* default; extend if needed */
        default:              return TYPE_INT;
    }
}

static void gen_expr(CodeGen *cg, ASTNode *node);
static void gen_stmt(CodeGen *cg, ASTNode *node);
static void gen_block(CodeGen *cg, ASTNode *node);

static void gen_expr(CodeGen *cg, ASTNode *node) {
    if (!node) { fprintf(cg->out, "0"); return; }

    switch (node->type) {
        case NODE_INT_LIT:
            fprintf(cg->out, "%lld", node->ival);
            break;

        case NODE_FLOAT_LIT:
            fprintf(cg->out, "%g", node->fval);
            break;

        case NODE_STRING_LIT:
            fprintf(cg->out, "\"%s\"", node->sval);
            break;

        case NODE_IDENT:
            fprintf(cg->out, "%s", node->sval);
            break;

        case NODE_INPUT:
            /* xlang_input_string() returns a freshly malloc'd char* */
            fprintf(cg->out, "xlang_input_string()");
            break;

        case NODE_FUNC_CALL:
            fprintf(cg->out, "%s(", node->sval);
            for (int i = 0; i < node->child_count; i++) {
                if (i > 0) fprintf(cg->out, ", ");
                gen_expr(cg, node->children[i]);
            }
            fprintf(cg->out, ")");
            break;

        case NODE_BINOP:
            fprintf(cg->out, "(");
            gen_expr(cg, node->children[0]);
            fprintf(cg->out, " %s ", op_to_c(node->sval));
            gen_expr(cg, node->children[1]);
            fprintf(cg->out, ")");
            break;

        case NODE_UNOP:
            fprintf(cg->out, "%s", node->sval);
            fprintf(cg->out, "(");
            gen_expr(cg, node->children[0]);
            fprintf(cg->out, ")");
            break;

        case NODE_INC_DEC:
            gen_expr(cg, node->children[0]);
            fprintf(cg->out, "%s", node->sval);
            break;

        case NODE_INDEX:
            fprintf(cg->out, "*(long long*)xlang_array_get(");
            gen_expr(cg, node->children[0]);
            fprintf(cg->out, ", ");
            gen_expr(cg, node->children[1]);
            fprintf(cg->out, ")");
            break;

        default:
            fprintf(cg->out, "0");
            break;
    }
}

/*
 * output() — fix: use symbol table to detect string variables so we
 * emit %s instead of %lld, preventing the garbage-output bug.
 */
static void gen_output(CodeGen *cg, ASTNode *node) {
    for (int i = 0; i < node->child_count; i++) {
        ASTNode *arg = node->children[i];
        do_indent(cg);

        VarType t = expr_type(arg);

        if (arg->type == NODE_STRING_LIT) {
            /* Literal string: just print it */
            fprintf(cg->out, "printf(\"%%s\\n\", \"%s\");\n", arg->sval);

        } else if (arg->type == NODE_INT_LIT) {
            fprintf(cg->out, "printf(\"%%lld\\n\", (long long)%lld);\n", arg->ival);

        } else if (arg->type == NODE_FLOAT_LIT) {
            fprintf(cg->out, "printf(\"%%g\\n\", (double)%g);\n", arg->fval);

        } else if (t == TYPE_STRING) {
            /* String variable or input() result — use %s, no cast */
            fprintf(cg->out, "printf(\"%%s\\n\", ");
            gen_expr(cg, arg);
            fprintf(cg->out, ");\n");

        } else if (t == TYPE_FLOAT || t == TYPE_DOUBLE) {
            fprintf(cg->out, "printf(\"%%g\\n\", (double)");
            gen_expr(cg, arg);
            fprintf(cg->out, ");\n");

        } else {
            /* Integer (default) */
            fprintf(cg->out, "printf(\"%%lld\\n\", (long long)");
            gen_expr(cg, arg);
            fprintf(cg->out, ");\n");
        }
    }
}

static void gen_if(CodeGen *cg, ASTNode *node) {
    if (node->else_kind == ELSE_BARE) {
        fprintf(cg->out, " else {\n");
        cg->indent++;
        gen_block(cg, node->children[0]);
        cg->indent--;
        do_indent(cg);
        fprintf(cg->out, "}");
    } else if (node->else_kind == ELSE_IF) {
        fprintf(cg->out, " else if (");
        gen_expr(cg, node->children[0]);
        fprintf(cg->out, ") {\n");
        cg->indent++;
        gen_block(cg, node->children[1]);
        cg->indent--;
        do_indent(cg);
        fprintf(cg->out, "}");
        for (int i = 2; i < node->child_count; i++)
            gen_if(cg, node->children[i]);
    } else {
        /* leading if */
        do_indent(cg);
        fprintf(cg->out, "if (");
        gen_expr(cg, node->children[0]);
        fprintf(cg->out, ") {\n");
        cg->indent++;
        gen_block(cg, node->children[1]);
        cg->indent--;
        do_indent(cg);
        fprintf(cg->out, "}");
        for (int i = 2; i < node->child_count; i++)
            gen_if(cg, node->children[i]);
        fprintf(cg->out, "\n");
    }
}

static void gen_stmt(CodeGen *cg, ASTNode *node) {
    if (!node) return;

    switch (node->type) {

        case NODE_BLOCK:
            gen_block(cg, node);
            break;

        case NODE_SKIP:
            do_indent(cg);
            fprintf(cg->out, "/* skip */\n"); /* no-op — like Python pass */
            break;

        case NODE_BREAK:
            do_indent(cg);
            fprintf(cg->out, "break;\n");
            break;

        /* done → exit the program cleanly (return 0 from main) */
        case NODE_DONE:
            do_indent(cg);
            fprintf(cg->out, "return 0;\n");
            break;

        case NODE_RETURN:
            do_indent(cg);
            fprintf(cg->out, "return");
            if (node->child_count > 0) {
                fprintf(cg->out, " ");
                gen_expr(cg, node->children[0]);
            }
            fprintf(cg->out, ";\n");
            break;

        /* ------ variable declaration ------ */
        case NODE_VAR_DECL: {
            /* Check if already declared as a parameter — skip redeclaration */
            int already = 0;
            for (int si = 0; si < sym_count; si++) {
                if (strcmp(sym_table[si].name, node->sval) == 0) {
                    already = 1;
                    /* Update type in case user re-declares with same name */
                    sym_table[si].vtype = node->vtype;
                    break;
                }
            }
            if (!already) {
                sym_push(node->sval, node->vtype);
                do_indent(cg);
                fprintf(cg->out, "%s %s", vtype_to_c(node->vtype), node->sval);
                if (node->child_count > 0) {
                    fprintf(cg->out, " = ");
                    gen_expr(cg, node->children[0]);
                } else if (node->vtype == TYPE_STRING) {
                    fprintf(cg->out, " = NULL");
                }
                fprintf(cg->out, ";\n");
            } else if (node->child_count > 0) {
                /* It was already declared (as param): emit as assignment */
                do_indent(cg);
                fprintf(cg->out, "%s = ", node->sval);
                gen_expr(cg, node->children[0]);
                fprintf(cg->out, ";\n");
            }
            /* else: bare re-decl of param with no init — silently skip */
            break;
        }

        /* ------ assignment ------ */
        case NODE_ASSIGN:
            if (node->child_count >= 2) {
                do_indent(cg);
                gen_expr(cg, node->children[0]);
                fprintf(cg->out, " = ");
                gen_expr(cg, node->children[1]);
                fprintf(cg->out, ";\n");
            }
            break;

        /* ------ output ------ */
        case NODE_OUTPUT:
            gen_output(cg, node);
            break;

        /* ------ if / else if / else ------ */
        case NODE_IF:
            gen_if(cg, node);
            break;

        /* ------ while ------ */
        case NODE_WHILE:
            do_indent(cg);
            fprintf(cg->out, "while (");
            gen_expr(cg, node->children[0]);
            fprintf(cg->out, ") {\n");
            cg->indent++;
            gen_block(cg, node->children[1]);
            cg->indent--;
            do_indent(cg);
            fprintf(cg->out, "}\n");
            break;

        /* ------ for ------ */
        case NODE_FOR: {
            if (node->child_count < 4) break;
            gen_stmt(cg, node->children[0]);
            do_indent(cg);
            fprintf(cg->out, "while (");
            gen_expr(cg, node->children[1]);
            fprintf(cg->out, ") {\n");
            cg->indent++;
            gen_block(cg, node->children[3]);
            gen_stmt(cg, node->children[2]);
            cg->indent--;
            do_indent(cg);
            fprintf(cg->out, "}\n");
            break;
        }

        /* ------ switch ------ */
        case NODE_SWITCH:
            do_indent(cg);
            fprintf(cg->out, "switch ((long long)(");
            gen_expr(cg, node->children[0]);
            fprintf(cg->out, ")) {\n");
            for (int i = 1; i < node->child_count; i++) {
                ASTNode *cas = node->children[i];
                if (cas->type != NODE_CASE) continue;
                do_indent(cg);
                if (cas->sval && strcmp(cas->sval, "default") == 0) {
                    fprintf(cg->out, "default:\n");
                    cg->indent++;
                    gen_block(cg, cas->children[0]);
                } else {
                    fprintf(cg->out, "case ");
                    gen_expr(cg, cas->children[0]);
                    fprintf(cg->out, ":\n");
                    cg->indent++;
                    gen_block(cg, cas->children[1]);
                }
                cg->indent--;
            }
            do_indent(cg);
            fprintf(cg->out, "}\n");
            break;

        /* ------ import → #include ------ */
        case NODE_IMPORT:
            for (int i = 0; i < node->child_count; i++) {
                ASTNode *h = node->children[i];
                if (h->sval)
                    fprintf(cg->out, "#include \"%s\"\n", h->sval);
            }
            break;

        case NODE_INC_DEC:
            do_indent(cg);
            gen_expr(cg, node);
            fprintf(cg->out, ";\n");
            break;

        case NODE_FUNC_CALL:
            do_indent(cg);
            gen_expr(cg, node);
            fprintf(cg->out, ";\n");
            break;

        default:
            do_indent(cg);
            gen_expr(cg, node);
            fprintf(cg->out, ";\n");
            break;
    }
}

static void gen_block(CodeGen *cg, ASTNode *node) {
    if (!node) return;
    for (int i = 0; i < node->child_count; i++)
        gen_stmt(cg, node->children[i]);
}

static VarType detect_return_type(ASTNode *block) {
    if (!block) return TYPE_VOID;
    for (int i = 0; i < block->child_count; i++) {
        ASTNode *s = block->children[i];
        if (!s) continue;
        if (s->type == NODE_RETURN && s->child_count > 0)
            return expr_type(s->children[0]);
        if (s->type == NODE_BLOCK || s->type == NODE_IF ||
            s->type == NODE_WHILE || s->type == NODE_FOR) {
            for (int j = 0; j < s->child_count; j++) {
                VarType t = detect_return_type(s->children[j]);
                if (t != TYPE_VOID) return t;
            }
        }
    }
    return TYPE_VOID;
}

static void emit_forward_decls(CodeGen *cg, ASTNode *root) {
    for (int i = 0; i < root->child_count; i++) {
        ASTNode *node = root->children[i];
        if (node->type != NODE_FUNCTION_DEF) continue;

        ASTNode *body = NULL;
        for (int j = 0; j < node->child_count; j++)
            if (node->children[j]->type == NODE_BLOCK) { body = node->children[j]; break; }

        VarType rtype = detect_return_type(body);
        if (strcmp(node->sval, "main") == 0) rtype = TYPE_INT;

        fprintf(cg->out, "%s %s(", vtype_to_c(rtype), node->sval);
        int first = 1;
        for (int j = 0; j < node->child_count; j++) {
            ASTNode *par = node->children[j];
            if (par->type != NODE_PARAM) continue;
            if (!first) fprintf(cg->out, ", ");
            first = 0;
            fprintf(cg->out, "%s %s", vtype_to_c(par->vtype), par->sval);
        }
        fprintf(cg->out, ");\n");
    }
    fprintf(cg->out, "\n");
}

/*
 * Collect user imports (NODE_IMPORT at top level) and emit them AFTER
 * the standard headers. "main.h" is special: it's not re-emitted here
 * (we handle it internally — xlang's main.h grants access to main()).
 */
static void emit_user_imports(CodeGen *cg, ASTNode *root) {
    for (int i = 0; i < root->child_count; i++) {
        ASTNode *node = root->children[i];
        if (node->type != NODE_IMPORT) continue;
        for (int j = 0; j < node->child_count; j++) {
            ASTNode *h = node->children[j];
            if (!h->sval) continue;
            /* Skip "main.h" — it is implicit in every xlang program */
            if (strcmp(h->sval, "main.h") == 0) continue;
            fprintf(cg->out, "#include \"%s\"\n", h->sval);
        }
    }
}

void codegen_run(CodeGen *cg, ASTNode *root) {
    if (!cg || !root) return;

    sym_reset();

    /* Standard headers */
    fprintf(cg->out, "#include <stdio.h>\n");
    fprintf(cg->out, "#include <stdlib.h>\n");
    fprintf(cg->out, "#include <string.h>\n");
    fprintf(cg->out, "#include \"runtime.h\"\n");

    /* User-specified imports */
    emit_user_imports(cg, root);
    fprintf(cg->out, "\n");

    /* Forward declarations */
    emit_forward_decls(cg, root);

    /* Generate each function */
    for (int i = 0; i < root->child_count; i++) {
        ASTNode *node = root->children[i];
        if (node->type != NODE_FUNCTION_DEF) continue;

        sym_reset(); /* fresh symbol table per function */

        /* Register parameters */
        for (int j = 0; j < node->child_count; j++) {
            ASTNode *par = node->children[j];
            if (par->type == NODE_PARAM && par->sval)
                sym_push(par->sval, par->vtype);
        }

        ASTNode *body = NULL;
        for (int j = 0; j < node->child_count; j++)
            if (node->children[j]->type == NODE_BLOCK) { body = node->children[j]; break; }

        VarType rtype = detect_return_type(body);
        if (strcmp(node->sval, "main") == 0) rtype = TYPE_INT;

        /* Signature */
        fprintf(cg->out, "%s %s(", vtype_to_c(rtype), node->sval);
        int first = 1;
        for (int j = 0; j < node->child_count; j++) {
            ASTNode *par = node->children[j];
            if (par->type != NODE_PARAM) continue;
            if (!first) fprintf(cg->out, ", ");
            first = 0;
            fprintf(cg->out, "%s %s", vtype_to_c(par->vtype), par->sval);
        }
        fprintf(cg->out, ") {\n");

        cg->indent = 1;
        if (body) gen_block(cg, body);

        /*
         * For non-void non-main functions that have 'skip' as their only
         * body (empty stub), emit a default return.
         */
        if (strcmp(node->sval, "main") != 0) {
            if (rtype != TYPE_VOID) {
                do_indent(cg);
                fprintf(cg->out, "return 0;\n");
            }
        } else {
            /* main always ends with return 0 (done keyword already emitted it,
               but guard against duplicate — the compiler is smart enough) */
            do_indent(cg);
            fprintf(cg->out, "return 0; /* safety */\n");
        }

        fprintf(cg->out, "}\n\n");
    }
}