/* ============================================================
   parser.c  --  Verity Compiler : Recursive-Descent LL(1) Parser
   BCSE307P Compiler Design Lab | Phase 2

   Grammar (EBNF) implemented below:

     program     -> decl* EOF
     decl        -> func_decl | var_decl
     type        -> 'int' | 'float' | 'char' | 'bool' | 'void'
     func_decl   -> type ID '(' params? ')' block
     params      -> param (',' param)*
     param       -> type ID
     var_decl    -> type declarator (',' declarator)* ';'
     declarator  -> ID ('=' expr)?
     block       -> '{' stmt* '}'
     stmt        -> var_decl | if_stmt | while_stmt | return_stmt
                  | read_stmt | write_stmt | block | expr_stmt
     if_stmt     -> 'if' '(' expr ')' stmt ('else' stmt)?
     while_stmt  -> 'while' '(' expr ')' stmt
     return_stmt -> 'return' expr? ';'
     read_stmt   -> 'read' '(' ID ')' ';'
     write_stmt  -> 'write' '(' expr ')' ';'
     expr_stmt   -> expr ';'

     expr        -> assignment
     assignment  -> logic_or ('=' assignment)?      -- lhs must be an lvalue
     logic_or    -> logic_and ('||' logic_and)*
     logic_and   -> equality ('&&' equality)*
     equality    -> relational (('=='|'!=') relational)*
     relational  -> additive (('<'|'<='|'>'|'>=') additive)*
     additive    -> term (('+'|'-') term)*
     term        -> unary (('*'|'/'|'%') unary)*
     unary       -> '-' unary | primary
     primary     -> INT_CONST | FLOAT_CONST | 'true' | 'false'
                  | ID | ID '(' args? ')' | '(' expr ')'
     args        -> expr (',' expr)*

   1-token lookahead throughout => LL(1). Each nonterminal below
   is one function; the token in p->cur is always the lookahead.
   ============================================================ */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser.h"

/* ---------- token stream plumbing ---------- */
static void advance_tok(Parser *p) {
    Token t = lexer_next(p->lx);
    while (t.type == TOK_ERROR) {
        /* lexer already reported it; skip and keep parsing */
        t = lexer_next(p->lx);
    }
    p->cur = t;
}

void parser_init(Parser *p, Lexer *lx) {
    p->lx = lx;
    p->error_count = 0;
    advance_tok(p); /* prime the first lookahead token */
}

static void syntax_error(Parser *p, const char *msg) {
    fprintf(stderr, "[parser] error line %d: %s (got '%s')\n",
            p->cur.line, msg, p->cur.lexeme);
    p->error_count++;
}

/* consume current token if it matches `type`, else report and bail out
   of the whole parse (simple, non-recovering parser: correct programs
   parse cleanly; malformed ones stop with a clear diagnostic). */
static Token expect(Parser *p, TokenType type, const char *what) {
    Token t = p->cur;
    if (p->cur.type != type) {
        char msg[160];
        snprintf(msg, sizeof(msg), "expected %s", what);
        syntax_error(p, msg);
        return t; /* don't consume the unexpected token; let caller's
                     structure (e.g. missing '}') surface at EOF instead
                     of desyncing the stream further */
    }
    advance_tok(p);
    return t;
}

static int at(Parser *p, TokenType type) { return p->cur.type == type; }

static int is_type_tok(Parser *p) {
    switch (p->cur.type) {
        case TOK_INT: case TOK_FLOAT: case TOK_CHAR:
        case TOK_BOOL: case TOK_VOID:
            return 1;
        default: return 0;
    }
}

/* ---------- forward declarations ---------- */
static Node *parse_decl(Parser *p);
static Node *parse_type_decl(Parser *p);            /* dispatches func vs var */
static Node *parse_block(Parser *p);
static Node *parse_stmt(Parser *p);
static Node *finish_var_decl(Parser *p, const char *tname, int line, Token first_name);
static Node *parse_expr(Parser *p);
static Node *parse_assignment(Parser *p);
static Node *parse_logic_or(Parser *p);
static Node *parse_logic_and(Parser *p);
static Node *parse_equality(Parser *p);
static Node *parse_relational(Parser *p);
static Node *parse_additive(Parser *p);
static Node *parse_term(Parser *p);
static Node *parse_unary(Parser *p);
static Node *parse_primary(Parser *p);

/* ---------- helper: binary-op chain builder ----------
   Folds a left-associative chain  next (OP next)*  into a
   left-leaning BinOp tree, used by 5 of the precedence levels. */
typedef Node *(*SubParser)(Parser *);

static Node *binop_chain(Parser *p, SubParser next, const TokenType *ops,
                          const char **op_text, int nops) {
    Node *left = next(p);
    for (;;) {
        int matched = -1;
        for (int i = 0; i < nops; i++) {
            if (p->cur.type == ops[i]) { matched = i; break; }
        }
        if (matched < 0) break;
        int line = p->cur.line;
        advance_tok(p);
        Node *right = next(p);
        Node *bin = node_new(NODE_BINOP, line);
        node_set_str(bin, op_text[matched]);
        node_add_child(bin, left);
        node_add_child(bin, right);
        left = bin;
    }
    return left;
}

/* ================= program / declarations ================= */

Node *parser_parse_program(Parser *p) {
    Node *root = node_new(NODE_PROGRAM, 1);
    while (!at(p, TOK_EOF)) {
        Node *d = parse_decl(p);
        if (!d) break;
        node_add_child(root, d);
    }
    return root;
}

static const char *type_text(TokenType t) {
    switch (t) {
        case TOK_INT: return "int";
        case TOK_FLOAT: return "float";
        case TOK_CHAR: return "char";
        case TOK_BOOL: return "bool";
        case TOK_VOID: return "void";
        default: return "?";
    }
}

static Node *parse_decl(Parser *p) {
    if (!is_type_tok(p)) {
        syntax_error(p, "expected a type ('int'/'float'/'char'/'bool'/'void') to start a declaration");
        return NULL;
    }
    return parse_type_decl(p);
}

/* type ID  -->  either '(' params ')' block   (function)
                 or     (',' declarator)* ';'  (variable(s)) */
static Node *parse_type_decl(Parser *p) {
    int line = p->cur.line;
    const char *tname = type_text(p->cur.type);
    advance_tok(p); /* consume type */

    Token name = expect(p, TOK_IDENT, "an identifier");

    if (at(p, TOK_LPAREN)) {
        /* ---- function declaration ---- */
        Node *fn = node_new(NODE_FUNC_DECL, line);
        char label[160];
        snprintf(label, sizeof(label), "%s %s", tname, name.lexeme);
        node_set_str(fn, label);

        advance_tok(p); /* '(' */
        if (!at(p, TOK_RPAREN)) {
            for (;;) {
                if (!is_type_tok(p)) { syntax_error(p, "expected a parameter type"); break; }
                int pline = p->cur.line;
                const char *ptype = type_text(p->cur.type);
                advance_tok(p);
                Token pname = expect(p, TOK_IDENT, "a parameter name");
                Node *param = node_new(NODE_PARAM, pline);
                char plabel[160];
                snprintf(plabel, sizeof(plabel), "%s %s", ptype, pname.lexeme);
                node_set_str(param, plabel);
                node_add_child(fn, param);
                if (at(p, TOK_COMMA)) { advance_tok(p); continue; }
                break;
            }
        }
        expect(p, TOK_RPAREN, "')'");
        Node *body = parse_block(p);
        node_add_child(fn, body);
        return fn;
    }

    /* ---- variable declaration: type + first name already consumed ---- */
    return finish_var_decl(p, tname, line, name);
}

/* Builds a VarDecl node given that `type` and the FIRST declarator's
   identifier have already been consumed by the caller (parse_type_decl
   for top-level decls, parse_local_var_decl for statements). Handles
   the remaining ('=' expr)? and (',' declarator)* ';' tail. */
static Node *finish_var_decl(Parser *p, const char *tname, int line, Token first_name) {
    Node *vd = node_new(NODE_VAR_DECL, line);
    node_set_str(vd, tname);

    /* first declarator (name already consumed by caller) */
    Node *decl0 = node_new(NODE_DECLARATOR, first_name.line);
    node_set_str(decl0, first_name.lexeme);
    if (at(p, TOK_ASSIGN)) {
        advance_tok(p);
        node_add_child(decl0, parse_expr(p));
    }
    node_add_child(vd, decl0);

    while (at(p, TOK_COMMA)) {
        advance_tok(p);
        Token nm = expect(p, TOK_IDENT, "an identifier");
        Node *d = node_new(NODE_DECLARATOR, nm.line);
        node_set_str(d, nm.lexeme);
        if (at(p, TOK_ASSIGN)) {
            advance_tok(p);
            node_add_child(d, parse_expr(p));
        }
        node_add_child(vd, d);
    }
    expect(p, TOK_SEMI, "';'");
    return vd;
}

/* ================= statements ================= */

static Node *parse_block(Parser *p) {
    int line = p->cur.line;
    expect(p, TOK_LBRACE, "'{'");
    Node *blk = node_new(NODE_BLOCK, line);
    while (!at(p, TOK_RBRACE) && !at(p, TOK_EOF)) {
        Node *s = parse_stmt(p);
        if (!s) break;
        node_add_child(blk, s);
    }
    expect(p, TOK_RBRACE, "'}'");
    return blk;
}

static Node *parse_if(Parser *p) {
    int line = p->cur.line;
    advance_tok(p); /* 'if' */
    expect(p, TOK_LPAREN, "'('");
    Node *cond = parse_expr(p);
    expect(p, TOK_RPAREN, "')'");
    Node *then_s = parse_stmt(p);
    Node *n = node_new(NODE_IF, line);
    node_add_child(n, cond);
    node_add_child(n, then_s);
    if (at(p, TOK_ELSE)) {
        advance_tok(p);
        node_add_child(n, parse_stmt(p));
    }
    return n;
}

static Node *parse_while(Parser *p) {
    int line = p->cur.line;
    advance_tok(p); /* 'while' */
    expect(p, TOK_LPAREN, "'('");
    Node *cond = parse_expr(p);
    expect(p, TOK_RPAREN, "')'");
    Node *body = parse_stmt(p);
    Node *n = node_new(NODE_WHILE, line);
    node_add_child(n, cond);
    node_add_child(n, body);
    return n;
}

static Node *parse_return(Parser *p) {
    int line = p->cur.line;
    advance_tok(p); /* 'return' */
    Node *n = node_new(NODE_RETURN, line);
    if (!at(p, TOK_SEMI)) node_add_child(n, parse_expr(p));
    expect(p, TOK_SEMI, "';'");
    return n;
}

static Node *parse_read(Parser *p) {
    int line = p->cur.line;
    advance_tok(p); /* 'read' */
    expect(p, TOK_LPAREN, "'('");
    Token id = expect(p, TOK_IDENT, "an identifier");
    expect(p, TOK_RPAREN, "')'");
    expect(p, TOK_SEMI, "';'");
    Node *n = node_new(NODE_READ, line);
    Node *target = node_new(NODE_IDENT, id.line);
    node_set_str(target, id.lexeme);
    node_add_child(n, target);
    return n;
}

static Node *parse_write(Parser *p) {
    int line = p->cur.line;
    advance_tok(p); /* 'write' */
    expect(p, TOK_LPAREN, "'('");
    Node *e = parse_expr(p);
    expect(p, TOK_RPAREN, "')'");
    expect(p, TOK_SEMI, "';'");
    Node *n = node_new(NODE_WRITE, line);
    node_add_child(n, e);
    return n;
}

static Node *parse_local_var_decl(Parser *p) {
    int line = p->cur.line;
    const char *tname = type_text(p->cur.type);
    advance_tok(p); /* type */
    Token name = expect(p, TOK_IDENT, "an identifier");
    return finish_var_decl(p, tname, line, name);
}

static Node *parse_stmt(Parser *p) {
    if (is_type_tok(p))        return parse_local_var_decl(p);
    if (at(p, TOK_IF))         return parse_if(p);
    if (at(p, TOK_WHILE))      return parse_while(p);
    if (at(p, TOK_RETURN))     return parse_return(p);
    if (at(p, TOK_READ))       return parse_read(p);
    if (at(p, TOK_WRITE))      return parse_write(p);
    if (at(p, TOK_LBRACE))     return parse_block(p);

    /* expression statement */
    int line = p->cur.line;
    Node *e = parse_expr(p);
    expect(p, TOK_SEMI, "';'");
    Node *n = node_new(NODE_EXPR_STMT, line);
    node_add_child(n, e);
    return n;
}

/* ================= expressions ================= */

static Node *parse_expr(Parser *p) { return parse_assignment(p); }

static Node *parse_assignment(Parser *p) {
    int line = p->cur.line;
    Node *lhs = parse_logic_or(p);
    if (at(p, TOK_ASSIGN)) {
        advance_tok(p);
        Node *rhs = parse_assignment(p);
        if (lhs->kind != NODE_IDENT) {
            syntax_error(p, "left-hand side of '=' is not assignable");
        }
        Node *asn = node_new(NODE_ASSIGN, line);
        node_add_child(asn, lhs);
        node_add_child(asn, rhs);
        return asn;
    }
    return lhs;
}

static Node *parse_logic_or(Parser *p) {
    static const TokenType ops[] = { TOK_OR };
    static const char *txt[] = { "||" };
    return binop_chain(p, parse_logic_and, ops, txt, 1);
}
static Node *parse_logic_and(Parser *p) {
    static const TokenType ops[] = { TOK_AND };
    static const char *txt[] = { "&&" };
    return binop_chain(p, parse_equality, ops, txt, 1);
}
static Node *parse_equality(Parser *p) {
    static const TokenType ops[] = { TOK_EQ, TOK_NEQ };
    static const char *txt[] = { "==", "!=" };
    return binop_chain(p, parse_relational, ops, txt, 2);
}
static Node *parse_relational(Parser *p) {
    static const TokenType ops[] = { TOK_LT, TOK_LE, TOK_GT, TOK_GE };
    static const char *txt[] = { "<", "<=", ">", ">=" };
    return binop_chain(p, parse_additive, ops, txt, 4);
}
static Node *parse_additive(Parser *p) {
    static const TokenType ops[] = { TOK_PLUS, TOK_MINUS };
    static const char *txt[] = { "+", "-" };
    return binop_chain(p, parse_term, ops, txt, 2);
}
static Node *parse_term(Parser *p) {
    static const TokenType ops[] = { TOK_STAR, TOK_SLASH, TOK_PERCENT };
    static const char *txt[] = { "*", "/", "%" };
    return binop_chain(p, parse_unary, ops, txt, 3);
}

static Node *parse_unary(Parser *p) {
    if (at(p, TOK_MINUS)) {
        int line = p->cur.line;
        advance_tok(p);
        Node *operand = parse_unary(p);
        Node *n = node_new(NODE_UNOP, line);
        node_set_str(n, "-");
        node_add_child(n, operand);
        return n;
    }
    return parse_primary(p);
}

static Node *parse_primary(Parser *p) {
    int line = p->cur.line;

    if (at(p, TOK_INT_CONST)) {
        Node *n = node_new(NODE_INT_LIT, line);
        n->ival = strtol(p->cur.lexeme, NULL, 10);
        advance_tok(p);
        return n;
    }
    if (at(p, TOK_FLOAT_CONST)) {
        Node *n = node_new(NODE_FLOAT_LIT, line);
        n->fval = strtod(p->cur.lexeme, NULL);
        advance_tok(p);
        return n;
    }
    if (at(p, TOK_TRUE) || at(p, TOK_FALSE)) {
        Node *n = node_new(NODE_BOOL_LIT, line);
        n->ival = at(p, TOK_TRUE) ? 1 : 0;
        advance_tok(p);
        return n;
    }
    if (at(p, TOK_IDENT)) {
        char name[MAX_LEXEME];
        strncpy(name, p->cur.lexeme, MAX_LEXEME - 1);
        name[MAX_LEXEME - 1] = '\0';
        advance_tok(p);
        if (at(p, TOK_LPAREN)) {
            advance_tok(p);
            Node *call = node_new(NODE_CALL, line);
            node_set_str(call, name);
            if (!at(p, TOK_RPAREN)) {
                for (;;) {
                    node_add_child(call, parse_expr(p));
                    if (at(p, TOK_COMMA)) { advance_tok(p); continue; }
                    break;
                }
            }
            expect(p, TOK_RPAREN, "')'");
            return call;
        }
        Node *id = node_new(NODE_IDENT, line);
        node_set_str(id, name);
        return id;
    }
    if (at(p, TOK_LPAREN)) {
        advance_tok(p);
        Node *e = parse_expr(p);
        expect(p, TOK_RPAREN, "')'");
        return e;
    }

    syntax_error(p, "expected an expression");
    /* recover with a placeholder so the caller's tree stays well-formed */
    advance_tok(p);
    Node *err = node_new(NODE_IDENT, line);
    node_set_str(err, "<error>");
    return err;
}
