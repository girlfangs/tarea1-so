#ifndef JOBS_H
#define JOBS_H

#include <sys/types.h>

#define MAX_JOBS 64

// Estado de un proceso en background
typedef enum {
    JOB_EJECUTANDO,
    JOB_TERMINADO
} JobEstado;

// Estructura para registrar procesos en background
typedef struct {
    int job_id;            // Número correlativo: [1], [2], etc.
    pid_t pid;             // PID asignado por el sistema operativo
    char comando[256];     // Nombre o comando ejecutado
    JobEstado estado;      // EJECUTANDO o TERMINADO
    unsigned long prev_cpu;// Ticks de CPU previos (para calcular %CPU en pmon)
} Job;

// --- Funciones de administración de jobs y señales ---
void jobs_init(void);
int  jobs_agregar(pid_t pid, const char *cmd_line);
void jobs_notificar_terminados(void);
void instalar_sigchld(void);

// --- Funciones de Comandos Internos (Built-ins) ---
int builtin_cd(char **argv);
int builtin_exit(char **argv);
int builtin_jobs(char **argv);
int builtin_pmon(char **argv);

#endif