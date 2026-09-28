#include <stdio.h>
#include <unistd.h>

#include "parser.h"
#include "executor.h"
#include "jobs.h"

int main(void) {
    // La shell conserva los jobs y controla sus señales desde el proceso padre.
    jobs_init();
    instalar_sigchld();
    instalar_senales_shell();

    for (;;) {
        char cwd[1024];

        jobs_notificar_terminados();
        if (getcwd(cwd, sizeof(cwd)) == NULL) {
            snprintf(cwd, sizeof(cwd), "?");
        }
        printf("miShell:%s$ ", cwd);
        fflush(stdout);

        if (parse_cmdline(NULL, 100) == 0) {
            putchar('\n');
            break;
        }
    }
    return 0;
}
