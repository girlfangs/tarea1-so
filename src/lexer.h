#pragma once

#include <stddef.h>

typedef enum {
    TOK_WORD,

    TOK_PIPE,
    TOK_AMP,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_NEWLINE,

    TOK_REDIR_IN,
    TOK_REDIR_OUT,
    TOK_REDIR_OUT_APPEND,
    TOK_REDIR_DUP,

    TOK_EOF
} TokenKind;

typedef struct {
    TokenKind kind;
    const char *start;
    size_t len;
} Token;


typedef struct {
    const char *src;
    size_t pos;
} Lexer;

Lexer *lexer_new(const char *str);
int lexer_next(Lexer *lx, Token *tok);
void lexer_delete(Lexer *lx);

const char *token_kind_name(TokenKind kind);
