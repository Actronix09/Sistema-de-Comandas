#ifndef AVISOS_H
#define AVISOS_H

#include <cjson/cJSON.h>

#include "cliente.h"

// Avisos: mensajes que el servidor manda sin que el cliente los pida, cuando
// otra persona cambia algo. No llevan "id", y así el cliente los distingue de
// las respuestas:
//   {"aviso": "comandas"}                       algo cambió; vuelve a pedir la lista
//   {"aviso": "orden_lista", "datos": {...}}    solo al mesero de esa orden
//
// Los nombres son las señales de Api.qml: comandas, cuentas, inventario,
// usuarios y orden_lista.

// Lleva la cuenta de las conexiones abiertas (lo llama conexion.c)
void avisos_agregar(Cliente *cliente);
void avisos_quitar(Cliente *cliente);

// La sesión de un cliente se cambia con el candado tomado, porque otro hilo
// puede estar leyéndola para decidir a quién avisar
void avisos_bloquear(void);
void avisos_desbloquear(void);

// A todos los que tienen sesión iniciada
void avisar_todos(const char *aviso);

// Solo a las conexiones de ese usuario. "datos" no se libera.
void avisar_usuario(int id_usuario, const char *aviso, const cJSON *datos);

#endif
