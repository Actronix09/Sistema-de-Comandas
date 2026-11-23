#ifndef RESPUESTA_H
#define RESPUESTA_H

#include <cjson/cJSON.h>

// Las dos formas de respuesta del contrato de mensajes:
//   {"ok": true,  "datos": ...}
//   {"ok": false, "error": {"codigo": "...", "mensaje": "..."}}
// El "id" de la petición lo agrega despachar() al final.

// Se queda con "datos" (lo libera junto con la respuesta). Puede ser NULL.
cJSON *respuesta_ok(cJSON *datos);

// "mensaje" siempre se puede mostrar al usuario: nunca lleva SQL ni detalles
// internos; esos van solo al log.
cJSON *respuesta_error(const char *codigo, const char *mensaje);

#endif
