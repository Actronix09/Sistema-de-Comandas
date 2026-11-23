#include "bd.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#include "registro.h"
#include "respuesta.h"

#define MAX_CONEXIONES 16

typedef struct {
    PGconn *pg; // NULL si todavía no se abre o se rompió
    int ocupada;
} Ranura;

static Ranura ranuras[MAX_CONEXIONES];
static int num_ranuras;
static pthread_mutex_t candado = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t hay_libre = PTHREAD_COND_INITIALIZER;

// Parámetros de PQconnectdbParams. Un valor NULL o vacío se ignora, y libpq
// usa el suyo por omisión.
static const char *const CLAVES[] = {
    "host", "port", "dbname", "user", "password", "application_name", NULL,
};
static const char *valores[sizeof CLAVES / sizeof CLAVES[0]];

static const char *variable(const char *nombre, const char *por_omision)
{
    const char *valor = getenv(nombre);
    return (valor != NULL && valor[0] != '\0') ? valor : por_omision;
}

// Registra un mensaje de libpq sin su salto de línea final
static void registrar_libpq(const char *antes, const char *mensaje)
{
    int largo = (int)strlen(mensaje);
    while (largo > 0 && (mensaje[largo - 1] == '\n' || mensaje[largo - 1] == ' '))
        largo--;
    registrar("%s: %.*s", antes, largo, mensaje);
}

static PGconn *conectar(void)
{
    PGconn *pg = PQconnectdbParams(CLAVES, valores, 0);
    if (PQstatus(pg) != CONNECTION_OK) {
        registrar_libpq("No se pudo conectar a la base", PQerrorMessage(pg));
        PQfinish(pg);
        return NULL;
    }
    PQsetClientEncoding(pg, "UTF8");
    return pg;
}

void bd_iniciar(void)
{
    valores[0] = variable("COMANDAS_BD_HOST", "localhost");
    valores[1] = variable("COMANDAS_BD_PUERTO", "5432");
    valores[2] = variable("COMANDAS_BD_NOMBRE", "comandas");
    valores[3] = variable("COMANDAS_BD_USUARIO", "app_comandas");
    valores[4] = getenv("COMANDAS_BD_CONTRASENA");
    valores[5] = "servidor_comandas";

    num_ranuras = atoi(variable("COMANDAS_BD_CONEXIONES", "4"));
    if (num_ranuras < 1)
        num_ranuras = 1;
    if (num_ranuras > MAX_CONEXIONES)
        num_ranuras = MAX_CONEXIONES;

    // Se prueba una conexión desde el arranque para avisar pronto si algo está
    // mal configurado. Si la base todavía no está lista, no pasa nada: cada
    // petición vuelve a intentar.
    ranuras[0].pg = conectar();
    if (ranuras[0].pg != NULL)
        registrar("Conectado a la base %s en %s:%s como %s (hasta %d conexiones)",
                  valores[2], valores[0], valores[1], valores[3], num_ranuras);
}

void bd_cerrar(void)
{
    pthread_mutex_lock(&candado);
    for (int i = 0; i < num_ranuras; i++) {
        if (ranuras[i].pg != NULL)
            PQfinish(ranuras[i].pg);
        ranuras[i].pg = NULL;
    }
    pthread_mutex_unlock(&candado);
}

static void liberar_ranura(Ranura *ranura)
{
    pthread_mutex_lock(&candado);
    ranura->ocupada = 0;
    pthread_cond_signal(&hay_libre);
    pthread_mutex_unlock(&candado);
}

PGconn *bd_tomar(void)
{
    pthread_mutex_lock(&candado);
    Ranura *ranura = NULL;
    while (ranura == NULL) {
        for (int i = 0; i < num_ranuras; i++) {
            if (!ranuras[i].ocupada) {
                ranura = &ranuras[i];
                break;
            }
        }
        if (ranura == NULL)
            pthread_cond_wait(&hay_libre, &candado);
    }
    ranura->ocupada = 1;
    pthread_mutex_unlock(&candado);

    // Conectar puede tardar, así que se hace sin el candado
    if (ranura->pg == NULL)
        ranura->pg = conectar();
    if (ranura->pg == NULL) {
        liberar_ranura(ranura);
        return NULL;
    }
    return ranura->pg;
}

void bd_soltar(PGconn *conexion)
{
    if (conexion == NULL)
        return;

    Ranura *ranura = NULL;
    for (int i = 0; i < num_ranuras; i++) {
        if (ranuras[i].pg == conexion)
            ranura = &ranuras[i];
    }
    if (ranura == NULL)
        return;

    // Si la base se reinició, esta petición ya falló; la siguiente reconecta
    if (PQstatus(conexion) != CONNECTION_OK) {
        registrar("Se perdió la conexión con la base; se reabre en la siguiente petición");
        PQfinish(conexion);
        ranura->pg = NULL;
    } else if (PQtransactionStatus(conexion) != PQTRANS_IDLE) {
        PQclear(PQexec(conexion, "ROLLBACK"));
    }
    liberar_ranura(ranura);
}

int bd_correcto(const PGresult *resultado)
{
    ExecStatusType estado = PQresultStatus(resultado);
    return estado == PGRES_TUPLES_OK || estado == PGRES_COMMAND_OK;
}

cJSON *bd_respuesta_error(const PGresult *resultado)
{
    const char *estado = PQresultErrorField(resultado, PG_DIAG_SQLSTATE);
    const char *mensaje = PQresultErrorField(resultado, PG_DIAG_MESSAGE_PRIMARY);

    // Clase 23: violación de integridad (incluye los RAISE de los triggers,
    // que usan ERRCODE = 'check_violation')
    if (estado != NULL && strncmp(estado, "23", 2) == 0 && mensaje != NULL)
        return respuesta_error("VALIDACION_FALLIDA", mensaje);

    registrar_libpq("Error en la base", resultado != NULL ? PQresultErrorMessage(resultado) : "sin resultado");
    return respuesta_error("ERROR_BD", "No se pudo consultar la base de datos");
}

cJSON *bd_respuesta_sin_conexion(void)
{
    return respuesta_error("ERROR_BD", "No hay conexión con la base de datos");
}
