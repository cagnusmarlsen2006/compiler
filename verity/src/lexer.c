/* ============================================================
   lexer.c  --  Verity Compiler : Lexical Analyzer
   BCSE307P Compiler Design Lab | Phase 1 Review-1 deliverable

   Hand-written, no lex/flex. Two token classes are recognized
   with EXPLICIT finite automata (named states + a transition
   switch), exactly as drawn on the "Lexical Analyzer - Design"
   slide:

     DFA-1  Identifier / Keyword
       S0 --letter/'_'--> S1 --(letter/digit/'_')*--> accept
       on accept: lookup keyword table; else IDENTIFIER

     DFA-2  Numeric Constant
       S0 --digit--> S1 --digit*--> (accept INT_CONST)
       S1 --'.' + lookahead digit--> S2 --digit--> S3
       S3 --digit*--> (accept FLOAT_CONST)
       (a '.' NOT followed by a digit is not consumed here,
        so "3." would stop the DFA at S1 and leave '.' for the
        next token -- lookahead prevents an ambiguous accept.)

   Everything else (operators, punctuation, comments, whitespace)
   is handled by direct character dispatch, which is how a real
   hand-written lexer typically treats single/double-char
   symbols rather than building a full DFA table for each.
   ============================================================ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "lexer.h"

/* ---------- keyword table ---------- */
typedef struct { const char *word; TokenType type; } KeywordEntry;

static const KeywordEntry KEYWORDS[] = {
    {"int",    TOK_INT},
    {"float",  TOK_FLOAT},
    {"char",   TOK_CHAR},
    {"bool",   TOK_BOOL},
    {"void",   TOK_VOID},
    {"if",     TOK_IF},
    {"else",   TOK_ELSE},
    {"while",  TOK_WHILE},
    {"return", TOK_RETURN},
    {"read",   TOK_READ},
    {"write",  TOK_WRITE},
    {"true",   TOK_TRUE},
    {"false",  TOK_FALSE},
};
#define NUM_KEYWORDS (int)(sizeof(KEYWORDS) / sizeof(KEYWORDS[0]))

static TokenType lookup_keyword(const char *lexeme) {
    for (int i = 0; i < NUM_KEYWORDS; i++)
        if (strcmp(lexeme, KEYWORDS[i].word) == 0)
            return KEYWORDS[i].type;
    return TOK_IDENT;
}

const char *token_type_name(TokenType t) {
    switch (t) {
        case TOK_EOF:         return "EOF";
        case TOK_ERROR:       return "ERROR";
        case TOK_IDENT:       return "IDENTIFIER";
        case TOK_INT_CONST:   return "INT_CONST";
        case TOK_FLOAT_CONST: return "FLOAT_CONST";
        case TOK_INT: case TOK_FLOAT: case TOK_CHAR: case TOK_BOOL:
        case TOK_VOID: case TOK_IF: case TOK_ELSE: case TOK_WHILE:
        case TOK_RETURN: case TOK_READ: case TOK_WRITE:
        case TOK_TRUE: case TOK_FALSE:
                               return "KEYWORD";
        case TOK_PLUS: case TOK_MINUS: case TOK_STAR: case TOK_SLASH:
        case TOK_PERCENT: case TOK_ASSIGN: case TOK_EQ: case TOK_NEQ:
        case TOK_LT: case TOK_LE: case TOK_GT: case TOK_GE:
        case TOK_AND: case TOK_OR:
                               return "OPERATOR";
        case TOK_LPAREN: case TOK_RPAREN: case TOK_LBRACE:
        case TOK_RBRACE: case TOK_SEMI: case TOK_COMMA:
                               return "PUNCTUATION";
        default:               return "UNKNOWN";
    }
}

/* ---------- lexer core ---------- */
void lexer_init(Lexer *lx, const char *src) {
    lx->src = src;
    lx->pos = 0;
    lx->line = 1;
    lx->len = (int)strlen(src);
    lx->error_count = 0;
    lx->token_count = 0;
}

static int peek(Lexer *lx)      { return lx->pos < lx->len ? (unsigned char)lx->src[lx->pos] : '\0'; }
static int peek2(Lexer *lx)     { return (lx->pos + 1) < lx->len ? (unsigned char)lx->src[lx->pos + 1] : '\0'; }
static int advance(Lexer *lx)   { int c = peek(lx); lx->pos++; if (c == '\n') lx->line++; return c; }

/* skip whitespace, line comments, and block comments; report unterminated block comments */
static void skip_trivia(Lexer *lx) {
    for (;;) {
        int c = peek(lx);
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance(lx);
        } else if (c == '/' && peek2(lx) == '/') {
            while (peek(lx) != '\n' && peek(lx) != '\0') advance(lx);
        } else if (c == '/' && peek2(lx) == '*') {
            int start_line = lx->line;
            advance(lx); advance(lx); /* consume the opening comment marker */
            while (!(peek(lx) == '*' && peek2(lx) == '/')) {
                if (peek(lx) == '\0') {
                    fprintf(stderr,
                        "[lexer] error line %d: unterminated block comment (opened here)\n",
                        start_line);
                    lx->error_count++;
                    return;
                }
                advance(lx);
            }
            advance(lx); advance(lx); /* consume closing */
        } else {
            break;
        }
    }
}

/* DFA-1 : identifier / keyword.  S0 already consumed (a letter/'_'). */
static Token scan_identifier(Lexer *lx, int start_line) {
    char buf[MAX_LEXEME];
    int n = 0;
    typedef enum { S1_IN_ID } State;
    State state = S1_IN_ID;
    buf[n++] = (char)advance(lx); /* the letter/_ that triggered S0->S1 */
    while (state == S1_IN_ID) {
        int c = peek(lx);
        if (isalnum(c) || c == '_') {          /* self-loop on S1 */
            if (n < MAX_LEXEME - 1) buf[n++] = (char)advance(lx);
            else advance(lx); /* overflow: keep scanning, truncate lexeme */
        } else {
            break; /* accept */
        }
    }
    buf[n] = '\0';
    Token t;
    t.type = lookup_keyword(buf);
    t.line = start_line;
    strncpy(t.lexeme, buf, MAX_LEXEME - 1);
    t.lexeme[MAX_LEXEME - 1] = '\0';
    return t;
}

/* DFA-2 : numeric constant.  S0 already consumed (a digit). */
static Token scan_number(Lexer *lx, int start_line) {
    char buf[MAX_LEXEME];
    int n = 0;
    typedef enum { S1_INT, S2_DOT, S3_FRAC } State;
    State state = S1_INT;
    int is_float = 0;

    buf[n++] = (char)advance(lx); /* the digit that triggered S0->S1 */

    for (;;) {
        int c = peek(lx);
        if (state == S1_INT) {
            if (isdigit(c)) {
                if (n < MAX_LEXEME - 1) buf[n++] = (char)advance(lx);
                else advance(lx);
            } else if (c == '.' && isdigit(peek2(lx))) {
                /* lookahead confirms a real fractional part -> S1 -> S2 */
                if (n < MAX_LEXEME - 1) buf[n++] = (char)advance(lx); /* consume '.' */
                state = S2_DOT;
            } else {
                break; /* accept as INT_CONST */
            }
        } else if (state == S2_DOT) {
            /* guaranteed a digit follows (checked above) : S2 -> S3 */
            if (n < MAX_LEXEME - 1) buf[n++] = (char)advance(lx);
            is_float = 1;
            state = S3_FRAC;
        } else { /* S3_FRAC : self-loop on digit */
            if (isdigit(c)) {
                if (n < MAX_LEXEME - 1) buf[n++] = (char)advance(lx);
                else advance(lx);
            } else {
                break; /* accept as FLOAT_CONST */
            }
        }
    }
    buf[n] = '\0';
    Token t;
    t.type = is_float ? TOK_FLOAT_CONST : TOK_INT_CONST;
    t.line = start_line;
    strncpy(t.lexeme, buf, MAX_LEXEME - 1);
    t.lexeme[MAX_LEXEME - 1] = '\0';
    return t;
}

static Token make_tok(TokenType type, const char *lex, int line) {
    Token t; t.type = type; t.line = line;
    strncpy(t.lexeme, lex, MAX_LEXEME - 1); t.lexeme[MAX_LEXEME - 1] = '\0';
    return t;
}

Token lexer_next(Lexer *lx) {
    skip_trivia(lx);
    int line = lx->line;

    if (lx->pos >= lx->len) {
        Token t = make_tok(TOK_EOF, "$", line);
        return t; /* not counted as a token */
    }

    int c = peek(lx);

    if (isalpha(c) || c == '_') { lx->token_count++; return scan_identifier(lx, line); }
    if (isdigit(c))             { lx->token_count++; return scan_number(lx, line); }

    /* operators & punctuation : direct dispatch, 1-char lookahead for 2-char ops */
    switch (c) {
        case '+': advance(lx); lx->token_count++; return make_tok(TOK_PLUS,   "+", line);
        case '-': advance(lx); lx->token_count++; return make_tok(TOK_MINUS,  "-", line);
        case '*': advance(lx); lx->token_count++; return make_tok(TOK_STAR,   "*", line);
        case '/': advance(lx); lx->token_count++; return make_tok(TOK_SLASH,  "/", line);
        case '%': advance(lx); lx->token_count++; return make_tok(TOK_PERCENT,"%", line);
        case '(': advance(lx); lx->token_count++; return make_tok(TOK_LPAREN, "(", line);
        case ')': advance(lx); lx->token_count++; return make_tok(TOK_RPAREN, ")", line);
        case '{': advance(lx); lx->token_count++; return make_tok(TOK_LBRACE, "{", line);
        case '}': advance(lx); lx->token_count++; return make_tok(TOK_RBRACE, "}", line);
        case ';': advance(lx); lx->token_count++; return make_tok(TOK_SEMI,   ";", line);
        case ',': advance(lx); lx->token_count++; return make_tok(TOK_COMMA,  ",", line);

        case '=':
            advance(lx);
            if (peek(lx) == '=') { advance(lx); lx->token_count++; return make_tok(TOK_EQ, "==", line); }
            lx->token_count++; return make_tok(TOK_ASSIGN, "=", line);
        case '!':
            advance(lx);
            if (peek(lx) == '=') { advance(lx); lx->token_count++; return make_tok(TOK_NEQ, "!=", line); }
            lx->error_count++;
            fprintf(stderr, "[lexer] error line %d: illegal character '!' (did you mean '!='?)\n", line);
            return make_tok(TOK_ERROR, "!", line);
        case '<':
            advance(lx);
            if (peek(lx) == '=') { advance(lx); lx->token_count++; return make_tok(TOK_LE, "<=", line); }
            lx->token_count++; return make_tok(TOK_LT, "<", line);
        case '>':
            advance(lx);
            if (peek(lx) == '=') { advance(lx); lx->token_count++; return make_tok(TOK_GE, ">=", line); }
            lx->token_count++; return make_tok(TOK_GT, ">", line);
        case '&':
            advance(lx);
            if (peek(lx) == '&') { advance(lx); lx->token_count++; return make_tok(TOK_AND, "&&", line); }
            lx->error_count++;
            fprintf(stderr, "[lexer] error line %d: illegal character '&' (did you mean '&&'?)\n", line);
            return make_tok(TOK_ERROR, "&", line);
        case '|':
            advance(lx);
            if (peek(lx) == '|') { advance(lx); lx->token_count++; return make_tok(TOK_OR, "||", line); }
            lx->error_count++;
            fprintf(stderr, "[lexer] error line %d: illegal character '|' (did you mean '||'?)\n", line);
            return make_tok(TOK_ERROR, "|", line);

        default: {
            /* illegal character: report + recover by skipping it */
            char lex[2] = { (char)c, '\0' };
            advance(lx);
            lx->error_count++;
            fprintf(stderr, "[lexer] error line %d: illegal character '%s'\n", line, lex);
            return make_tok(TOK_ERROR, lex, line);
        }
    }
}
