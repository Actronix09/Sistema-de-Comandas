#ifndef BD_H
#define BD_H

#include <cjson/cJSON.h>
#include <libpq-fe.h>

// Conexiones a PostgreSQL compartidas por los hilos de civetweb.
//
// Una conexión de libpq no se puede usar desde dos hilos a la vez, así que hay
// unas cuantas (COMANDAS_BD_CONEXIONES, 4 por omisión) y cada petición toma una
// libre, la usa y la regresa. Si todas están ocupadas, la petición espera.
//
// Variables de entorno:
//   COMANDAS_BD_HOST, COMANDAS_BD_PUERTO, COMANDAS_BD_NOMBRE,
//   COMANDAS_BD_USUARIO, COMANDAS_BD_CONTRASENA, COMANDAS_BD_CONEXIONES

void bd_iniciar(void);
void bd_cerrar(void);

// Regresa una conexión lista, o NULL si la base no responde
PGconn *bd_tomar(void);

// Regresa la conexión. Si quedó una transacción abierta, la deshace; si la
// conexión se rompió, la cierra y la siguiente petición abre otra.
void bd_soltar(PGconn *conexion);

// 1 si el resultado es correcto (con o sin filas)
int bd_correcto(const PGresult *resultado);

// La respuesta de error para un resultado fallido. Lo que rechazan las reglas
// de la base (CHECK, claves foráneas, triggers) llega con su mensaje, porque
// está escrito para el usuario. Cualquier otro error solo se registra completo
// en el log, y al cliente le llega un mensaje genérico.
cJSON *bd_respuesta_error(const PGresult *resultado);

// Para cuando no hubo ni conexión
cJSON *bd_respuesta_sin_conexion(void);

// El evento con el que trabaja todo el salón: el que está en curso (si por
// error hay dos, el más reciente). Va como subconsulta dentro de cada SQL.
#define SQL_EVENTO                                                                     \
    "(SELECT numero_evento FROM evento WHERE estado = 'en_curso'"                      \
    " ORDER BY fecha DESC, numero_evento DESC LIMIT 1)"

// Una fecha de PostgreSQL en milisegundos desde 1970, como Date.now() en QML
#define SQL_MS(columna) "(extract(epoch FROM " columna ") * 1000)::bigint"

// Las operaciones arman su respuesta en PostgreSQL (json_build_object) y
// reciben un solo valor: el JSON en texto en la primera columna de la
// primera fila. Así los importes llegan como NUMERIC y nunca pasan por un
// double en el servidor.

// Ejecuta un paso. Si "json" no es NULL, deja ahí la primera celda ya
// convertida (NULL si no hubo filas). Regresa NULL si todo bien, o la
// respuesta de error.
cJSON *bd_paso(PGconn *conexion, const char *sql, int num_parametros, const char *const *parametros,
               cJSON **json);

// Una revisión antes de modificar: la consulta regresa un mensaje para el
// usuario si algo impide seguir, o ninguna fila (o NULL) si se puede.
// Regresa la respuesta VALIDACION_FALLIDA con ese mensaje, o NULL.
cJSON *bd_revisar(PGconn *conexion, const char *sql, int num_parametros, const char *const *parametros);

// Para una consulta de solo lectura: toma una conexión, ejecuta y regresa la
// respuesta completa. Si no hubo filas y "vacio" no es NULL, responde
// VALIDACION_FALLIDA con ese mensaje; si es NULL, datos queda en null.
cJSON *bd_consultar(const char *sql, int num_parametros, const char *const *parametros, const char *vacio);

#endif
