/* ============================================================
   parser.h  --  Verity Compiler : Recursive-Descent LL(1) Parser
   BCSE307P Compiler Design Lab | Phase 2
   ============================================================ */
#ifndef VERITY_PARSER_H
#define VERITY_PARSER_H

#include "lexer.h"
#include "ast.h"

typedef struct {
    Lexer  *lx;
    Token   cur;       /* current lookahead token (1-token lookahead: LL(1)) */
    int     error_count;
} Parser;

void   parser_init(Parser *p, Lexer *lx);
Node  *parser_parse_program(Parser *p); /* returns AST root, or NULL on fatal failure */

#endif /* VERITY_PARSER_H */
