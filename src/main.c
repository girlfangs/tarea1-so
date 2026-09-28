#include <stdio.h>
#include <stdbool.h>

#include "parser.h"
#include "executor.h"

int main(void) {
    bool running = true;
    // preparar terminal

    // entrar en loop de I/O
    while(running) {
        parse_cmdline(NULL, 100);
    }

    char *argv1[] = {"echo", "hola", NULL};
    Comando cmd = { argv1, { NULL, NULL, 0 } };
    Pipeline p = { &cmd, 1, 0 };

    return ejecutar_ejecutor(&p);
}
