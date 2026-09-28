#include "parser.h"
#include "executor.h"
#include "io.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_LINE 256

typedef struct {
    char **items;
    size_t count;
    size_t cap;
} WordList;

static void wordlist_init(WordList *wl) {
    wl->items = NULL;
    wl->count = 0;
    wl->cap = 0;
}

// Agrega una palabra a la lista, reservando espacio de mas para el NULL final
static void wordlist_push(WordList *wl, char *w) {
    if (wl->count + 1 >= wl->cap) {
        size_t nuevo_cap = (wl->cap == 0) ? 4 : wl->cap * 2;
        wl->items = realloc(wl->items, nuevo_cap * sizeof(char *));
        wl->cap = nuevo_cap;
    }
    wl->items[wl->count++] = w;
}

static void wordlist_free_content(WordList *wl) {
    for (size_t i = 0; i < wl->count; i++) {
        free(wl->items[i]);
    }
    free(wl->items);
    wl->items = NULL;
    wl->count = wl->cap = 0;
}

/*
 * token_valor - Convierte un token TOK_WORD "crudo" (tal como lo entrega el
 * lexer, con comillas y escapes todavia presentes) en la palabra final que
 * debe ir en argv: quita comillas simples/dobles y resuelve backslashes.
 */
static char *token_value(const Token *tok) {
    char *buf = malloc(tok->len + 1);
    size_t bi = 0;
    size_t i = 0;

    while (i < tok->len) {
        char c = tok->start[i];

        if (c == '\\') {
            i++;
            if (i < tok->len) buf[bi++] = tok->start[i++];
            continue;
        }

        if (c == '\'') {
            i++;
            while (i < tok->len && tok->start[i] != '\'') {
                buf[bi++] = tok->start[i++];
            }
            if (i < tok->len) i++; // saltar comilla de cierre
            continue;
        }

        if (c == '"') {
            i++;
            while (i < tok->len && tok->start[i] != '"') {
                if (tok->start[i] == '\\' && i + 1 < tok->len) {
                    i++;
                }
                buf[bi++] = tok->start[i++];
            }
            if (i < tok->len) i++; // saltar comilla de cierre
            continue;
        }

        buf[bi++] = c;
        i++;
    }

    buf[bi] = '\0';
    return buf;
}

Parser *parser_new(Lexer *l) {
    Parser *p = NULL;
    p = malloc(sizeof(Parser));
    p->l = l;
    return p;
}

void parser_delete(Parser *p) {
    free(p);
}

void executionunit_free(ExecutionUnit *e) {
    if (e == NULL) return;

    if (e->argv != NULL) {
        for (size_t i = 0; e->argv[i] != NULL; i++) {
            free(e->argv[i]);
        }
        free(e->argv);
        e->argv = NULL;
    }

    free(e->archivo_in);
    free(e->archivo_out);
    e->archivo_in = NULL;
    e->archivo_out = NULL;
}

/*
 * parser_next - Extrae la siguiente "unidad de ejecucion" (un comando con
 * sus argumentos y sus redirecciones) desde el flujo de tokens del lexer.
 *
 * Retorna 1 y llena *e si logro reconocer un comando.
 * Retorna 0 si no hay mas comandos (fin de linea vacio, EOF) o si hubo un
 * error de sintaxis.
 *
 * Un '|' cierra la unidad actual (marcando out = TO_PIPE) para que quien
 * llama arme el pipeline agrupando unidades sucesivas separadas por '|'.
 * Un '&' cierra la unidad actual y marca background = 1.
 * TOK_NEWLINE / TOK_EOF cierran la unidad (y la linea completa).
 */
int parser_next(Parser *p, ExecutionUnit *e) {
    if (p == NULL || p->l == NULL || e == NULL) {
        return 0;
    }

    e->argv = NULL;
    e->in = FROM_STDIN;
    e->out = TO_STDOUT;
    e->archivo_in = NULL;
    e->archivo_out = NULL;
    e->append = 0;
    e->background = 0;

    WordList palabras;
    wordlist_init(&palabras);

    int hay_contenido = 0; // hubo al menos una palabra o redireccion
    Token tok;

    for (;;) {
        lexer_next(p->l, &tok);

        switch (tok.kind) {
            case TOK_WORD:
                wordlist_push(&palabras, token_value(&tok));
                hay_contenido = 1;
                break;

            case TOK_REDIR_IN: {
                Token dest;
                lexer_next(p->l, &dest);
                if (dest.kind != TOK_WORD) {
                    ferr("parser: se esperaba un archivo despues de '<'\n");
                    goto error_sintaxis;
                }
                free(e->archivo_in);
                e->archivo_in = token_value(&dest);
                e->in = FROM_FILE;
                hay_contenido = 1;
                break;
            }

            case TOK_REDIR_OUT:
            case TOK_REDIR_OUT_APPEND: {
                Token dest;
                lexer_next(p->l, &dest);
                if (dest.kind != TOK_WORD) {
                    ferr("parser: se esperaba un archivo despues de '%s'\n",
                            (tok.kind == TOK_REDIR_OUT_APPEND) ? ">>" : ">");
                    goto error_sintaxis;
                }
                free(e->archivo_out);
                e->archivo_out = token_value(&dest);
                e->out = TO_FILE;
                e->append = (tok.kind == TOK_REDIR_OUT_APPEND);
                hay_contenido = 1;
                break;
            }

            case TOK_REDIR_DUP:
                // Duplicacion de descriptores (ej. 2>&1): no soportado aun
                ferr("parser: redireccion de descriptores no soportada\n");
                goto error_sintaxis;

            case TOK_PIPE:
                if (!hay_contenido) {
                    ferr("parser: error de sintaxis cerca de '|'\n");
                    goto error_sintaxis;
                }
                e->out = TO_PIPE;
                goto fin_unidad;

            case TOK_AMP:
                if (!hay_contenido) {
                    ferr("parser: error de sintaxis cerca de '&'\n");
                    goto error_sintaxis;
                }
                e->background = 1;
                goto fin_unidad;

            case TOK_LPAREN:
            case TOK_RPAREN:
                ferr("parser: subshells '()' no soportados aun\n");
                goto error_sintaxis;

            case TOK_NEWLINE:
            case TOK_EOF:
                goto fin_unidad;

            default:
                ferr("parser: token no reconocido (%s)\n", token_kind_name(tok.kind));
                goto error_sintaxis;
        }
    }

    fin_unidad:
    if (!hay_contenido) {
        // Linea vacia: no hay comando que devolver
        wordlist_free_content(&palabras);
        return 0;
    }

    wordlist_push(&palabras, NULL); // argv debe terminar en NULL
    e->argv = palabras.items;
    return 1;

    error_sintaxis:
    wordlist_free_content(&palabras);
    free(e->archivo_in);
    free(e->archivo_out);
    e->archivo_in = NULL;
    e->archivo_out = NULL;
    e->argv = NULL;
    return 0;
}

size_t parse_cmdline(Pipeline *pipeline, size_t n) {
    (void)n;

    char buf[MAX_LINE] = { 0 };
    ssize_t rd = read(STDIN_FILENO, buf, sizeof(buf) - 1);
    if (rd == 0) {
        return PARSE_EOF;
    }
    if (rd < 0) {
        return 0;
    }
    buf[rd] = '\0';
    if (buf[0] == '\n') {
        return 0;
    }

    // prepararse para lexear la linea
    Lexer *l = lexer_new(buf);
    Parser *p = parser_new(l);
    ExecutionUnit ex;
    size_t total = 0;

    Comando *comandos = NULL;
    size_t cantidad = 0;

    while (parser_next(p, &ex)) {
        // Cada unidad terminada en '|' se conserva hasta completar el pipeline.
        Comando *nuevo = realloc(comandos, (cantidad + 1) * sizeof(*comandos));
        if (nuevo == NULL) {
            ferr("realloc");
            executionunit_free(&ex);
            break;
        }
        comandos = nuevo;
        comandos[cantidad].argv = ex.argv;
        comandos[cantidad].redir.archivo_in = ex.archivo_in;
        comandos[cantidad].redir.archivo_out = ex.archivo_out;
        comandos[cantidad].redir.append = ex.append;
        ex.argv = NULL;
        ex.archivo_in = NULL;
        ex.archivo_out = NULL;
        cantidad++;

        if (ex.out == TO_PIPE) {
            continue;
        }

        // Sin |, la unidad actual cierra el pipeline y se puede despachar.
        Pipeline actual = { comandos, (int)cantidad, ex.background };
        if (pipeline != NULL) {
            *pipeline = actual;
        }
        ejecutar_ejecutor(&actual);

        // El ejecutor ya terminó de usar los argumentos; liberar la unidad completa.
        for (size_t i = 0; i < cantidad; i++) {
            for (size_t j = 0; comandos[i].argv[j] != NULL; j++) {
                free(comandos[i].argv[j]);
            }
            free(comandos[i].argv);
            free(comandos[i].redir.archivo_in);
            free(comandos[i].redir.archivo_out);
        }
        free(comandos);
        comandos = NULL;
        cantidad = 0;
        total++;
    }

    for (size_t i = 0; i < cantidad; i++) {
        for (size_t j = 0; comandos[i].argv[j] != NULL; j++) {
            free(comandos[i].argv[j]);
        }
        free(comandos[i].argv);
        free(comandos[i].redir.archivo_in);
        free(comandos[i].redir.archivo_out);
    }
    free(comandos);

    parser_delete(p);
    lexer_delete(l);
    return total;
}
