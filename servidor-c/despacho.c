#include "despacho.h"

#include <stdio.h>
#include <string.h>

#include <cjson/cJSON.h>

#include "protocolo.h"
#include "registro.h"
#include "respuesta.h"

// El nombre que viaja en "op" es el mismo identificador de protocolo.h:
// OP(OP_LOGIN) se convierte en { "OP_LOGIN", OP_LOGIN }.
#define OP(nombre) { #nombre, nombre }

static const struct {
    const char *nombre;
    TipoOperacion operacion;
} OPERACIONES[] = {
    OP(OP_LOGIN),
    OP(OP_CREAR_USUARIO),
    OP(OP_RECUPERAR_PASS),
    OP(OP_VERIFICAR_USUARIO),
    OP(OP_LOGOUT),
    OP(OP_LISTAR_PRODUCTOS),
    OP(OP_CREAR_PEDIDO),
    OP(OP_LISTAR_PEDIDOS),
    OP(OP_CAMBIAR_ESTADO_PEDIDO),
    OP(OP_LISTAR_USUARIOS),
    OP(OP_MODIFICAR_USUARIO),
    OP(OP_ELIMINAR_USUARIO),
    OP(OP_OBTENER_USUARIO),
    OP(OP_MODIFICAR_PRODUCTO),
    OP(OP_ELIMINAR_PRODUCTO),
    OP(OP_CREAR_PRODUCTO),
    OP(OP_OBTENER_PRODUCTO),
    OP(OP_LISTAR_INGREDIENTES),
    OP(OP_CREAR_INGREDIENTE),
    OP(OP_MODIFICAR_INGREDIENTE),
    OP(OP_ELIMINAR_INGREDIENTE),
    OP(OP_OBTENER_INGREDIENTE),
    OP(OP_ELIMINAR_PEDIDO),
    OP(OP_OBTENER_PEDIDO),
    OP(OP_VERIFICAR_DISPONIBILIDAD_PRODUCTO),
    OP(OP_LISTAR_LOGS),
    OP(OP_VER_LOG),
};

#define NUM_OPERACIONES (sizeof OPERACIONES / sizeof OPERACIONES[0])

// Regresa 1 y llena *operacion si el nombre existe; 0 si no
static int buscar_operacion(const char *nombre, TipoOperacion *operacion)
{
    for (size_t i = 0; i < NUM_OPERACIONES; i++) {
        if (strcmp(OPERACIONES[i].nombre, nombre) == 0) {
            *operacion = OPERACIONES[i].operacion;
            return 1;
        }
    }
    return 0;
}

// Es el switch de procesarPeticion en Servidor.c. A diferencia del original,
// revisa quién llama: sin sesión solo se puede hacer login.
static cJSON *procesar_peticion(Cliente *cliente, TipoOperacion operacion, const cJSON *datos)
{
    (void)datos; // Lo usarán las operaciones de las siguientes partes

    if (operacion != OP_LOGIN && !cliente->sesion.activa)
        return respuesta_error("NO_AUTENTICADO", "Primero inicia sesión");

    switch (operacion) {
    default:
        return respuesta_error("NO_IMPLEMENTADA", "Esta operación todavía no está en el servidor");
    }
}

char *despachar(Cliente *cliente, const char *texto, size_t largo)
{
    cJSON *peticion = cJSON_ParseWithLength(texto, largo);
    const cJSON *id = cJSON_GetObjectItemCaseSensitive(peticion, "id");
    const cJSON *op = cJSON_GetObjectItemCaseSensitive(peticion, "op");
    TipoOperacion operacion;
    cJSON *respuesta;

    if (!cJSON_IsObject(peticion) || id == NULL || !cJSON_IsString(op) || op->valuestring[0] == '\0') {
        respuesta = respuesta_error("PETICION_INVALIDA", "La petición debe ser JSON con id y op");
    } else if (!buscar_operacion(op->valuestring, &operacion)) {
        char mensaje[160];
        snprintf(mensaje, sizeof mensaje, "Operación desconocida: %.100s", op->valuestring);
        respuesta = respuesta_error("OP_DESCONOCIDA", mensaje);
    } else {
        const cJSON *datos = cJSON_GetObjectItemCaseSensitive(peticion, "datos");
        respuesta = procesar_peticion(cliente, operacion, datos);
        registrar("%s %s: %s", cliente->ip, op->valuestring,
                  cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(respuesta, "ok")) ? "ok" : "error");
    }

    // La respuesta regresa el mismo id, para que el cliente sepa a qué petición
    // corresponde. Si la petición no traía uno, va null.
    cJSON_AddItemToObject(respuesta, "id", id != NULL ? cJSON_Duplicate(id, 1) : cJSON_CreateNull());

    char *salida = cJSON_PrintUnformatted(respuesta);
    cJSON_Delete(respuesta);
    cJSON_Delete(peticion);
    return salida;
}
