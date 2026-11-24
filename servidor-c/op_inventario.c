// Operaciones del inventario: ingredientes y sus existencias. Solo las que
// modifican piden ser administrador; la lista la ve cualquiera con sesión.
//
// El stock está en la unidad base del ingrediente (g, ml o pieza) con tres
// decimales, como la columna cantidad de la base.
#include "operaciones.h"

#include <stdio.h>
#include <string.h>

#include "avisos.h"
#include "bd.h"
#include "datos.h"
#include "registro.h"
#include "respuesta.h"

#define PASO(llamada)                                                                   \
    do {                                                                                \
        if ((respuesta = (llamada)) != NULL)                                            \
            goto fin;                                                                   \
    } while (0)

// El mayor valor de NUMERIC(10,3)
#define MAX_CANTIDAD_BD 9999999.999

// "usos" es en cuántas recetas aparece: así la pantalla avisa antes de borrar
#define INGREDIENTE_JSON(i)                                                             \
    "json_build_object('id_ingrediente', " i ".id_ingrediente, 'nombre', " i ".nombre," \
    " 'unidad_medida', " i ".unidad_medida, 'stock', trim_scale(" i ".stock_actual),"   \
    " 'stock_minimo', trim_scale(" i ".stock_minimo),"                                  \
    " 'usos', (SELECT count(*) FROM receta r WHERE r.id_ingrediente = " i ".id_ingrediente))"

static cJSON *leer_id(const cJSON *datos, char *salida, size_t tam)
{
    int id;
    if (!dato_entero(datos, "id_ingrediente", 1, 2147483647, &id))
        return respuesta_error("VALIDACION_FALLIDA", "Falta el ingrediente o no es válido");
    snprintf(salida, tam, "%d", id);
    return NULL;
}

// ---------------------------------------------------------------------------
//  OP_LISTAR_INGREDIENTES
// ---------------------------------------------------------------------------

static const char SQL_LISTAR_INGREDIENTES[] =
    "SELECT COALESCE(json_agg(" INGREDIENTE_JSON("i") " ORDER BY lower(i.nombre)), '[]')::text"
    "  FROM ingrediente i";

cJSON *op_listar_ingredientes(void)
{
    return bd_consultar(SQL_LISTAR_INGREDIENTES, 0, NULL, NULL);
}

// ---------------------------------------------------------------------------
//  OP_CREAR_INGREDIENTE y OP_MODIFICAR_INGREDIENTE
//  datos: { id_ingrediente (solo al modificar), nombre, stock,
//           unidad_medida (g | ml | pieza), stock_minimo }
// ---------------------------------------------------------------------------

// uq_ingrediente_nombre y ux_ingrediente_nombre_minusculas, con mensaje claro
static const char SQL_REVISAR_NOMBRE[] =
    "SELECT 'Ya existe un ingrediente llamado «' || nombre || '»' FROM ingrediente"
    " WHERE lower(nombre) = lower($1::text) AND id_ingrediente IS DISTINCT FROM $2::integer";

static const char SQL_CREAR_INGREDIENTE[] =
    "INSERT INTO ingrediente AS i (nombre, unidad_medida, stock_actual, stock_minimo)"
    " VALUES ($1::text, COALESCE($2::text, 'pieza'), $3::numeric, COALESCE($4::numeric, 0))"
    " RETURNING " INGREDIENTE_JSON("i") "::text";

static const char SQL_MODIFICAR_INGREDIENTE[] =
    "UPDATE ingrediente AS i"
    "   SET nombre = $1::text, unidad_medida = COALESCE($2::text, i.unidad_medida),"
    "       stock_actual = $3::numeric, stock_minimo = COALESCE($4::numeric, i.stock_minimo)"
    " WHERE i.id_ingrediente = $5::integer"
    " RETURNING " INGREDIENTE_JSON("i") "::text";

cJSON *op_guardar_ingrediente(Cliente *cliente, const cJSON *datos, int es_nuevo)
{
    cJSON *respuesta = exigir_administrador(cliente);
    if (respuesta != NULL)
        return respuesta;

    char id[16] = "";
    if (!es_nuevo && (respuesta = leer_id(datos, id, sizeof id)) != NULL)
        return respuesta;
    const char *recibido = dato_texto(datos, "nombre");
    char nombre[256];
    if (recibido == NULL || !recortar(recibido, nombre, sizeof nombre) || nombre[0] == '\0')
        return respuesta_error("VALIDACION_FALLIDA", "El nombre no puede quedar vacío");
    if (caracteres_utf8(nombre) > 60)
        return respuesta_error("VALIDACION_FALLIDA", "El nombre admite hasta 60 caracteres");
    char stock[32];
    if (!dato_numero(datos, "stock", MAX_CANTIDAD_BD, 3, stock, sizeof stock))
        return respuesta_error("VALIDACION_FALLIDA", "La existencia debe ser un número mayor o igual a 0");

    const char *unidad = dato_texto(datos, "unidad_medida");
    if (unidad != NULL && strcmp(unidad, "g") != 0 && strcmp(unidad, "ml") != 0 && strcmp(unidad, "pieza") != 0)
        return respuesta_error("VALIDACION_FALLIDA", "La unidad debe ser g, ml o pieza");
    char minimo[32];
    const char *p_minimo = NULL;
    const cJSON *campo_minimo = cJSON_GetObjectItemCaseSensitive(datos, "stock_minimo");
    if (campo_minimo != NULL && !cJSON_IsNull(campo_minimo)) {
        if (!dato_numero(datos, "stock_minimo", MAX_CANTIDAD_BD, 3, minimo, sizeof minimo))
            return respuesta_error("VALIDACION_FALLIDA", "El mínimo debe ser un número mayor o igual a 0");
        p_minimo = minimo;
    }

    PGconn *pg = bd_tomar();
    if (pg == NULL)
        return bd_respuesta_sin_conexion();
    cJSON *ingrediente = NULL;
    const char *p_revisar[] = { nombre, es_nuevo ? NULL : id };
    const char *p_guardar[] = { nombre, unidad, stock, p_minimo, id };

    PASO(bd_paso(pg, "BEGIN", 0, NULL, NULL));
    PASO(bd_revisar(pg, SQL_REVISAR_NOMBRE, 2, p_revisar));
    PASO(bd_paso(pg, es_nuevo ? SQL_CREAR_INGREDIENTE : SQL_MODIFICAR_INGREDIENTE, es_nuevo ? 4 : 5, p_guardar,
                 &ingrediente));
    if (ingrediente == NULL) {
        respuesta = respuesta_error("VALIDACION_FALLIDA", "El ingrediente ya no existe");
        goto fin;
    }
    PASO(bd_paso(pg, "COMMIT", 0, NULL, NULL));

    registrar("%s %s el ingrediente %s", cliente->sesion.usuario, es_nuevo ? "agregó" : "modificó", nombre);
    respuesta = respuesta_ok(ingrediente);
    ingrediente = NULL;
    avisar_todos("inventario");

fin:
    cJSON_Delete(ingrediente);
    bd_soltar(pg);
    return respuesta;
}

// ---------------------------------------------------------------------------
//  OP_AJUSTAR_STOCK: { id_ingrediente, delta } suma o resta, sin bajar de 0
// ---------------------------------------------------------------------------

static const char SQL_AJUSTAR_STOCK[] =
    "UPDATE ingrediente AS i SET stock_actual = GREATEST(0, i.stock_actual + $2::numeric)"
    " WHERE i.id_ingrediente = $1::integer"
    " RETURNING " INGREDIENTE_JSON("i") "::text";

cJSON *op_ajustar_stock(Cliente *cliente, const cJSON *datos)
{
    cJSON *respuesta = exigir_administrador(cliente);
    char id[16];
    if (respuesta == NULL)
        respuesta = leer_id(datos, id, sizeof id);
    if (respuesta != NULL)
        return respuesta;

    const cJSON *campo = cJSON_GetObjectItemCaseSensitive(datos, "delta");
    double delta = cJSON_IsNumber(campo) ? campo->valuedouble : 0;
    if (!(delta >= -MAX_CANTIDAD_BD && delta <= MAX_CANTIDAD_BD) || delta == 0)
        return respuesta_error("VALIDACION_FALLIDA", "El ajuste debe ser un número distinto de 0");
    char txt_delta[32];
    snprintf(txt_delta, sizeof txt_delta, "%.3f", delta);

    const char *parametros[] = { id, txt_delta };
    respuesta = bd_consultar(SQL_AJUSTAR_STOCK, 2, parametros, "El ingrediente ya no existe");
    if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(respuesta, "ok")))
        avisar_todos("inventario");
    return respuesta;
}

// ---------------------------------------------------------------------------
//  OP_ELIMINAR_INGREDIENTE: también lo quita de las recetas, como en el
//  sistema en C (receta tiene ON DELETE RESTRICT hacia ingrediente)
// ---------------------------------------------------------------------------

static const char SQL_QUITAR_DE_RECETAS[] =
    "WITH quitadas AS (DELETE FROM receta WHERE id_ingrediente = $1::integer RETURNING 1)"
    "SELECT count(*)::text FROM quitadas";

static const char SQL_ELIMINAR_INGREDIENTE[] =
    "DELETE FROM ingrediente WHERE id_ingrediente = $1::integer"
    " RETURNING json_build_object('nombre', nombre)::text";

cJSON *op_eliminar_ingrediente(Cliente *cliente, const cJSON *datos)
{
    cJSON *respuesta = exigir_administrador(cliente);
    char id[16];
    if (respuesta == NULL)
        respuesta = leer_id(datos, id, sizeof id);
    if (respuesta != NULL)
        return respuesta;

    PGconn *pg = bd_tomar();
    if (pg == NULL)
        return bd_respuesta_sin_conexion();
    cJSON *recetas = NULL, *eliminado = NULL;
    const char *parametros[] = { id };

    PASO(bd_paso(pg, "BEGIN", 0, NULL, NULL));
    PASO(bd_paso(pg, SQL_QUITAR_DE_RECETAS, 1, parametros, &recetas));
    PASO(bd_paso(pg, SQL_ELIMINAR_INGREDIENTE, 1, parametros, &eliminado));
    if (eliminado == NULL) {
        respuesta = respuesta_error("VALIDACION_FALLIDA", "El ingrediente ya no existe");
        goto fin;
    }
    PASO(bd_paso(pg, "COMMIT", 0, NULL, NULL));

    // count(*) llega como texto ("2") y cJSON lo lee como número
    cJSON_AddNumberToObject(eliminado, "recetas", cJSON_IsNumber(recetas) ? recetas->valueint : 0);
    registrar("%s eliminó el ingrediente %s", cliente->sesion.usuario, id);
    respuesta = respuesta_ok(eliminado);
    eliminado = NULL;
    avisar_todos("inventario");

fin:
    cJSON_Delete(recetas);
    cJSON_Delete(eliminado);
    bd_soltar(pg);
    return respuesta;
}

// ---------------------------------------------------------------------------
//  OP_REINICIAR_STOCK: todas las existencias a 0 (para el inventario inicial)
// ---------------------------------------------------------------------------

cJSON *op_reiniciar_stock(Cliente *cliente)
{
    cJSON *respuesta = exigir_administrador(cliente);
    if (respuesta != NULL)
        return respuesta;
    respuesta = bd_consultar("UPDATE ingrediente SET stock_actual = 0", 0, NULL, NULL);
    if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(respuesta, "ok"))) {
        registrar("%s dejó todas las existencias en 0", cliente->sesion.usuario);
        avisar_todos("inventario");
    }
    return respuesta;
}
