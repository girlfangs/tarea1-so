#define _GNU_SOURCE
#include "jobs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>

/* VARIABLES Y ESTRUCTURAS INTERNAS (Privadas a este módulo) */

// Tabla estatica para registrar todos los procesos en segundo plano (jobs)
static Job tabla_jobs[MAX_JOBS];
static int total_jobs = 0;

// Bandera atomica para notificar cuando la alarma de pmon dispara
static volatile sig_atomic_t sigalrm_recibida = 0;

// Bandera atómica para controlar la salida limpia de pmon con Ctrl+C (SIGINT)
static volatile sig_atomic_t pmon_activo = 1;

/* 1. ADMINISTRACIÓN DE JOBS Y SEÑAL SIGCHLD (Requisito R5) */

/**
 * jobs_init - Inicializa la tabla de jobs vaciando todas las entradas.
 * Debe ser llamada una sola vez al arrancar la shell en main()
 */
void jobs_init(void) {
    for (int i = 0; i < MAX_JOBS; i++) {
        tabla_jobs[i].job_id = 0;
        tabla_jobs[i].pid = -1;
        tabla_jobs[i].comando[0] = '\0';
        tabla_jobs[i].estado = JOB_TERMINADO;
        tabla_jobs[i].prev_cpu = 0;
    }
}

/**
 * jobs_agregar - Registra un nuevo proceso lanzado en background ('&')
 * Busca una ranura libre en la tabla, asigna un job_id correlativo e
 * imprime de inmediato en stdout el formato exigido por la pauta: [job_id] PID
 */
int jobs_agregar(pid_t pid, const char *cmd_line) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (tabla_jobs[i].pid <= 0) {
            tabla_jobs[i].job_id = ++total_jobs;
            tabla_jobs[i].pid = pid;
            strncpy(tabla_jobs[i].comando, cmd_line, sizeof(tabla_jobs[i].comando) - 1);
            tabla_jobs[i].comando[sizeof(tabla_jobs[i].comando) - 1] = '\0';
            tabla_jobs[i].estado = JOB_EJECUTANDO;
            tabla_jobs[i].prev_cpu = 0;

            // Formato exigido en el PDF: [job_id] PID
            printf("[%d] %d\n", tabla_jobs[i].job_id, (int)pid);
            fflush(stdout);
            return tabla_jobs[i].job_id;
        }
    }
    fprintf(stderr, "shell: limite maximo de jobs alcanzado (%d)\n", MAX_JOBS);
    return -1;
}

/**
 * manejador_sigchld - Manejador asíncrono para la señal SIGCHLD
 * Se ejecuta cada vez que un hijo termina o cambia de estado
 * CRITICO: Debe usarse un ciclo 'while' con el flag WNOHANG. Como las
 * señales POSIX no se encolan, si varios hijos terminan al mismo tiempo,
 * el kernel solo entrega una señal SIGCHLD. El bucle waitpid garantiza
 * recolectar a TODOS los procesos zombies pendientes sin bloquear la shell.
 */
static void manejador_sigchld(int sig) {
    (void)sig;
    int saved_errno = errno; // Garantiza la reentrancia del manejador
    int status;
    pid_t pid;

    // Recolectar de forma no bloqueante todos los hijos disponibles
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        for (int i = 0; i < MAX_JOBS; i++) {
            if (tabla_jobs[i].pid == pid) {
                tabla_jobs[i].estado = JOB_TERMINADO;
                break;
            }
        }
    }
    errno = saved_errno;
}

/**
 * instalar_sigchld - Registra el manejador de SIGCHLD usando sigaction()
 * Se activa con SA_RESTART para evitar interrumpir llamadas de I/O lentas,
 * y SA_NOCLDSTOP para ignorar eventos de pausa de procesos (solo nos interesa el termino).
 */
void instalar_sigchld(void) {
    struct sigaction sa;
    sa.sa_handler = manejador_sigchld;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sa, NULL);
}

/**
 * jobs_notificar_terminados - Imprime los jobs terminados y libera sus casillas
 * Esta funcion debe llamarse justo antes de desplegar el prompt principal,
 * informando al usuario si sus tareas en background concluyeron (ej: [1]+ Done sleep 30)
 */
void jobs_notificar_terminados(void) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (tabla_jobs[i].pid > 0 && tabla_jobs[i].estado == JOB_TERMINADO) {
            printf("[%d]+  Done                    %s\n", 
                   tabla_jobs[i].job_id, tabla_jobs[i].comando);
            fflush(stdout);

            // Liberar casilla en la tabla
            tabla_jobs[i].pid = -1;
            tabla_jobs[i].job_id = 0;
            tabla_jobs[i].comando[0] = '\0';
        }
    }
}

/* 2. COMANDOS INTERNOS / BUILT-INS (Requisito R2) */

/**
 * builtin_cd - Cambia el directorio de trabajo del proceso padre de la shell
 * Si no se especifica destino, navega a la ruta indicada en $HOME
 */
int builtin_cd(char **argv) {
    const char *destino = (argv != NULL) ? argv[1] : NULL;

    if (destino == NULL) {
        destino = getenv("HOME");
        if (destino == NULL) {
            fprintf(stderr, "cd: la variable HOME no esta definida\n");
            return -1;
        }
    }

    if (chdir(destino) != 0) {
        perror("cd");
        return -1;
    }
    return 0;
}

/**
 * builtin_exit - Finaliza la sesion de la shell
 */
int builtin_exit(char **argv) {
    int codigo = 0;
    if (argv != NULL && argv[1] != NULL) {
        codigo = atoi(argv[1]);
    }
    exit(codigo);
}

/**
 * builtin_jobs - Lista en pantalla los trabajos en background actualmente activos.
 */
int builtin_jobs(char **argv) {
    (void)argv;
    for (int i = 0; i < MAX_JOBS; i++) {
        if (tabla_jobs[i].pid > 0 && tabla_jobs[i].estado == JOB_EJECUTANDO) {
            printf("[%d]   Ejecutando              %s\n", 
                   tabla_jobs[i].job_id, tabla_jobs[i].comando);
        }
    }
    return 0;
}

/* 3. MONITOR DE PROCESOS: pmon (Seccion 3) */

// Manejador para la señal de alarma periodica
static void manejador_sigalrm(int sig) {
    (void)sig;
    sigalrm_recibida = 1;
}

// Manejador para salir limpiamente de pmon al presionar Ctrl+C (SIGINT)
static void manejador_sigint_pmon(int sig) {
    (void)sig;
    pmon_activo = 0;
}

/**
 * obtener_vmrss - Lee la memoria residente fisica (en KB) desde /proc/[pid]/status.
 * Retorna 0 si el archivo no existe o el proceso termino
 */
static long obtener_vmrss(pid_t pid) {
    char ruta[64];
    snprintf(ruta, sizeof(ruta), "/proc/%d/status", (int)pid);
    FILE *f = fopen(ruta, "r");
    if (!f) return 0;

    char linea[128];
    long rss = 0;
    while (fgets(linea, sizeof(linea), f)) {
        if (strncmp(linea, "VmRSS:", 6) == 0) {
            sscanf(linea + 6, "%ld", &rss);
            break;
        }
    }
    fclose(f);
    return rss;
}

/**
 * obtener_info_stat - Lee estado y tiempos de CPU desde /proc/[pid]/stat
 * Retorna 0 en exito, o -1 si el proceso finalizo entre lecturas.
 */
static int obtener_info_stat(pid_t pid, char *estado_char, unsigned long *cpu_ticks) {
    char ruta[64];
    snprintf(ruta, sizeof(ruta), "/proc/%d/stat", (int)pid);
    FILE *f = fopen(ruta, "r");
    if (!f) return -1; // Proceso finalizo o ruta no accesible

    char buffer[512];
    if (!fgets(buffer, sizeof(buffer), f)) {
        fclose(f);
        return -1;
    }
    fclose(f);

    // El campo comm esta entre parentesis y puede contener espacios.
    // Buscamos el último paréntesis cerrado para asegurar un parseo robusto.
    char *cierre_parentesis = strrchr(buffer, ')');
    if (!cierre_parentesis) return -1;

    char state;
    unsigned long utime = 0, stime = 0;
    // Tras ')', los campos son: state (campo 3), ppid(4)... utime(14), stime(15)
    sscanf(cierre_parentesis + 2, 
           "%c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu", 
           &state, &utime, &stime);

    *estado_char = state;
    *cpu_ticks = utime + stime;
    return 0;
}

/**
 * describir_estado - Convierte el caracter de estado de Linux en texto descriptivo.
 */
static const char *describir_estado(char c) {
    switch (c) {
        case 'R': return "ejecutando";
        case 'S': return "durmiendo";
        case 'Z': return "zombie";
        case 'T': return "detenido";
        default:  return "inactivo";
    }
}

/**
 * builtin_pmon - Monitor de procesos en tiempo real
 * Utiliza alarm() y SIGALRM para refrescar la pantalla periodicamente.
 * Se intercepta SIGINT para que presionar Ctrl+C detenga el monitor
 * de forma limpia sin cerrar el proceso principal de la shell.
 */
int builtin_pmon(char **argv) {
    int intervalo = 2; // Por defecto 2 segundos
    if (argv != NULL && argv[1] != NULL) {
        intervalo = atoi(argv[1]);
        if (intervalo <= 0) intervalo = 2;
    }

    pmon_activo = 1;

    // Respaldar disposiciones previas de señales
    struct sigaction sa_alrm_old, sa_int_old;
    struct sigaction sa_alrm, sa_int;

    // Configurar SIGALRM para el ciclo periodico
    sa_alrm.sa_handler = manejador_sigalrm;
    sigemptyset(&sa_alrm.sa_mask);
    sa_alrm.sa_flags = 0;
    sigaction(SIGALRM, &sa_alrm, &sa_alrm_old);

    // Configurar SIGINT para salir de pmon con Ctrl+C de forma controlada
    sa_int.sa_handler = manejador_sigint_pmon;
    sigemptyset(&sa_int.sa_mask);
    sa_int.sa_flags = 0;
    sigaction(SIGINT, &sa_int, &sa_int_old);

    long clk_tck = sysconf(_SC_CLK_TCK);

    // Bucle interactivo
    while (pmon_activo) {
        // Secuencia ANSI estandar para limpiar la pantalla y ubicar cursor al inicio
        printf("\033[H\033[2J");
        printf("%-7s | %-16s | %-12s | %-11s | %s\n", 
               "PID", "COMANDO", "ESTADO", "%CPU (aprox)", "RSS (KB)");
        printf("-----------------------------------------------------------------\n");

        for (int i = 0; i < MAX_JOBS; i++) {
            if (tabla_jobs[i].pid > 0 && tabla_jobs[i].estado == JOB_EJECUTANDO) {
                char est_char;
                unsigned long total_ticks = 0;

                // Si no se puede leer /proc, el proceso termino durante el monitoreo
                if (obtener_info_stat(tabla_jobs[i].pid, &est_char, &total_ticks) < 0) {
                    tabla_jobs[i].estado = JOB_TERMINADO;
                    continue;
                }

                long rss = obtener_vmrss(tabla_jobs[i].pid);

                // Calculo de %CPU respecto al intervalo de tiempo
                double cpu_pct = 0.0;
                if (tabla_jobs[i].prev_cpu > 0 && total_ticks >= tabla_jobs[i].prev_cpu) {
                    unsigned long delta = total_ticks - tabla_jobs[i].prev_cpu;
                    cpu_pct = ((double)delta / (double)clk_tck) / (double)intervalo * 100.0;
                }
                tabla_jobs[i].prev_cpu = total_ticks;

                printf("%-7d | %-16s | %-12s | %-11.1f | %ld\n",
                       tabla_jobs[i].pid,
                       tabla_jobs[i].comando,
                       describir_estado(est_char),
                       cpu_pct,
                       rss);
            }
        }
        fflush(stdout);

        // Programar la siguiente alarma y esperar dormido sin consumir CPU
        sigalrm_recibida = 0;
        alarm(intervalo);
        while (!sigalrm_recibida && pmon_activo) {
            pause(); // Se despierta con SIGALRM o SIGINT
        }
    }

    // Cancelar cualquier alarma pendiente y restaurar los manejadores de señales
    alarm(0);
    sigaction(SIGALRM, &sa_alrm_old, NULL);
    sigaction(SIGINT, &sa_int_old, NULL);

    printf("\n[pmon finalizado]\n");
    fflush(stdout);
    return 0;
}