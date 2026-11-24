#include "bd.h"

#include <pthread.h>
#include <stdio.h>
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
    const char *restriccion = PQresultErrorField(resultado, PG_DIAG_CONSTRAINT_NAME);

    // Clase 23: violación de integridad. Los RAISE de los triggers (con
    // ERRCODE = 'check_violation') traen un mensaje en español para el usuario
    // y no nombran restricción; esos se pasan tal cual, con mayúscula inicial.
    // Los de una restricción (CHECK, UNIQUE, clave foránea) los escribe
    // PostgreSQL en inglés, así que se cambian por uno en español que la nombra.
    if (estado != NULL && strncmp(estado, "23", 2) == 0 && mensaje != NULL) {
        char texto[300];
        if (restriccion == NULL) {
            snprintf(texto, sizeof texto, "%s", mensaje);
            if (texto[0] >= 'a' && texto[0] <= 'z')
                texto[0] = (char)(texto[0] - 'a' + 'A');
        } else if (strcmp(estado, "23505") == 0) {
            snprintf(texto, sizeof texto, "Ya existe un registro con ese dato (%s)", restriccion);
        } else if (strcmp(estado, "23503") == 0) {
            snprintf(texto, sizeof texto, "Otros registros dependen de este dato (%s)", restriccion);
        } else {
            snprintf(texto, sizeof texto, "Los datos no cumplen una regla de la base (%s)", restriccion);
        }
        return respuesta_error("VALIDACION_FALLIDA", texto);
    }

    registrar_libpq("Error en la base", resultado != NULL ? PQresultErrorMessage(resultado) : "sin resultado");
    return respuesta_error("ERROR_BD", "No se pudo consultar la base de datos");
}

cJSON *bd_paso(PGconn *conexion, const char *sql, int num_parametros, const char *const *parametros,
               cJSON **json)
{
    PGresult *resultado = PQexecParams(conexion, sql, num_parametros, NULL, parametros, NULL, NULL, 0);
    if (!bd_correcto(resultado)) {
        cJSON *error = bd_respuesta_error(resultado);
        PQclear(resultado);
        return error;
    }
    if (json != NULL) {
        *json = NULL;
        if (PQntuples(resultado) > 0 && PQnfields(resultado) > 0 && !PQgetisnull(resultado, 0, 0)) {
            *json = cJSON_Parse(PQgetvalue(resultado, 0, 0));
            if (*json == NULL) {
                PQclear(resultado);
                registrar("La base regresó un JSON que no se pudo leer");
                return respuesta_error("ERROR_BD", "La base regresó datos que no se pudieron leer");
            }
        }
    }
    PQclear(resultado);
    return NULL;
}

cJSON *bd_revisar(PGconn *conexion, const char *sql, int num_parametros, const char *const *parametros)
{
    PGresult *resultado = PQexecParams(conexion, sql, num_parametros, NULL, parametros, NULL, NULL, 0);
    cJSON *respuesta = NULL;
    if (!bd_correcto(resultado))
        respuesta = bd_respuesta_error(resultado);
    else if (PQntuples(resultado) > 0 && !PQgetisnull(resultado, 0, 0))
        respuesta = respuesta_error("VALIDACION_FALLIDA", PQgetvalue(resultado, 0, 0));
    PQclear(resultado);
    return respuesta;
}

cJSON *bd_consultar(const char *sql, int num_parametros, const char *const *parametros, const char *vacio)
{
    PGconn *conexion = bd_tomar();
    if (conexion == NULL)
        return bd_respuesta_sin_conexion();
    cJSON *datos = NULL;
    cJSON *error = bd_paso(conexion, sql, num_parametros, parametros, &datos);
    bd_soltar(conexion);
    if (error != NULL)
        return error;
    if (datos == NULL && vacio != NULL)
        return respuesta_error("VALIDACION_FALLIDA", vacio);
    return respuesta_ok(datos);
}

cJSON *bd_respuesta_sin_conexion(void)
{
    return respuesta_error("ERROR_BD", "No hay conexión con la base de datos");
}
