/* ============================================================
   main.c  --  Verity Compiler front-end driver
   BCSE307P Compiler Design Lab

   Usage: verity <file.vr> [--tokens-only] [--ast-only]

   Default: runs the lexer over the whole file and prints the
   token table (LINE, TYPE, LEXEME) + counts, exactly as demoed
   in Review 1, THEN re-lexes + parses the same file and prints
   the resulting AST (Phase 2 deliverable added today).
   ============================================================ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lexer.h"
#include "parser.h"
#include "ast.h"

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "verity: cannot open '%s'\n", path); return NULL; }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc(size + 1);
    size_t n = fread(buf, 1, size, f);
    buf[n] = '\0';
    fclose(f);
    return buf;
}

static void run_lexer_demo(const char *src) {
    Lexer lx;
    lexer_init(&lx, src);

    printf("=========================================================\n");
    printf(" VERITY -- Lexical Analyzer   |  Token Table\n");
    printf("=========================================================\n");
    printf("%-6s %-14s %s\n", "LINE", "TYPE", "LEXEME");
    printf("---------------------------------------------------------\n");

    for (;;) {
        Token t = lexer_next(&lx);
        if (t.type == TOK_EOF) break;
        printf("%-6d %-14s %s\n", t.line, token_type_name(t.type), t.lexeme);
    }

    printf("---------------------------------------------------------\n");
    printf("Total tokens : %d\n", lx.token_count);
    printf("Lex errors   : %d\n", lx.error_count);
    printf("=========================================================\n\n");
}

static void run_parser_demo(const char *src) {
    Lexer lx;
    lexer_init(&lx, src);
    Parser p;
    parser_init(&p, &lx);

    printf("=========================================================\n");
    printf(" VERITY -- Recursive-Descent Parser  |  Abstract Syntax Tree\n");
    printf("=========================================================\n");

    Node *root = parser_parse_program(&p);
    if (root) ast_print(root);

    printf("---------------------------------------------------------\n");
    printf("Syntax errors : %d\n", p.error_count);
    printf("Lex errors    : %d\n", lx.error_count);
    printf("=========================================================\n");

    if (root) ast_free(root);
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: %s <file.vr> [--tokens-only|--ast-only]\n", argv[0]);
        return 1;
    }
    char *src = read_file(argv[1]);
    if (!src) return 1;

    int tokens_only = (argc > 2 && (!strcmp(argv[2], "--tokens-only")));
    int ast_only    = (argc > 2 && (!strcmp(argv[2], "--ast-only")));

    if (!ast_only)    run_lexer_demo(src);
    if (!tokens_only) run_parser_demo(src);

    free(src);
    return 0;
}
