#pragma once

#include "lexer.h"
#include "estructura.h"

typedef enum {
    TO_STDOUT,
    TO_FILE,
    TO_PIPE,
} StdoutRedir;

typedef enum {
    FROM_STDIN,
    FROM_FILE,
    FROM_PIPE,
} StdinRedir;

typedef struct {
    Lexer *l;
} Parser;

typedef struct {
    char **argv;
    StdinRedir in;
    StdoutRedir out;
    char *archivo_in;   // nombre de archivo si in  == FROM_FILE
    char *archivo_out;  // nombre de archivo si out == TO_FILE
    int   append;       // 1 si la redireccion de salida es con >>
    int   background;   // 1 si la unidad termino en '&'
} ExecutionUnit;

void executionunit_free(ExecutionUnit *e);

Parser *parser_new(Lexer *l);
int parser_next(Parser *p, ExecutionUnit *e);
void parser_delete(Parser *p);

Comando executionunit_to_executor(const ExecutionUnit *ex);
size_t parse_cmdline(Pipeline *pipeline, size_t n);
