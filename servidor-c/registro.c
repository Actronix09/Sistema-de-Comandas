#include "registro.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <time.h>

// La bitácora es un arreglo circular: al llenarse, cada línea nueva ocupa el
// lugar de la más vieja. Se pierde al reiniciar el servidor; el log completo
// queda en el del contenedor.
#define LINEAS_BITACORA 500
#define TAM_LINEA 320

static struct {
    char fecha[20];
    char texto[TAM_LINEA];
} bitacora[LINEAS_BITACORA];
static int siguiente;  // dónde va la próxima línea
static int guardadas;  // cuántas hay (hasta LINEAS_BITACORA)
static pthread_mutex_t candado = PTHREAD_MUTEX_INITIALIZER;

static void escribir(int a_bitacora, const char *formato, va_list argumentos)
{
    char hora[20];
    time_t ahora = time(NULL);
    struct tm tiempo;
    localtime_r(&ahora, &tiempo);
    strftime(hora, sizeof hora, "%Y-%m-%d %H:%M:%S", &tiempo);

    char texto[TAM_LINEA];
    vsnprintf(texto, sizeof texto, formato, argumentos);

    // Bloquea stderr para que dos hilos no intercalen sus líneas
    flockfile(stderr);
    fprintf(stderr, "[%s] %s\n", hora, texto);
    funlockfile(stderr);

    if (!a_bitacora)
        return;
    pthread_mutex_lock(&candado);
    snprintf(bitacora[siguiente].fecha, sizeof bitacora[siguiente].fecha, "%s", hora);
    snprintf(bitacora[siguiente].texto, sizeof bitacora[siguiente].texto, "%s", texto);
    siguiente = (siguiente + 1) % LINEAS_BITACORA;
    if (guardadas < LINEAS_BITACORA)
        guardadas++;
    pthread_mutex_unlock(&candado);
}

void registrar(const char *formato, ...)
{
    va_list argumentos;
    va_start(argumentos, formato);
    escribir(1, formato, argumentos);
    va_end(argumentos);
}

void registrar_solo_log(const char *formato, ...)
{
    va_list argumentos;
    va_start(argumentos, formato);
    escribir(0, formato, argumentos);
    va_end(argumentos);
}

cJSON *registro_bitacora(void)
{
    cJSON *lista = cJSON_CreateArray();
    pthread_mutex_lock(&candado);
    for (int i = 1; i <= guardadas; i++) {
        int j = (siguiente - i + LINEAS_BITACORA) % LINEAS_BITACORA;
        cJSON *linea = cJSON_CreateObject();
        cJSON_AddStringToObject(linea, "fecha", bitacora[j].fecha);
        cJSON_AddStringToObject(linea, "texto", bitacora[j].texto);
        cJSON_AddItemToArray(lista, linea);
    }
    pthread_mutex_unlock(&candado);
    return lista;
}
