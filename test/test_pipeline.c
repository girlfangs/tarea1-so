#include <stdio.h>
#include "pipeline.h"

int main(void) {
    printf("Caso sin pipe, solo redireccion (sort < in.txt > out1.txt)\n");
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

    printf("\nRevisar out1.txt y out3.txt.\n");
    return 0;
}