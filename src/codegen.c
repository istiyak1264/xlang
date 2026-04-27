#include "codegen.h"
#include "ast.h"
#include "error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

CodeGen *codegen_new(FILE *out) {
    CodeGen *cg = calloc(1, sizeof(CodeGen));
    cg->out = out;
    cg->needs_string_h = 0;
    cg->needs_math_h = 0;
    return cg;
}

void codegen_free(CodeGen *cg) { 
    if (cg) free(cg); 
}

static void indent(CodeGen *cg) {
    for (int i = 0; i < cg->indent; i++) 
        fprintf(cg->out, "    ");
}

static const char *vtype_to_c(VarType t) {
    switch(t) {
        case TYPE_INT:     return "int";
        case TYPE_FLOAT:   return "float";
        case TYPE_DOUBLE:  return "double";
        case TYPE_STRING:  return "char*";
        case TYPE_ARRAY:   return "void*";
        case TYPE_VOID:    return "void";
        default:           return "int";
    }
}

static const char *op_to_c(const char *op) {
    if (!op) return "";
    if (strcmp(op, "==") == 0) return "==";
    if (strcmp(op, "!=") == 0) return "!=";
    if (strcmp(op, ">") == 0) return ">";
    if (strcmp(op, "<") == 0) return "<";
    if (strcmp(op, ">=") == 0) return ">=";
    if (strcmp(op, "<=") == 0) return "<=";
    if (strcmp(op, "&&") == 0) return "&&";
    if (strcmp(op, "||") == 0) return "||";
    if (strcmp(op, "^") == 0) return "^";
    if (strcmp(op, "!") == 0) return "!";
    return op;
}

/* Forward declarations */
static void gen_expr(CodeGen *cg, ASTNode *node);
static void gen_stmt(CodeGen *cg, ASTNode *node);
static void gen_block(CodeGen *cg, ASTNode *node);

/* Expression code generation */
static void gen_expr(CodeGen *cg, ASTNode *node) {
    if (!node) {
        fprintf(cg->out, "0");
        return;
    }
    
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
            cg->needs_string_h = 1;
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
            fprintf(cg->out, "%s(", op_to_c(node->sval));
            gen_expr(cg, node->children[0]);
            fprintf(cg->out, ")");
            break;
            
        case NODE_INC_DEC:
            gen_expr(cg, node->children[0]);
            fprintf(cg->out, "%s", node->sval);
            break;
            
        case NODE_INDEX: {
            /* Array indexing - support both 1D and 2D */
            fprintf(cg->out, "(*(");
            if (node->child_count == 1) {
                /* 1D array */
                fprintf(cg->out, "int*)");
                gen_expr(cg, node->children[0]);
                fprintf(cg->out, "[");
                gen_expr(cg, node->children[0]);
                fprintf(cg->out, "]");
            } else if (node->child_count == 2) {
                /* 2D array */
                fprintf(cg->out, "int(*)[");
                gen_expr(cg, node->children[1]);
                fprintf(cg->out, "])");
                gen_expr(cg, node->children[0]);
                fprintf(cg->out, "[");
                gen_expr(cg, node->children[0]);
                fprintf(cg->out, "][");
                gen_expr(cg, node->children[1]);
                fprintf(cg->out, "]");
            }
            break;
        }
        
        case NODE_ARRAY_LIT: {
            /* Array literal */
            fprintf(cg->out, "{");
            for (int i = 0; i < node->child_count; i++) {
                if (i > 0) fprintf(cg->out, ", ");
                if (node->children[i]->type == NODE_ARRAY_LIT) {
                    /* Nested array (2D) */
                    fprintf(cg->out, "{");
                    for (int j = 0; j < node->children[i]->child_count; j++) {
                        if (j > 0) fprintf(cg->out, ", ");
                        gen_expr(cg, node->children[i]->children[j]);
                    }
                    fprintf(cg->out, "}");
                } else {
                    gen_expr(cg, node->children[i]);
                }
            }
            fprintf(cg->out, "}");
            break;
        }
        
        default:
            error(node->line, "Unknown expression type in codegen: %d", node->type);
            fprintf(cg->out, "0");
            break;
    }
}

/* Statement code generation */
static void gen_stmt(CodeGen *cg, ASTNode *node) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_BLOCK:
            gen_block(cg, node);
            break;
            
        case NODE_SKIP:
            indent(cg);
            fprintf(cg->out, "/* skip */\n");
            break;
            
        case NODE_BREAK:
            indent(cg);
            fprintf(cg->out, "break;\n");
            break;
            
        case NODE_DONE:
            indent(cg);
            fprintf(cg->out, "return 0;\n");
            break;
            
        case NODE_RETURN:
            indent(cg);
            fprintf(cg->out, "return");
            if (node->child_count > 0) {
                fprintf(cg->out, " ");
                gen_expr(cg, node->children[0]);
            }
            fprintf(cg->out, ";\n");
            break;
            
        case NODE_VAR_DECL: {
            indent(cg);
            const char *type_str = vtype_to_c(node->vtype);
            
            /* Handle array declarations specially */
            if (node->vtype == TYPE_ARRAY) {
                if (node->child_count > 0 && node->children[0]->type == NODE_ARRAY_LIT) {
                    /* Array with initializer */
                    ASTNode *arr = node->children[0];
                    if (arr->child_count > 0 && arr->children[0]->type == NODE_ARRAY_LIT) {
                        /* 2D array */
                        int rows = arr->child_count;
                        int cols = arr->children[0]->child_count;
                        fprintf(cg->out, "int %s[%d][%d] = ", node->sval, rows, cols);
                        gen_expr(cg, arr);
                        fprintf(cg->out, ";\n");
                    } else {
                        /* 1D array */
                        int size = arr->child_count;
                        fprintf(cg->out, "int %s[%d] = ", node->sval, size);
                        gen_expr(cg, arr);
                        fprintf(cg->out, ";\n");
                    }
                } else {
                    /* Uninitialized array - use runtime */
                    fprintf(cg->out, "XLangArray* %s = xlang_array_new(TYPE_INT);\n", node->sval);
                    cg->needs_string_h = 1;
                }
            } else {
                /* Regular variable declaration */
                fprintf(cg->out, "%s %s", type_str, node->sval);
                if (node->child_count > 0) {
                    fprintf(cg->out, " = ");
                    gen_expr(cg, node->children[0]);
                }
                fprintf(cg->out, ";\n");
            }
            break;
        }
        
        case NODE_ASSIGN: {
            indent(cg);
            if (node->child_count >= 2) {
                gen_expr(cg, node->children[0]);
                fprintf(cg->out, " = ");
                gen_expr(cg, node->children[1]);
                fprintf(cg->out, ";\n");
            }
            break;
        }
        
        case NODE_INC_DEC:
            indent(cg);
            gen_expr(cg, node->children[0]);
            fprintf(cg->out, "%s;\n", node->sval);
            break;
            
        case NODE_FUNC_CALL:
            indent(cg);
            gen_expr(cg, node);
            fprintf(cg->out, ";\n");
            break;
            
        case NODE_OUTPUT:
            indent(cg);
            fprintf(cg->out, "printf(");
            if (node->child_count == 1) {
                /* Single argument - determine format */
                ASTNode *arg = node->children[0];
                if (arg->type == NODE_STRING_LIT) {
                    fprintf(cg->out, "\"%s\"", arg->sval);
                } else if (arg->type == NODE_INT_LIT) {
                    fprintf(cg->out, "\"%%lld\", %lld", arg->ival, arg->ival);
                } else if (arg->type == NODE_FLOAT_LIT) {
                    fprintf(cg->out, "\"%%g\", %g", arg->fval);
                } else {
                    fprintf(cg->out, "\"%%d\", ");
                    gen_expr(cg, arg);
                }
            } else {
                /* Multiple arguments - build format string */
                fprintf(cg->out, "\"");
                for (int i = 0; i < node->child_count; i++) {
                    ASTNode *arg = node->children[i];
                    if (arg->type == NODE_STRING_LIT)
                        fprintf(cg->out, "%%s");
                    else if (arg->type == NODE_INT_LIT)
                        fprintf(cg->out, "%%d");
                    else if (arg->type == NODE_FLOAT_LIT)
                        fprintf(cg->out, "%%g");
                    else
                        fprintf(cg->out, "%%d");
                    if (i < node->child_count - 1) fprintf(cg->out, " ");
                }
                fprintf(cg->out, "\"");
                for (int i = 0; i < node->child_count; i++) {
                    fprintf(cg->out, ", ");
                    gen_expr(cg, node->children[i]);
                }
            }
            fprintf(cg->out, ");\n");
            fprintf(cg->out, "printf(\"\\n\");\n");
            break;
            
        case NODE_IF: {
            indent(cg);
            if (node->is_else == 0) {
                /* Regular if statement */
                fprintf(cg->out, "if (");
                if (node->child_count > 0) {
                    gen_expr(cg, node->children[0]);
                }
                fprintf(cg->out, ") {\n");
                cg->indent++;
                if (node->child_count > 1) {
                    gen_block(cg, node->children[1]);
                }
                cg->indent--;
                indent(cg);
                fprintf(cg->out, "}");
                
                /* Handle else-if and else branches */
                for (int i = 2; i < node->child_count; i++) {
                    ASTNode *branch = node->children[i];
                    if (branch->is_else == 1) {
                        /* else-if */
                        fprintf(cg->out, " else if (");
                        if (branch->child_count > 0) {
                            gen_expr(cg, branch->children[0]);
                        }
                        fprintf(cg->out, ") {\n");
                        cg->indent++;
                        if (branch->child_count > 1) {
                            gen_block(cg, branch->children[1]);
                        }
                        cg->indent--;
                        indent(cg);
                        fprintf(cg->out, "}");
                    } else if (branch->is_else == 2) {
                        /* else */
                        fprintf(cg->out, " else {\n");
                        cg->indent++;
                        if (branch->child_count > 0) {
                            gen_block(cg, branch->children[0]);
                        }
                        cg->indent--;
                        indent(cg);
                        fprintf(cg->out, "}");
                    }
                }
                fprintf(cg->out, "\n");
            }
            break;
        }
        
        case NODE_WHILE:
            indent(cg);
            fprintf(cg->out, "while (");
            if (node->child_count > 0) {
                gen_expr(cg, node->children[0]);
            }
            fprintf(cg->out, ") {\n");
            cg->indent++;
            if (node->child_count > 1) {
                gen_block(cg, node->children[1]);
            }
            cg->indent--;
            indent(cg);
            fprintf(cg->out, "}\n");
            break;
            
        case NODE_FOR:
            indent(cg);
            fprintf(cg->out, "for (");
            /* Initialization */
            if (node->child_count > 0) {
                ASTNode *init = node->children[0];
                if (init->type == NODE_VAR_DECL) {
                    fprintf(cg->out, "%s %s", vtype_to_c(init->vtype), init->sval);
                    if (init->child_count > 0) {
                        fprintf(cg->out, " = ");
                        gen_expr(cg, init->children[0]);
                    }
                } else if (init->type == NODE_ASSIGN) {
                    if (init->child_count >= 2) {
                        gen_expr(cg, init->children[0]);
                        fprintf(cg->out, " = ");
                        gen_expr(cg, init->children[1]);
                    }
                } else {
                    gen_expr(cg, init);
                }
            }
            fprintf(cg->out, "; ");
            /* Condition */
            if (node->child_count > 1) {
                gen_expr(cg, node->children[1]);
            }
            fprintf(cg->out, "; ");
            /* Update */
            if (node->child_count > 2) {
                ASTNode *upd = node->children[2];
                if (upd->type == NODE_ASSIGN) {
                    if (upd->child_count >= 2) {
                        gen_expr(cg, upd->children[0]);
                        fprintf(cg->out, " = ");
                        gen_expr(cg, upd->children[1]);
                    }
                } else {
                    gen_expr(cg, upd);
                }
            }
            fprintf(cg->out, ") {\n");
            cg->indent++;
            if (node->child_count > 3) {
                gen_block(cg, node->children[3]);
            }
            cg->indent--;
            indent(cg);
            fprintf(cg->out, "}\n");
            break;
            
        case NODE_SWITCH:
            indent(cg);
            fprintf(cg->out, "switch (");
            if (node->child_count > 0) {
                gen_expr(cg, node->children[0]);
            }
            fprintf(cg->out, ") {\n");
            cg->indent++;
            for (int i = 1; i < node->child_count; i++) {
                ASTNode *cas = node->children[i];
                indent(cg);
                if (cas->sval && strcmp(cas->sval, "default") == 0) {
                    fprintf(cg->out, "default:\n");
                    cg->indent++;
                    if (cas->child_count > 0) {
                        gen_block(cg, cas->children[0]);
                    }
                    indent(cg);
                    fprintf(cg->out, "break;\n");
                    cg->indent--;
                } else {
                    fprintf(cg->out, "case ");
                    if (cas->child_count > 0) {
                        gen_expr(cg, cas->children[0]);
                    }
                    fprintf(cg->out, ":\n");
                    cg->indent++;
                    if (cas->child_count > 1) {
                        gen_block(cg, cas->children[1]);
                    }
                    indent(cg);
                    fprintf(cg->out, "break;\n");
                    cg->indent--;
                }
            }
            cg->indent--;
            indent(cg);
            fprintf(cg->out, "}\n");
            break;
            
        default:
            /* Expression statement */
            indent(cg);
            gen_expr(cg, node);
            fprintf(cg->out, ";\n");
            break;
    }
}

static void gen_block(CodeGen *cg, ASTNode *node) {
    if (!node) return;
    for (int i = 0; i < node->child_count; i++) {
        gen_stmt(cg, node->children[i]);
    }
}

/* Collect imports to determine required headers */
static void collect_imports(ASTNode *node, int *need_string, int *need_math, int *need_stdio) {
    if (!node) return;
    
    if (node->type == NODE_IMPORT) {
        for (int i = 0; i < node->child_count; i++) {
            if (node->children[i]->sval) {
                const char *import = node->children[i]->sval;
                if (strstr(import, "string") || strstr(import, "string.h"))
                    *need_string = 1;
                if (strstr(import, "math") || strstr(import, "math.h"))
                    *need_math = 1;
                if (strstr(import, "stdio") || strstr(import, "stdio.h"))
                    *need_stdio = 1;
            }
        }
    }
    
    for (int i = 0; i < node->child_count; i++) {
        collect_imports(node->children[i], need_string, need_math, need_stdio);
    }
}

/* Detect function return type from body */
static VarType detect_return_type(ASTNode *block) {
    if (!block) return TYPE_VOID;
    
    for (int i = 0; i < block->child_count; i++) {
        ASTNode *s = block->children[i];
        if (s && s->type == NODE_RETURN && s->child_count > 0) {
            ASTNode *expr = s->children[0];
            if (expr->type == NODE_INT_LIT) return TYPE_INT;
            if (expr->type == NODE_FLOAT_LIT) return TYPE_FLOAT;
            if (expr->type == NODE_STRING_LIT) return TYPE_STRING;
            if (expr->type == NODE_IDENT) return TYPE_INT; /* Assume int by default */
            return TYPE_INT;
        }
    }
    return TYPE_VOID;
}

/* Main code generation entry point */
void codegen_run(CodeGen *cg, ASTNode *root) {
    if (!cg || !root) return;
    
    int need_stdio = 1;  /* Always need stdio for output */
    int need_string = 0;
    int need_math = 0;
    collect_imports(root, &need_string, &need_math, &need_stdio);
    
    /* Generate C file header */
    fprintf(cg->out, 
        "/***************************************************************\n"
        " * Auto-generated code by XLang Compiler\n"
        " * DO NOT EDIT MANUALLY\n"
        " ***************************************************************/\n\n");
    
    /* Standard headers */
    fprintf(cg->out, "#include <stdio.h>\n");
    fprintf(cg->out, "#include <stdlib.h>\n");
    fprintf(cg->out, "#include <string.h>\n");
    
    if (need_math) {
        fprintf(cg->out, "#include <math.h>\n");
    }
    
    /* XLang runtime */
    fprintf(cg->out, "#include \"runtime.h\"\n\n");
    
    /* Runtime helper functions (if not using external runtime) */
    fprintf(cg->out,
        "/* XLang Runtime Helpers */\n"
        "static char* xlang_input_string(void) {\n"
        "    char *buf = malloc(4096);\n"
        "    if (!buf) return NULL;\n"
        "    if (fgets(buf, 4096, stdin)) {\n"
        "        size_t l = strlen(buf);\n"
        "        if (l > 0 && buf[l-1] == '\\n') buf[l-1] = '\\0';\n"
        "    }\n"
        "    return buf;\n"
        "}\n\n");
    
    /* Generate all functions */
    for (int i = 0; i < root->child_count; i++) {
        ASTNode *node = root->children[i];
        
        if (node->type == NODE_FUNCTION_DEF) {
            /* Find body and parameters */
            ASTNode *body = NULL;
            int param_count = 0;
            
            for (int j = 0; j < node->child_count; j++) {
                if (node->children[j]->type == NODE_PARAM) {
                    param_count++;
                } else if (node->children[j]->type == NODE_BLOCK) {
                    body = node->children[j];
                }
            }
            
            /* Determine return type */
            VarType rtype = detect_return_type(body);
            if (strcmp(node->sval, "main") == 0) {
                rtype = TYPE_INT;
            }
            
            /* Function header */
            fprintf(cg->out, "%s %s(", vtype_to_c(rtype), node->sval);
            
            /* Parameters */
            int first_param = 1;
            for (int j = 0; j < node->child_count; j++) {
                ASTNode *p = node->children[j];
                if (p->type != NODE_PARAM) continue;
                
                if (!first_param) {
                    fprintf(cg->out, ", ");
                }
                first_param = 0;
                fprintf(cg->out, "%s %s", vtype_to_c(p->vtype), p->sval);
            }
            
            /* Close function header */
            fprintf(cg->out, ") {\n");
            
            /* Function body */
            cg->indent = 1;
            if (body) {
                gen_block(cg, body);
            }
            
            /* Add implicit return for main */
            if (strcmp(node->sval, "main") == 0) {
                indent(cg);
                fprintf(cg->out, "return 0;\n");
            }
            
            fprintf(cg->out, "}\n\n");
        }
    }
    
    /* If no main function defined, generate error */
    int has_main = 0;
    for (int i = 0; i < root->child_count; i++) {
        ASTNode *node = root->children[i];
        if (node->type == NODE_FUNCTION_DEF && node->sval && 
            strcmp(node->sval, "main") == 0) {
            has_main = 1;
            break;
        }
    }
    
    if (!has_main) {
        fprintf(cg->out,
            "int main(void) {\n"
            "    fprintf(stderr, \"Error: No main function defined\\n\");\n"
            "    return 1;\n"
            "}\n");
    }
}