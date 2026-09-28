#include "pipeline.h"
#include "jobs.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>

// Se llama solo dentro de un proceso hijo antes de execvp.
// Se usa _exit y no exit porque exit vaciaria los buffers de stdio heredados del padre y duplicaria texto.
static void redireccion(Redireccion *r) {
    // Si es el archivo de entrada se abre en modo solo lectura
    if (r->archivo_in) {
        int fd = open(r->archivo_in, O_RDONLY);
        if (fd < 0){ 
            perror("open");
            _exit(1);
        }
        dup2(fd, STDIN_FILENO); // fd 0 apunta al archivo
        close(fd);
    }
    // Si es el archivo de salida se abre en modo solo escritura, si no existe se crea
    // y si ya existe se escribe al final o se vacia.
    if (r->archivo_out) {
        int flags = O_WRONLY | O_CREAT | (r->append ? O_APPEND : O_TRUNC);
        int fd = open(r->archivo_out, flags, 0644);
        if (fd < 0) {
            perror("open"); 
            _exit(1);
        }
        dup2(fd, STDOUT_FILENO); // fd 1 ahora apunta al archivo
        close(fd);
    }
}
// Recorre todos los comandos y los une con | para mostrarlo en jobs y pmon
static void construir_cmd(Pipeline *p, char *buf, size_t tam) {
    buf[0] = '\0';
    for (int i = 0; i < p->n; i++) {
        if (i > 0) {
            strncat(buf, " | ", tam - strlen(buf) - 1);
        }
        for (int j = 0; p->cmds[i].argv[j] != NULL; j++) {
            if (j > 0) {
                strncat(buf, " ", tam - strlen(buf) - 1);
            }
            strncat(buf, p->cmds[i].argv[j], tam - strlen(buf) - 1);
        }
    }
}

// Configura las señales del hijo sin usar signal(), tal como exige el enunciado.
// En foreground se restauran las acciones por defecto; en background se ignoran.
static void configurar_senales_hijo(int background) {
    struct sigaction sa;

    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = background ? SIG_IGN : SIG_DFL;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
}

int ejecutar_pipeline(Pipeline *p){
    int n = p->n;
    // n-1 tuberias para n comandos.
    // Se pide al menos un par para que al llamar a malloc(0) no devuelva NULL sin que sea un error
    int (*pipes)[2] = malloc (sizeof(int[2])*(n > 1 ? (n - 1) : 1));
    if (pipes == NULL) {
        perror("malloc");
        return -1;
    }
    for (int i = 0; i < n - 1; i++) {
        if (pipe(pipes[i]) < 0) {
            perror("pipe");
            // Se cierran los pipes ya creados para no filtrar descriptores
            for (int k = 0; k < i; k++) {
                close(pipes[k][0]);
                close(pipes[k][1]);
            }
            free(pipes);
            return -1;
        }
    }

    // Bloquear SIGCHLD antes del primer fork hasta registrar el job.
    // Sin esto, si un hijo termina rapido seria recolectado por el manejador antes de estar en 
    // la tabla de jobs y se registraria un job ya muerto como "Ejecutando" para siempre.
    // La señal queda pendiente y se entrega al desbloquear
    sigset_t mask_chld, mask_old;
    sigemptyset(&mask_chld);
    sigaddset(&mask_chld, SIGCHLD);
    sigprocmask(SIG_BLOCK, &mask_chld, &mask_old);

    pid_t pids[n];
    for (int i = 0; i < n; i++) {
        // Evita que si el fork falla a mitad los demas pids queden con contenido basura
        pids[i] = -1;
    }
    int fallo = 0;

    for(int i = 0; i < n; i++){
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork");
            fallo = 1;
            break;
        }
        if (pids[i] == 0) {
            // La mascara de señales se hereda por fork y sobrevive a execvp.
            // Se restaura primero para que el programa lanzado no arranque con SIGCHLD bloqueada.
            sigprocmask(SIG_SETMASK, &mask_old, NULL);

            // Foreground recibe Ctrl+C/Ctrl+\\; background queda protegido.
            configurar_senales_hijo(p->background);
            // Conecta la entrada al pipe anterior y la salida al siguiente
            if (i > 0) {
                dup2(pipes[i-1][0], STDIN_FILENO);
            }
            if (i < n-1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }
            // Se cierran todos los extremos de pipe, pq si queda uno abierto un lector
            // nunca recibiria EOF y el pipeline falla.
            for(int j = 0; j < n-1; j++){
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            // Una redireccion explicita tiene prioridad sobre el pipe
            redireccion(&p -> cmds[i].redir);
            execvp(p -> cmds[i].argv[0], p -> cmds[i].argv);
            perror("execvp");
            _exit(127);
        }
    }
    // El padre tambien cierra sus copias de los pipes para evitar que los lectores nunca vean EOF
    for (int j = 0; j < n-1; j++) {
        close(pipes[j][0]);
        close(pipes[j][1]);
    }
    free(pipes);

    if (fallo) {
        // Matar y recoger los hijos ya creados para no dejar zombies
        for(int i = 0; i < n; i++) {
            if (pids[i] > 0) {
                kill(pids[i], SIGTERM);
            }
        }
        for(int i = 0; i < n; i++) {
            if (pids[i] > 0) {
                waitpid(pids[i], NULL, 0);
            }
        }
        sigprocmask(SIG_SETMASK, &mask_old, NULL);
        return -1;

    }

    if (!p -> background) {
        // Con SIGCHLD bloqueada no hay conflicto con el manejador para recolectar a los hijos
        for (int i = 0; i < n; i++) {
            waitpid(pids[i], NULL, 0);
        }
    } else {
        // Reconstruir la línea de comando
        char cmd_completo[256] = "";
        construir_cmd(p, cmd_completo, sizeof(cmd_completo));

        // Registrar con el comando completo
        jobs_agregar(pids[n - 1], cmd_completo);
    }
    // Se desbloquea para foreground y background
    sigprocmask(SIG_SETMASK, &mask_old, NULL);
    return 0;
}