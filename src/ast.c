#include "ast.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

ASTNode *ast_node_new(NodeType type, int line) {
    ASTNode *n = calloc(1, sizeof(ASTNode));
    n->type  = type;
    n->line  = line;
    return n;
}

void ast_add_child(ASTNode *parent, ASTNode *child) {
    if (!child) return;
    parent->children = realloc(parent->children,
                               sizeof(ASTNode*) * (parent->child_count + 1));
    parent->children[parent->child_count++] = child;
}

void ast_free(ASTNode *node) {
    if (!node) return;
    for (int i = 0; i < node->child_count; i++)
        ast_free(node->children[i]);
    free(node->children);
    free(node->sval);
    free(node);
}

static const char *node_type_name(NodeType t) {
    switch(t) {
        case NODE_PROGRAM: return "PROGRAM";
        case NODE_IMPORT: return "IMPORT";
        case NODE_FUNCTION_DEF: return "FUNCTION_DEF";
        case NODE_PARAM: return "PARAM";
        case NODE_BLOCK: return "BLOCK";
        case NODE_SKIP: return "SKIP";
        case NODE_RETURN: return "RETURN";
        case NODE_BREAK: return "BREAK";
        case NODE_DONE: return "DONE";
        case NODE_VAR_DECL: return "VAR_DECL";
        case NODE_ASSIGN: return "ASSIGN";
        case NODE_IF: return "IF";
        case NODE_WHILE: return "WHILE";
        case NODE_FOR: return "FOR";
        case NODE_SWITCH: return "SWITCH";
        case NODE_CASE: return "CASE";
        case NODE_OUTPUT: return "OUTPUT";
        case NODE_INPUT: return "INPUT";
        case NODE_FUNC_CALL: return "FUNC_CALL";
        case NODE_BINOP: return "BINOP";
        case NODE_UNOP: return "UNOP";
        case NODE_IDENT: return "IDENT";
        case NODE_INT_LIT: return "INT_LIT";
        case NODE_FLOAT_LIT: return "FLOAT_LIT";
        case NODE_STRING_LIT: return "STRING_LIT";
        case NODE_ARRAY_LIT: return "ARRAY_LIT";
        case NODE_INDEX: return "INDEX";
        case NODE_INC_DEC: return "INC_DEC";
        default: return "?";
    }
}

void ast_print(ASTNode *node, int indent) {
    if (!node) return;
    for (int i = 0; i < indent; i++) printf("  ");
    printf("[%s]", node_type_name(node->type));
    if (node->sval) printf(" '%s'", node->sval);
    if (node->type == NODE_INT_LIT) printf(" %lld", node->ival);
    if (node->type == NODE_FLOAT_LIT) printf(" %g", node->fval);
    printf("\n");
    for (int i = 0; i < node->child_count; i++)
        ast_print(node->children[i], indent + 1);
}