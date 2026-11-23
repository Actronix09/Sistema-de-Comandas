#ifndef OPERACIONES_H
#define OPERACIONES_H

#include <cjson/cJSON.h>

#include "cliente.h"

// Arranca libsodium. Regresa 0 si todo bien.
int operaciones_iniciar(void);

// Cada una recibe el "datos" de la petición (puede ser NULL) y regresa la
// respuesta {ok, datos | error}; despacho.c le agrega el id.
cJSON *op_login(Cliente *cliente, const cJSON *datos);
cJSON *op_logout(Cliente *cliente);
cJSON *op_listar_productos(void);
cJSON *op_crear_pedido(Cliente *cliente, const cJSON *datos);

#endif
