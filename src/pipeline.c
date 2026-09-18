#include "pipeline.h"
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <signal.h>

static void redireccion(Redireccion *r) {
    if (r->archivo_in) {
        int fd = open(r->archivo_in, O_RDONLY);
        if (fd < 0){ 
            perror("open");
            _exit(1);
        }
        dup2(fd, STDIN_FILENO);
        close(fd);
    }
    if (r->archivo_out) {
        int flags = O_WRONLY | O_CREAT | (r->append ? O_APPEND : O_TRUNC);
        int fd = open(r->archivo_out, flags, 0644);
        if (fd < 0) {
            perror("open"); 
            _exit(1);
        }
        dup2(fd, STDOUT_FILENO);
        close(fd);
    }
}
int ejecutar_pipeline(Pipeline *p){
    int n = p->n;
    //n-1 tuberias para n comandos
    int (*pipes)[2] = malloc (sizeof(int[2])*(n > 1 ? (n - 1) : 1));
    if (n > 1 && pipes == NULL) {
        perror("malloc");
        return -1;
    }
    for(int i = 0; i < n - 1; i++){
        pipe(pipes[i]);
    }
    pid_t pids[n];
    int fallo = 0;

    for(int i = 0; i < n; i++){
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork");
            fallo = 1;
            pids[i] = -1;
            break;
        }
        if (pids[i] == 0) {
            signal(SIGINT, SIG_DFL);
            if (i > 0) {
                dup2(pipes[i-1][0], STDIN_FILENO);
            }
            if (i < n-1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }
            for(int j = 0; j < n-1; j++){
                close(pipes[j][0]);
                close(pipes[j][1]);
            }
            redireccion(&p -> cmds[i].redir);
            execvp(p -> cmds[i].argv[0], p -> cmds[i].argv);
            perror("execvp");
            _exit(127);
        }
    }
    for (int j = 0; j < n-1; j++) {
        close(pipes[j][0]);
        close(pipes[j][1]);
    }
    free(pipes);

    if (fallo) {
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
        return -1;
    }

    if (!p -> background) {
        for (int i = 0; i < n; i++) {
            waitpid(pids[i], NULL, 0);
        }
        //esto lo completa quien hace SIGCHLD/jobs.
    }
    return 0;
}