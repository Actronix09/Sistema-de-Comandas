#ifndef DATOS_H
#define DATOS_H

#include <stddef.h>

#include <cjson/cJSON.h>

#include "cliente.h"

// Lo que comparten todas las operaciones: leer el "datos" de una petición,
// revisar quién llama y saber si el servidor corre como demo.

// El texto de un campo, o NULL si no viene o no es texto
const char *dato_texto(const cJSON *objeto, const char *campo);

// Lee un entero dentro de [minimo, maximo]. Regresa 1 si está bien.
int dato_entero(const cJSON *objeto, const char *campo, int minimo, int maximo, int *salida);

// Lee un importe o una cantidad: número de 0 a maximo, redondeado a "decimales"
// cifras y escrito como texto para pasárselo a PostgreSQL sin perder nada
// ("85.50"). Regresa 1 si está bien.
int dato_numero(const cJSON *objeto, const char *campo, double maximo, int decimales, char *salida,
                size_t tam);

// Lee un arreglo de ids enteros positivos (sin repetidos, hasta "maximo") y lo
// escribe como arreglo de PostgreSQL: "{3,5,8}". Regresa cuántos leyó, o 0 si
// el arreglo falta, está vacío o trae algo que no es un id.
int dato_ids(const cJSON *objeto, const char *campo, int maximo, char *salida, size_t tam);

// Copia el texto sin los espacios de los extremos. Regresa 1 si cupo.
int recortar(const char *texto, char *salida, size_t tam);

// Cuenta caracteres, no bytes: "ñ" ocupa dos bytes en UTF-8 y es uno solo
size_t caracteres_utf8(const char *cadena);

// El mismo formato que ck_usuario_formato (migración 002)
int usuario_con_formato(const char *usuario);

// NULL si la sesión tiene ese rol; si no, la respuesta de error
cJSON *exigir_rol(const Cliente *cliente, const char *rol);
cJSON *exigir_administrador(const Cliente *cliente);

// COMANDAS_DEMO=1: el servidor es el de la demo pública, donde cualquiera entra
// con las cuentas de prueba. Ahí no se borran usuarios ni se cambian contraseñas
// o nombres de acceso, para que las cuentas de prueba sigan sirviendo.
int modo_demo(void);

#endif
