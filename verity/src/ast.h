/* ============================================================
   ast.h  --  Verity Compiler : Abstract Syntax Tree
   BCSE307P Compiler Design Lab | Phase 2 (Parser)
   ============================================================ */
#ifndef VERITY_AST_H
#define VERITY_AST_H

typedef enum {
    NODE_PROGRAM,
    NODE_FUNC_DECL,
    NODE_PARAM,
    NODE_VAR_DECL,       /* type + one or more declarators           */
    NODE_DECLARATOR,     /* name, optional initializer                */
    NODE_BLOCK,

    NODE_IF,
    NODE_WHILE,
    NODE_RETURN,
    NODE_READ,
    NODE_WRITE,
    NODE_EXPR_STMT,

    NODE_ASSIGN,
    NODE_BINOP,
    NODE_UNOP,
    NODE_CALL,
    NODE_IDENT,
    NODE_INT_LIT,
    NODE_FLOAT_LIT,
    NODE_BOOL_LIT
} NodeKind;

typedef struct Node {
    NodeKind      kind;
    int           line;

    char         *str;      /* identifier name / operator text / type name */
    long          ival;     /* integer literal value                       */
    double        fval;     /* float literal value                         */

    struct Node **children;
    int           nchild;
    int           cap;
} Node;

Node *node_new(NodeKind kind, int line);
void  node_add_child(Node *parent, Node *child);
void  node_set_str(Node *n, const char *s);
void  ast_print(Node *root);
void  ast_free(Node *n);

#endif /* VERITY_AST_H */
