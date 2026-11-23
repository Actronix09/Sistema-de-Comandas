#ifndef DESPACHO_H
#define DESPACHO_H

#include <stddef.h>

#include "cliente.h"

// Recibe el texto de una petición y regresa el texto JSON de la respuesta,
// que hay que liberar con cJSON_free(). Nunca regresa NULL salvo que se acabe
// la memoria.
char *despachar(Cliente *cliente, const char *texto, size_t largo);

#endif
