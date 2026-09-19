#include "executor.h"
#include "pipeline.h"

#include <stdio.h>
#include <string.h>

static int es_builtin(const Comando *cmd) {
    if (cmd == NULL || cmd->argv == NULL || cmd->argv[0] == NULL) {
        return 0;
    }

    const char *nombre = cmd->argv[0];
    return strcmp(nombre, "cd") == 0 ||
           strcmp(nombre, "pwd") == 0 ||
           strcmp(nombre, "echo") == 0 ||
           strcmp(nombre, "exit") == 0 ||
           strcmp(nombre, "export") == 0 ||
           strcmp(nombre, "unset") == 0;
}

int ejecutar_ejecutor(Pipeline *p) {
    if (p == NULL || p->cmds == NULL || p->n <= 0) {
        fprintf(stderr, "ejecutor: pipeline invalido\n");
        return -1;
    }

    for (int i = 0; i < p->n; i++) {
        if (p->cmds[i].argv == NULL || p->cmds[i].argv[0] == NULL) {
            fprintf(stderr, "ejecutor: comando %d sin ejecutable\n", i);
            return -1;
        }
    }

    if (p->n == 1 && es_builtin(&p->cmds[0])) {
        fprintf(stderr, "ejecutor: builtin no implementado aun por este modulo\n");
        return 0;
    }

    return ejecutar_pipeline(p);
}
