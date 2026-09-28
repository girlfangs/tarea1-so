#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>
#include "pipeline.h"
#include "jobs.h"

// Espera sin usar sleep.
// Cada SIGCHLD interrumpe sleep antes de tiempo
static void esperar(int segundos) {
    time_t fin = time(NULL) + segundos;
    while (time(NULL) < fin) {
        usleep(100000);
    }
}

// Si un ejecutar_pipeline deja SIGCHLD bloqueada, la shell dejaria de
// recolectar procesos en background.
// Se consulta la mascara actual.
static void verificar_sigchld(const char *momento) {
    sigset_t actual;
    sigprocmask(SIG_BLOCK, NULL, &actual);
    if (sigismember(&actual, SIGCHLD)) {
        printf("[FALLO] SIGCHLD quedo bloqueada %s\n", momento);
    } else {
        printf("[OK] SIGCHLD desbloqueada %s\n", momento);
    }
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    jobs_init();
    instalar_sigchld();

    printf("===== PARTE 1: FOREGROUND =====\n");

    printf("\nCaso sin pipe, solo redireccion (sort < in.txt > out1.txt)\n");
    {
        char *argv1[] = {"sort", NULL};
        Comando cmds[1] = {
            { argv1, { "in.txt", "out1.txt", 0 } }
        };
        Pipeline p = { cmds, 1, 0 };
        ejecutar_pipeline(&p);
    }

    printf("\nCaso con pipe simple (ls -l | wc -l)\n");
    {
        char *argv1[] = {"ls", "-l", NULL};
        char *argv2[] = {"wc", "-l", NULL};
        Comando cmds[2] = {
            { argv1, { NULL, NULL, 0 } },
            { argv2, { NULL, NULL, 0 } },
        };
        Pipeline p = { cmds, 2, 0 };
        ejecutar_pipeline(&p);
    }

    printf("\nCaso con pipe de 3 + redireccion final (sort < in.txt | uniq | wc -l > out3.txt)\n");
    {
        char *argv1[] = {"sort", NULL};
        char *argv2[] = {"uniq", NULL};
        char *argv3[] = {"wc", "-l", NULL};
        Comando cmds[3] = {
            { argv1, { "in.txt", NULL, 0 } },
            { argv2, { NULL, NULL, 0 } },
            { argv3, { NULL, "out3.txt", 0 } },
        };
        Pipeline p = { cmds, 3, 0 };
        ejecutar_pipeline(&p);
    }

    printf("\nCaso con comando que no existe\n");
    {
        char *argv1[] = {"comandoquenoexiste", NULL};
        Comando cmds[1] = {
            { argv1, { NULL, NULL, 0 } }
        };
        Pipeline p = { cmds, 1, 0 };
        ejecutar_pipeline(&p);
        printf("el proceso padre sigue funcionando despues del error\n");
    }

    printf("\nCaso de archivo de entrada que no existe (< noexiste.txt)\n");
    {
        char *argv1[] = {"cat", NULL};
        Comando cmds[1] = {
            { argv1, { "noexiste.txt", NULL, 0 } }
        };
        Pipeline p = { cmds, 1, 0 };
        ejecutar_pipeline(&p);
        printf("el proceso padre sigue funcionando despues del error\n");
    }

    printf("\nCaso de pipe de 3 con comando invalido en el medio\n");
    {
        char *argv1[] = {"echo", "hola", NULL};
        char *argv2[] = {"comandoquenoexiste", NULL};
        char *argv3[] = {"wc", "-l", NULL};
        Comando cmds[3] = {
            { argv1, { NULL, NULL, 0 } },
            { argv2, { NULL, NULL, 0 } },
            { argv3, { NULL, NULL, 0 } },
        };
        Pipeline p = { cmds, 3, 0 };
        ejecutar_pipeline(&p);
        printf("el proceso padre sigue funcionando, el pipeline no fallo\n");
    }

    printf("\n");
    verificar_sigchld("despues de los casos en foreground");

    printf("\n===== PARTE 2: BACKGROUND =====\n");

    printf("\nCaso 10 comandos rapidos en background (echo hola &)\n");
    printf("Prueba la carrera de SIGCHLD: no debe quedar ningun job Ejecutando\n");
    for (int i = 0; i < 10; i++) {
        char *argv1[] = {"echo", "hola", NULL};
        Comando cmds[1] = {
            { argv1, { NULL, NULL, 0 } }
        };
        Pipeline p = { cmds, 1, 1 };   // ultimo campo = background
        ejecutar_pipeline(&p);
    }
    esperar(2);
    jobs_notificar_terminados();
    printf("--- jobs activos (no deberia listar ninguno) ---\n");
    builtin_jobs(NULL);

    printf("\nCaso pipeline en background (sleep 1 | cat &)\n");
    printf("El nombre del job debe verse completo: sleep 1 | cat\n");
    {
        char *argv1[] = {"sleep", "1", NULL};
        char *argv2[] = {"cat", NULL};
        Comando cmds[2] = {
            { argv1, { NULL, NULL, 0 } },
            { argv2, { NULL, NULL, 0 } },
        };
        Pipeline p = { cmds, 2, 1 };
        ejecutar_pipeline(&p);
    }
    printf("--- jobs activos (deberia listar sleep 1 | cat) ---\n");
    builtin_jobs(NULL);
    esperar(2);
    jobs_notificar_terminados();

    printf("\n");
    verificar_sigchld("despues de los casos en background");

    printf("\nRevisar out1.txt y out3.txt.\n");
    return 0;
}