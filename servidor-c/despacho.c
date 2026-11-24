#include "despacho.h"

#include <stdio.h>
#include <string.h>

#include <cjson/cJSON.h>

#include "operaciones.h"
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
    OP(OP_EVENTO_EN_CURSO),
    OP(OP_LISTAR_MESAS),
    OP(OP_LISTAR_CUENTAS),
    OP(OP_CREAR_CUENTA),
    OP(OP_LISTAR_CUENTAS_CAJA),
    OP(OP_DETALLE_CUENTA),
    OP(OP_REGISTRAR_PAGO),
    OP(OP_CERRAR_CUENTA),
    OP(OP_CANCELAR_CUENTA),
    OP(OP_AJUSTAR_STOCK),
    OP(OP_REINICIAR_STOCK),
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
// revisa quién llama: sin sesión solo se puede hacer login y ver el evento en
// curso, que la pantalla de login muestra. Cada operación revisa además el rol.
static cJSON *procesar_peticion(Cliente *cliente, TipoOperacion operacion, const cJSON *datos)
{
    if (operacion != OP_LOGIN && operacion != OP_EVENTO_EN_CURSO && !cliente->sesion.activa)
        return respuesta_error("NO_AUTENTICADO", "Primero inicia sesión");

    switch (operacion) {
    case OP_LOGIN:
        return op_login(cliente, datos);

    case OP_LOGOUT:
        return op_logout(cliente);

    case OP_LISTAR_PRODUCTOS:
        return op_listar_productos();

    case OP_CREAR_PEDIDO:
        return op_crear_pedido(cliente, datos);

    case OP_EVENTO_EN_CURSO:
        return op_evento_en_curso();
    case OP_LISTAR_MESAS:
        return op_listar_mesas(cliente);
    case OP_LISTAR_CUENTAS:
        return op_listar_cuentas(datos);
    case OP_CREAR_CUENTA:
        return op_crear_cuenta(cliente, datos);
    case OP_LISTAR_CUENTAS_CAJA:
        return op_listar_cuentas_caja(cliente);
    case OP_DETALLE_CUENTA:
        return op_detalle_cuenta(cliente, datos);
    case OP_REGISTRAR_PAGO:
        return op_registrar_pago(cliente, datos);
    case OP_CERRAR_CUENTA:
        return op_cerrar_cuenta(cliente, datos);
    case OP_CANCELAR_CUENTA:
        return op_cancelar_cuenta(cliente, datos);

    case OP_LISTAR_PEDIDOS:
        return op_listar_comandas(datos);
    case OP_CAMBIAR_ESTADO_PEDIDO:
        return op_cambiar_estado(cliente, datos);
    case OP_ELIMINAR_PEDIDO:
        return op_cancelar_comandas(cliente, datos);

    case OP_LISTAR_INGREDIENTES:
        return op_listar_ingredientes();
    case OP_CREAR_INGREDIENTE:
        return op_guardar_ingrediente(cliente, datos, 1);
    case OP_MODIFICAR_INGREDIENTE:
        return op_guardar_ingrediente(cliente, datos, 0);
    case OP_AJUSTAR_STOCK:
        return op_ajustar_stock(cliente, datos);
    case OP_ELIMINAR_INGREDIENTE:
        return op_eliminar_ingrediente(cliente, datos);
    case OP_REINICIAR_STOCK:
        return op_reiniciar_stock(cliente);

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
