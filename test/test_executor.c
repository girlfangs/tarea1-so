#include <stdio.h>
#include "executor.h"

int main(void) {
    char *argv1[] = {"echo", "hola", NULL};
    Comando cmd = { argv1, { NULL, NULL, 0 } };
    Pipeline p = { &cmd, 1, 0 };

    int rc = ejecutar_ejecutor(&p);
    if (rc != 0) {
        fprintf(stderr, "ejecutar_ejecutor fallo con rc=%d\n", rc);
        return 1;
    }

    puts("executor ok");
    return 0;
}
