#include "executor.h"
#include "pipeline.h"
#include "jobs.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>

static void manejar_senal_shell(int sig) {
    // Ctrl+C/Ctrl+\ no deben terminar la shell mientras espera entrada.
    (void)sig;
}

void instalar_senales_shell(void) {
    struct sigaction sa;

    // SA_RESTART permite reanudar lecturas interrumpidas por señales.
    sa.sa_handler = manejar_senal_shell;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
}

static int es_builtin(const Comando *cmd) {
    if (cmd == NULL || cmd->argv == NULL || cmd->argv[0] == NULL) {
        return 0;
    }

    const char *nombre = cmd->argv[0];
    return strcmp(nombre, "cd") == 0 ||
           strcmp(nombre, "exit") == 0 ||
           strcmp(nombre, "jobs") == 0 ||
           strcmp(nombre, "pmon") == 0;
}

static int ejecutar_builtin(const Comando *cmd) {
    const char *nombre = cmd->argv[0];

    if (strcmp(nombre, "cd") == 0) {
        return builtin_cd(cmd->argv);
    }
    if (strcmp(nombre, "exit") == 0) {
        return builtin_exit(cmd->argv);
    }
    if (strcmp(nombre, "jobs") == 0) {
        return builtin_jobs(cmd->argv);
    }
    // La única opción restante reconocida por es_builtin es pmon.
    return builtin_pmon(cmd->argv);
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

    // Estos comandos deben modificar o consultar el estado de la shell padre.
    if (p->n == 1 && !p->background && es_builtin(&p->cmds[0])) {
        return ejecutar_builtin(&p->cmds[0]);
    }

    return ejecutar_pipeline(p);
}
