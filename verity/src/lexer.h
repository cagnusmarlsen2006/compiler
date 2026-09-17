/* ============================================================
   lexer.h  --  Verity Compiler : Lexical Analyzer interface
   BCSE307P Compiler Design Lab | Phase 2
   ============================================================ */
#ifndef VERITY_LEXER_H
#define VERITY_LEXER_H

#define MAX_LEXEME 128

typedef enum {
    /* end / error */
    TOK_EOF = 0,
    TOK_ERROR,

    /* literals / identifier */
    TOK_IDENT,
    TOK_INT_CONST,
    TOK_FLOAT_CONST,

    /* keywords */
    TOK_INT, TOK_FLOAT, TOK_CHAR, TOK_BOOL, TOK_VOID,
    TOK_IF, TOK_ELSE, TOK_WHILE, TOK_RETURN,
    TOK_READ, TOK_WRITE, TOK_TRUE, TOK_FALSE,

    /* operators */
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT,
    TOK_ASSIGN, TOK_EQ, TOK_NEQ,
    TOK_LT, TOK_LE, TOK_GT, TOK_GE,
    TOK_AND, TOK_OR,

    /* punctuation */
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE,
    TOK_SEMI, TOK_COMMA,

    TOK_COUNT /* sentinel */
} TokenType;

typedef struct {
    TokenType type;
    char      lexeme[MAX_LEXEME];
    int       line;
} Token;

typedef struct {
    const char *src;   /* full source buffer, NUL terminated   */
    int         pos;   /* current offset into src              */
    int         line;  /* current line number (1-based)        */
    int         len;   /* length of src                        */
    int         error_count;
    int         token_count;
} Lexer;

void        lexer_init(Lexer *lx, const char *src);
Token       lexer_next(Lexer *lx);
const char *token_type_name(TokenType t);

#endif /* VERITY_LEXER_H */
