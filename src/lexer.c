#include "lexer.h"

#include <stdlib.h>

const char *token_kind_name(TokenKind kind) {
    switch (kind) {
        case TOK_WORD:             return "TOK_WORD";
        case TOK_PIPE:             return "TOK_PIPE";
        case TOK_AMP:              return "TOK_AMP";
        case TOK_LPAREN:           return "TOK_LPAREN";
        case TOK_RPAREN:           return "TOK_RPAREN";
        case TOK_NEWLINE:          return "TOK_NEWLINE";
        case TOK_REDIR_IN:         return "TOK_REDIR_IN";
        case TOK_REDIR_OUT:        return "TOK_REDIR_OUT";
        case TOK_REDIR_OUT_APPEND: return "TOK_REDIR_OUT_APPEND";
        case TOK_REDIR_DUP:        return "TOK_REDIR_DUP";
        case TOK_EOF:              return "TOK_EOF";
        default:                   return "TOK_ERROR";
    }
}


// Determina si el char c es un blanco
static int is_blank(char c) {
    return c == ' ' || c == '\t';
}

// Determina si el char c indica el inicio de un operador
static int is_operator_start(char c) {
    switch(c) {
        case '|':
        case '&':
        case ';':
        case '<':
        case '>':
        case '(':
        case ')':
            return 1;
        default:
            return 0;
    }
}

static int lex_word(Lexer *lx, Token *tok)
{
    const char *s = lx->src;
    size_t start = lx->pos;

    while(s[lx->pos] != '\0') {
        char c = s[lx->pos];

        // char "normal"
        if(c != ' ' && c != '\t' && c != '\n' &&
            !is_operator_start(c)) {
            // Secuencia de escape
            if(c == '\\') {
                lx->pos++;

                if(s[lx->pos] != '\0') lx->pos++;

                continue;
            }

            // Comilla sola
            if(c == '\'') {
                lx->pos++;

                while(s[lx->pos] != '\0' && s[lx->pos] != '\'') lx->pos++;
                if(s[lx->pos] == '\'') lx->pos++;

                continue;
            }

            // Doble comilla
            if(c == '"') {
                lx->pos++;

                while(s[lx->pos] != '\0') {
                    c = s[lx->pos];

                    if(c == '\\') {
                        lx->pos++;

                        if(s[lx->pos] != '\0') lx->pos++;

                        continue;
                    }

                    if(c == '"') {
                        lx->pos++;
                        break;
                    }

                    lx->pos++;
                }

                continue;
            }

            lx->pos++;
            continue;
            }

            break;
    }

    tok->kind = TOK_WORD;
    tok->start = s + start;
    tok->len = lx->pos - start;

    return 1;
}

Lexer *lexer_new(const char *str) {
    Lexer *lx = malloc(sizeof(Lexer));
    lx->src = str;
    lx->pos = 0;

    return lx;
}

int lexer_next(Lexer *lx, Token *tok) {
    const char *s = lx->src;

    // Saltar el whitespace
    while(is_blank(s[lx->pos])) lx->pos++;

    // EOF
    if(s[lx->pos] == '\0') {
        tok->kind = TOK_EOF;
        tok->start = s + lx->pos;
        tok->len = 0;
        return 0;
    }

    size_t start = lx->pos;

    switch (s[lx->pos]) {
        case '|':
            lx->pos++;
            tok->kind = TOK_PIPE;
            break;

        case '&':
            lx->pos++;
            tok->kind = TOK_AMP;
            break;

        case '(':
            lx->pos++;
            tok->kind = TOK_LPAREN;
            break;

        case ')':
            lx->pos++;
            tok->kind = TOK_RPAREN;
            break;

        case '\n':
            lx->pos++;
            tok->kind = TOK_NEWLINE;
            break;

        case '<':
            lx->pos++;

            if(s[lx->pos] == '&') {
                lx->pos++;
                tok->kind = TOK_REDIR_DUP;
            } else {
                tok->kind = TOK_REDIR_IN;
            }
            break;

        case '>':
            lx->pos++;

            if(s[lx->pos] == '>') {
                lx->pos++;
                tok->kind = TOK_REDIR_OUT_APPEND;
            } else if(s[lx->pos] == '&') {
                lx->pos++;
                tok->kind = TOK_REDIR_DUP;
            } else {
                tok->kind = TOK_REDIR_OUT;
            }
            break;

        default:
            return lex_word(lx, tok);
    }

    tok->start = s + start;
    tok->len = lx->pos - start;

    return 1;
}

void lexer_delete(Lexer *lx) {
    free(lx);
}

