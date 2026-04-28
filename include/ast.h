#ifndef XLANG_AST_H
#define XLANG_AST_H

typedef enum {
    TYPE_UNKNOWN = 0,
    TYPE_INT     = 1,
    TYPE_FLOAT   = 2,
    TYPE_DOUBLE  = 3,
    TYPE_STRING  = 4,
    TYPE_ARRAY   = 5,
    TYPE_VOID    = 6
} VarType;

typedef enum {
    ELSE_NONE = 0,   /* leading if */
    ELSE_IF   = 1,   /* else if branch */
    ELSE_BARE = 2    /* bare else */
} ElseKind;

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
    ElseKind  else_kind;
    struct ASTNode **children;
    int              child_count;
    int              child_cap;
} ASTNode;

ASTNode *ast_node_new(NodeType type, int line);
void     ast_add_child(ASTNode *parent, ASTNode *child);
void     ast_free(ASTNode *node);
void     ast_print(ASTNode *node, int indent);

#endif