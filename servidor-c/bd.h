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

#endif
