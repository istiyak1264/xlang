#include "codegen.h"
#include "ast.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

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
        case TYPE_ARRAY:  return "void*";
        case TYPE_VOID:   return "void";
        default:          return "long long";
    }
}

/* printf format specifier for a VarType */
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
    if (strcmp(op, "==") == 0) return "==";
    if (strcmp(op, "!=") == 0) return "!=";
    if (strcmp(op, ">")  == 0) return ">";
    if (strcmp(op, "<")  == 0) return "<";
    if (strcmp(op, ">=") == 0) return ">=";
    if (strcmp(op, "<=") == 0) return "<=";
    if (strcmp(op, "&&") == 0) return "&&";
    if (strcmp(op, "||") == 0) return "||";
    return op;
}

static VarType expr_type(ASTNode *node) {
    if (!node) return TYPE_INT;
    switch (node->type) {
        case NODE_INT_LIT:    return TYPE_INT;
        case NODE_FLOAT_LIT:  return TYPE_FLOAT;
        case NODE_STRING_LIT: return TYPE_STRING;
        case NODE_INPUT:      return TYPE_STRING;
        case NODE_IDENT:      return TYPE_INT;
        case NODE_BINOP:      return expr_type(node->children[0]);
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
            /* xlang_array_get returns void*, cast to int* then dereference */
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

static void gen_output(CodeGen *cg, ASTNode *node) {
    for (int i = 0; i < node->child_count; i++) {
        ASTNode *arg = node->children[i];
        do_indent(cg);
        if (arg->type == NODE_STRING_LIT) {
            /* Print the string then a newline */
            fprintf(cg->out, "printf(\"%%s\\n\", \"%s\");\n", arg->sval);
        } else if (arg->type == NODE_INT_LIT) {
            fprintf(cg->out, "printf(\"%%lld\\n\", (long long)%lld);\n", arg->ival);
        } else if (arg->type == NODE_FLOAT_LIT) {
            fprintf(cg->out, "printf(\"%%g\\n\", (double)%g);\n", arg->fval);
        } else {
            /* General expression: use type heuristic for format string */
            VarType t = expr_type(arg);
            fprintf(cg->out, "printf(\"%s\\n\", ", vtype_fmt(t));
            if (t == TYPE_INT) fprintf(cg->out, "(long long)");
            gen_expr(cg, arg);
            fprintf(cg->out, ");\n");
        }
    }
}

static void gen_if(CodeGen *cg, ASTNode *node) {
    /* node->is_else:  0 = if,  1 = else if,  2 = bare else */
    if (node->is_else == 2) {
        /* bare else – child[0] is the body block */
        fprintf(cg->out, " else {\n");
        cg->indent++;
        gen_block(cg, node->children[0]);
        cg->indent--;
        do_indent(cg);
        fprintf(cg->out, "}");
    } else if (node->is_else == 1) {
        /* else if – child[0]=cond, child[1]=body, child[2..]=chain */
        fprintf(cg->out, " else if (");
        gen_expr(cg, node->children[0]);
        fprintf(cg->out, ") {\n");
        cg->indent++;
        gen_block(cg, node->children[1]);
        cg->indent--;
        do_indent(cg);
        fprintf(cg->out, "}");
        /* continue chain */
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
            fprintf(cg->out, "continue;\n");
            break;

        case NODE_BREAK:
            do_indent(cg);
            fprintf(cg->out, "break;\n");
            break;

        /* done → return 0 (always, even outside main) */
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
        case NODE_VAR_DECL:
            do_indent(cg);
            fprintf(cg->out, "%s %s", vtype_to_c(node->vtype), node->sval);
            if (node->child_count > 0) {
                fprintf(cg->out, " = ");
                gen_expr(cg, node->children[0]);
            }
            fprintf(cg->out, ";\n");
            break;

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
            /* emit initialiser as a standalone statement */
            gen_stmt(cg, node->children[0]);
            do_indent(cg);
            fprintf(cg->out, "while (");
            gen_expr(cg, node->children[1]);
            fprintf(cg->out, ") {\n");
            cg->indent++;
            gen_block(cg, node->children[3]);
            /* update at end of loop body */
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

        /* ------ inc/dec as statement ------ */
        case NODE_INC_DEC:
            do_indent(cg);
            gen_expr(cg, node);
            fprintf(cg->out, ";\n");
            break;

        /* ------ function call as statement ------ */
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
        /* recurse into nested blocks */
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

void codegen_run(CodeGen *cg, ASTNode *root) {
    if (!cg || !root) return;

    /* Standard headers */
    fprintf(cg->out, "#include <stdio.h>\n");
    fprintf(cg->out, "#include <stdlib.h>\n");
    fprintf(cg->out, "#include <string.h>\n");
    fprintf(cg->out, "#include \"runtime.h\"\n\n");

    /* Forward declarations */
    emit_forward_decls(cg, root);

    /* Generate each function */
    for (int i = 0; i < root->child_count; i++) {
        ASTNode *node = root->children[i];
        if (node->type != NODE_FUNCTION_DEF) continue;

        /* Find body */
        ASTNode *body = NULL;
        for (int j = 0; j < node->child_count; j++)
            if (node->children[j]->type == NODE_BLOCK) { body = node->children[j]; break; }

        /* Return type */
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

        /* Body */
        cg->indent = 1;
        if (body) gen_block(cg, body);

        /* Implicit return 0 for main (in case user used 'done' keyword or omitted) */
        if (strcmp(node->sval, "main") == 0) {
            do_indent(cg);
            fprintf(cg->out, "return 0;\n");
        }

        fprintf(cg->out, "}\n\n");
    }
}