#ifndef TIPOS_H
#define TIPOS_H

typedef struct {
    char *archivo_in;
    char *archivo_out;
    int   append;
} Redireccion;

typedef struct {
    char **argv;
    Redireccion redir;
} Comando;

typedef struct {
    Comando *cmds;
    int      n;
    int      background;
} Pipeline;

#endif