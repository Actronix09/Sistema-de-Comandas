#include "registro.h"

#include <stdarg.h>
#include <stdio.h>
#include <time.h>

void registrar(const char *formato, ...)
{
    char hora[32];
    time_t ahora = time(NULL);
    struct tm tiempo;
    localtime_r(&ahora, &tiempo);
    strftime(hora, sizeof hora, "%Y-%m-%d %H:%M:%S", &tiempo);

    va_list argumentos;
    va_start(argumentos, formato);
    // Bloquea stderr para que dos hilos no intercalen sus líneas
    flockfile(stderr);
    fprintf(stderr, "[%s] ", hora);
    vfprintf(stderr, formato, argumentos);
    fputc('\n', stderr);
    funlockfile(stderr);
    va_end(argumentos);
}
