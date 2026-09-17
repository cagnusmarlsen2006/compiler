/* ============================================================
   ast.c  --  Verity Compiler : Abstract Syntax Tree helpers
   ============================================================ */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

static const char *kind_name(NodeKind k) {
    switch (k) {
        case NODE_PROGRAM:    return "Program";
        case NODE_FUNC_DECL:  return "FuncDecl";
        case NODE_PARAM:      return "Param";
        case NODE_VAR_DECL:   return "VarDecl";
        case NODE_DECLARATOR: return "Declarator";
        case NODE_BLOCK:      return "Block";
        case NODE_IF:         return "If";
        case NODE_WHILE:      return "While";
        case NODE_RETURN:     return "Return";
        case NODE_READ:       return "Read";
        case NODE_WRITE:      return "Write";
        case NODE_EXPR_STMT:  return "ExprStmt";
        case NODE_ASSIGN:     return "Assign";
        case NODE_BINOP:      return "BinOp";
        case NODE_UNOP:       return "UnOp";
        case NODE_CALL:       return "Call";
        case NODE_IDENT:      return "Ident";
        case NODE_INT_LIT:    return "IntLit";
        case NODE_FLOAT_LIT:  return "FloatLit";
        case NODE_BOOL_LIT:   return "BoolLit";
        default:              return "?";
    }
}

Node *node_new(NodeKind kind, int line) {
    Node *n = (Node *)calloc(1, sizeof(Node));
    n->kind = kind;
    n->line = line;
    n->str = NULL;
    n->children = NULL;
    n->nchild = 0;
    n->cap = 0;
    return n;
}

void node_set_str(Node *n, const char *s) {
    if (n->str) free(n->str);
    n->str = s ? strdup(s) : NULL;
}

void node_add_child(Node *parent, Node *child) {
    if (!child) return;
    if (parent->nchild == parent->cap) {
        parent->cap = parent->cap ? parent->cap * 2 : 4;
        parent->children = (Node **)realloc(parent->children, parent->cap * sizeof(Node *));
    }
    parent->children[parent->nchild++] = child;
}

static void print_rec(Node *n, int depth, int is_last, char *prefix) {
    if (!n) return;

    printf("%s", prefix);
    if (depth > 0) printf("%s", is_last ? "`-- " : "|-- ");

    printf("%s", kind_name(n->kind));
    if (n->str && n->str[0]) printf(" (%s)", n->str);
    if (n->kind == NODE_INT_LIT)   printf(" = %ld", n->ival);
    if (n->kind == NODE_FLOAT_LIT) printf(" = %g", n->fval);
    if (n->kind == NODE_BOOL_LIT)  printf(" = %s", n->ival ? "true" : "false");
    printf("   [line %d]\n", n->line);

    char child_prefix[512];
    snprintf(child_prefix, sizeof(child_prefix), "%s%s", prefix,
              depth == 0 ? "" : (is_last ? "    " : "|   "));

    for (int i = 0; i < n->nchild; i++)
        print_rec(n->children[i], depth + 1, i == n->nchild - 1, child_prefix);
}

void ast_print(Node *root) {
    print_rec(root, 0, 1, "");
}

void ast_free(Node *n) {
    if (!n) return;
    for (int i = 0; i < n->nchild; i++) ast_free(n->children[i]);
    free(n->children);
    if (n->str) free(n->str);
    free(n);
}
