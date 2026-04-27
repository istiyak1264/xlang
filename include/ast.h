#ifndef XLANG_AST_H
#define XLANG_AST_H

typedef enum {
    TYPE_UNKNOWN, TYPE_INT, TYPE_FLOAT, TYPE_DOUBLE,
    TYPE_STRING, TYPE_ARRAY, TYPE_VOID
} VarType;

typedef enum {
    NODE_PROGRAM,
    NODE_IMPORT,
    NODE_FUNCTION_DEF,
    NODE_PARAM,
    NODE_BLOCK,
    NODE_SKIP,
    NODE_RETURN,
    NODE_BREAK,
    NODE_DONE,
    NODE_VAR_DECL,
    NODE_ASSIGN,
    NODE_IF,
    NODE_WHILE,
    NODE_FOR,
    NODE_SWITCH,
    NODE_CASE,
    NODE_OUTPUT,
    NODE_INPUT,
    NODE_FUNC_CALL,
    NODE_BINOP,
    NODE_UNOP,
    NODE_IDENT,
    NODE_INT_LIT,
    NODE_FLOAT_LIT,
    NODE_STRING_LIT,
    NODE_ARRAY_LIT,
    NODE_INDEX,
    NODE_INC_DEC
} NodeType;

typedef struct ASTNode {
    NodeType  type;
    int       line;
    char     *sval;
    long long ival;
    double    fval;
    VarType   vtype;
    int       is_else;
    struct ASTNode **children;
    int              child_count;
} ASTNode;

ASTNode *ast_node_new(NodeType type, int line);
void     ast_add_child(ASTNode *parent, ASTNode *child);
void     ast_free(ASTNode *node);
void     ast_print(ASTNode *node, int indent);

#endif