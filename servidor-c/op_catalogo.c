// Operaciones del catálogo: productos (con su receta y variantes) y paquetes
// (con los productos que incluyen). Solo las hace un administrador; la lista
// de productos que ven los meseros está en operaciones.c.
//
// El título de un paquete va al inicio de "detalles", como "Título: descripción"
// (la base no tiene columna de título); lo separa el cliente.
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

#define MAX_RECETA 30
#define MAX_PRODUCTOS_PAQUETE 100
#define TAM_IDS (MAX_PRODUCTOS_PAQUETE * 11 + 3)

// ---------------------------------------------------------------------------
//  OP_CREAR_PRODUCTO y OP_MODIFICAR_PRODUCTO
//  datos: { id_producto (al modificar), nombre, descripcion, categoria, precio,
//           id_producto_principal (null si no es variante),
//           receta: [{ id_ingrediente, cantidad }] }
// ---------------------------------------------------------------------------

// Revisa la receta y la deja como JSON limpio para json_to_recordset
static cJSON *validar_receta(const cJSON *receta, char **json)
{
    if (receta != NULL && !cJSON_IsNull(receta) && !cJSON_IsArray(receta))
        return respuesta_error("VALIDACION_FALLIDA", "La receta debe ser una lista");
    if (cJSON_GetArraySize(receta) > MAX_RECETA)
        return respuesta_error("VALIDACION_FALLIDA", "La receta tiene demasiados ingredientes");

    cJSON *limpia = cJSON_CreateArray();
    int vistos[MAX_RECETA], total = 0;
    const cJSON *linea;
    cJSON_ArrayForEach(linea, receta) {
        int id;
        const cJSON *cantidad = cJSON_GetObjectItemCaseSensitive(linea, "cantidad");
        if (!dato_entero(linea, "id_ingrediente", 1, 2147483647, &id)) {
            cJSON_Delete(limpia);
            return respuesta_error("VALIDACION_FALLIDA", "Elige el ingrediente de cada línea de la receta");
        }
        if (!cJSON_IsNumber(cantidad) || !(cantidad->valuedouble > 0 && cantidad->valuedouble <= 9999999.999)) {
            cJSON_Delete(limpia);
            return respuesta_error("VALIDACION_FALLIDA", "Cada ingrediente de la receta necesita una cantidad mayor que 0");
        }
        for (int i = 0; i < total; i++) {
            if (vistos[i] == id) {
                cJSON_Delete(limpia);
                return respuesta_error("VALIDACION_FALLIDA", "Un ingrediente aparece dos veces en la receta");
            }
        }
        vistos[total++] = id;
        cJSON *fila = cJSON_CreateObject();
        cJSON_AddNumberToObject(fila, "id_ingrediente", id);
        cJSON_AddNumberToObject(fila, "cantidad", cantidad->valuedouble);
        cJSON_AddItemToArray(limpia, fila);
    }
    *json = cJSON_PrintUnformatted(limpia);
    cJSON_Delete(limpia);
    return NULL;
}

// $1 nombre, $2 id del producto (NULL al crear), $3 principal, $4 receta
static const char SQL_REVISAR_PRODUCTO[] =
    "SELECT CASE"
    "  WHEN EXISTS (SELECT 1 FROM producto WHERE lower(nombre) = lower($1::text)"
    "                                        AND id_producto IS DISTINCT FROM $2::integer)"
    "  THEN 'Ya existe un producto llamado «' || $1::text || '»'"
    "  WHEN $2::integer IS NOT NULL AND NOT EXISTS (SELECT 1 FROM producto WHERE id_producto = $2::integer)"
    "  THEN 'El producto ya no existe'"
    "  WHEN $3::integer IS NOT NULL AND NOT EXISTS (SELECT 1 FROM producto WHERE id_producto = $3::integer)"
    "  THEN 'El producto principal ya no existe'"
    "  WHEN $3::integer = $2::integer THEN 'Un producto no puede ser variante de sí mismo'"
    "  WHEN EXISTS (SELECT 1 FROM json_to_recordset($4::json) AS l(id_ingrediente integer, cantidad numeric)"
    "                WHERE NOT EXISTS (SELECT 1 FROM ingrediente i WHERE i.id_ingrediente = l.id_ingrediente))"
    "  THEN 'Uno de los ingredientes de la receta ya no existe'"
    " END";

static const char SQL_CREAR_PRODUCTO[] =
    "INSERT INTO producto (nombre, descripcion, categoria, precio, id_producto_principal)"
    " VALUES ($1::text, NULLIF($2::text, ''), $3::text, $4::numeric, $5::integer)"
    " RETURNING id_producto::text";

static const char SQL_MODIFICAR_PRODUCTO[] =
    "UPDATE producto SET nombre = $1::text, descripcion = NULLIF($2::text, ''), categoria = $3::text,"
    "       precio = $4::numeric, id_producto_principal = $5::integer"
    " WHERE id_producto = $6::integer"
    " RETURNING id_producto::text";

// La receta se reemplaza completa (099-permisos.sql le da DELETE a la app para esto)
static const char SQL_BORRAR_RECETA[] = "DELETE FROM receta WHERE id_producto = $1::integer";
static const char SQL_GUARDAR_RECETA[] =
    "INSERT INTO receta (id_producto, id_ingrediente, cantidad_por_porcion)"
    " SELECT $1::integer, l.id_ingrediente, l.cantidad"
    "   FROM json_to_recordset($2::json) AS l(id_ingrediente integer, cantidad numeric)";

cJSON *op_guardar_producto(Cliente *cliente, const cJSON *datos, int es_nuevo)
{
    cJSON *respuesta = exigir_administrador(cliente);
    if (respuesta != NULL)
        return respuesta;

    int id_producto = 0;
    if (!es_nuevo && !dato_entero(datos, "id_producto", 1, 2147483647, &id_producto))
        return respuesta_error("VALIDACION_FALLIDA", "Falta el producto o no es válido");
    char nombre[256], descripcion[1024] = "";
    const char *recibido = dato_texto(datos, "nombre");
    if (recibido == NULL || !recortar(recibido, nombre, sizeof nombre) || nombre[0] == '\0'
        || caracteres_utf8(nombre) > 80)
        return respuesta_error("VALIDACION_FALLIDA", "El nombre es obligatorio y admite hasta 80 caracteres");
    recibido = dato_texto(datos, "descripcion");
    if (recibido != NULL && (!recortar(recibido, descripcion, sizeof descripcion) || caracteres_utf8(descripcion) > 255))
        return respuesta_error("VALIDACION_FALLIDA", "La descripción admite hasta 255 caracteres");
    const char *categoria = dato_texto(datos, "categoria");
    if (categoria == NULL || (strcmp(categoria, "alimentos") != 0 && strcmp(categoria, "bebidas") != 0
                              && strcmp(categoria, "dulceria") != 0))
        return respuesta_error("VALIDACION_FALLIDA", "Elige la categoría");
    char precio[32];
    if (!dato_numero(datos, "precio", 99999999.99, 2, precio, sizeof precio))
        return respuesta_error("VALIDACION_FALLIDA", "El precio debe ser un número mayor o igual a 0");
    char principal[16] = "";
    const cJSON *campo_principal = cJSON_GetObjectItemCaseSensitive(datos, "id_producto_principal");
    if (campo_principal != NULL && !cJSON_IsNull(campo_principal)) {
        int id;
        if (!dato_entero(datos, "id_producto_principal", 1, 2147483647, &id))
            return respuesta_error("VALIDACION_FALLIDA", "El producto principal no es válido");
        snprintf(principal, sizeof principal, "%d", id);
    }
    char *receta = NULL;
    if ((respuesta = validar_receta(cJSON_GetObjectItemCaseSensitive(datos, "receta"), &receta)) != NULL)
        return respuesta;

    PGconn *pg = bd_tomar();
    if (pg == NULL) {
        cJSON_free(receta);
        return bd_respuesta_sin_conexion();
    }
    cJSON *nuevo_id = NULL;
    char txt_id[16];
    snprintf(txt_id, sizeof txt_id, "%d", id_producto);
    const char *p_principal = principal[0] != '\0' ? principal : NULL;
    const char *p_revisar[] = { nombre, es_nuevo ? NULL : txt_id, p_principal, receta };
    const char *p_guardar[] = { nombre, descripcion, categoria, precio, p_principal, txt_id };

    PASO(bd_paso(pg, "BEGIN", 0, NULL, NULL));
    PASO(bd_revisar(pg, SQL_REVISAR_PRODUCTO, 4, p_revisar));
    PASO(bd_paso(pg, es_nuevo ? SQL_CREAR_PRODUCTO : SQL_MODIFICAR_PRODUCTO, es_nuevo ? 5 : 6, p_guardar,
                 &nuevo_id));
    if (!cJSON_IsNumber(nuevo_id)) {
        respuesta = respuesta_error("VALIDACION_FALLIDA", "El producto ya no existe");
        goto fin;
    }
    snprintf(txt_id, sizeof txt_id, "%d", nuevo_id->valueint);
    const char *p_borrar[] = { txt_id };
    const char *p_receta[] = { txt_id, receta };
    PASO(bd_paso(pg, SQL_BORRAR_RECETA, 1, p_borrar, NULL));
    PASO(bd_paso(pg, SQL_GUARDAR_RECETA, 2, p_receta, NULL));
    PASO(bd_paso(pg, "COMMIT", 0, NULL, NULL));

    registrar("%s %s el producto %s", cliente->sesion.usuario, es_nuevo ? "agregó" : "modificó", nombre);
    cJSON *resultado = cJSON_CreateObject();
    cJSON_AddNumberToObject(resultado, "id_producto", nuevo_id->valueint);
    cJSON_AddStringToObject(resultado, "nombre", nombre);
    respuesta = respuesta_ok(resultado);
    avisar_todos("catalogo");
    avisar_todos("inventario");

fin:
    cJSON_Delete(nuevo_id);
    cJSON_free(receta);
    bd_soltar(pg);
    return respuesta;
}

// ---------------------------------------------------------------------------
//  OP_ELIMINAR_PRODUCTO: { id_producto }. Solo si nunca se pidió y no está en
//  paquetes ni tiene variantes; su receta se va con él.
// ---------------------------------------------------------------------------

static const char SQL_REVISAR_BAJA_PRODUCTO[] =
    "SELECT CASE WHEN p.id_producto IS NULL THEN 'El producto ya no existe'"
    "            ELSE NULLIF(" SQL_PRODUCTO_SIN_BAJA("p") ", '') END"
    "  FROM (SELECT 1) AS uno LEFT JOIN producto p ON p.id_producto = $1::integer";

static const char SQL_ELIMINAR_PRODUCTO[] =
    "DELETE FROM producto WHERE id_producto = $1::integer RETURNING json_build_object('nombre', nombre)::text";

cJSON *op_eliminar_producto(Cliente *cliente, const cJSON *datos)
{
    cJSON *respuesta = exigir_administrador(cliente);
    if (respuesta != NULL)
        return respuesta;
    int id;
    if (!dato_entero(datos, "id_producto", 1, 2147483647, &id))
        return respuesta_error("VALIDACION_FALLIDA", "Falta el producto o no es válido");

    PGconn *pg = bd_tomar();
    if (pg == NULL)
        return bd_respuesta_sin_conexion();
    cJSON *eliminado = NULL;
    char txt_id[16];
    snprintf(txt_id, sizeof txt_id, "%d", id);
    const char *parametros[] = { txt_id };

    PASO(bd_paso(pg, "BEGIN", 0, NULL, NULL));
    PASO(bd_revisar(pg, SQL_REVISAR_BAJA_PRODUCTO, 1, parametros));
    PASO(bd_paso(pg, SQL_ELIMINAR_PRODUCTO, 1, parametros, &eliminado));
    PASO(bd_paso(pg, "COMMIT", 0, NULL, NULL));

    registrar("%s eliminó el producto %s", cliente->sesion.usuario, txt_id);
    respuesta = respuesta_ok(eliminado);
    eliminado = NULL;
    avisar_todos("catalogo");
    avisar_todos("inventario");

fin:
    cJSON_Delete(eliminado);
    bd_soltar(pg);
    return respuesta;
}

// ---------------------------------------------------------------------------
//  OP_LISTAR_PAQUETES
// ---------------------------------------------------------------------------

static const char SQL_LISTAR_PAQUETES[] =
    "SELECT COALESCE(json_agg(json_build_object("
    "         'numero_paquete', pq.numero_paquete, 'detalles', pq.detalles, 'monto', pq.monto,"
    "         'productos', COALESCE((SELECT json_agg(json_build_object('id_producto', p.id_producto,"
    "                                                 'nombre', p.nombre, 'categoria', p.categoria)"
    "                                               ORDER BY p.nombre)"
    "                                  FROM paquete_producto pp JOIN producto p ON p.id_producto = pp.id_producto"
    "                                 WHERE pp.numero_paquete = pq.numero_paquete), '[]'),"
    "         'eventos', (SELECT count(DISTINCT a.numero_evento) FROM acuerdo a"
    "                      WHERE a.numero_paquete = pq.numero_paquete))"
    "       ORDER BY pq.numero_paquete), '[]')::text"
    "  FROM paquete pq";

cJSON *op_listar_paquetes(Cliente *cliente)
{
    cJSON *negado = exigir_administrador(cliente);
    return negado != NULL ? negado : bd_consultar(SQL_LISTAR_PAQUETES, 0, NULL, NULL);
}

// ---------------------------------------------------------------------------
//  OP_CREAR_PAQUETE y OP_MODIFICAR_PAQUETE
//  datos: { numero_paquete (al modificar), detalles, monto, productos: [ids] }
// ---------------------------------------------------------------------------

static const char SQL_REVISAR_PAQUETE[] =
    "SELECT CASE"
    "  WHEN $1::integer IS NOT NULL AND NOT EXISTS (SELECT 1 FROM paquete WHERE numero_paquete = $1::integer)"
    "  THEN 'El paquete ya no existe'"
    "  WHEN cardinality($2::int[]) <> (SELECT count(*) FROM producto WHERE id_producto = ANY ($2::int[]))"
    "  THEN 'Uno de los productos ya no existe'"
    " END";

static const char SQL_CREAR_PAQUETE[] =
    "INSERT INTO paquete (detalles, monto) VALUES ($1::text, $2::numeric) RETURNING numero_paquete::text";

static const char SQL_MODIFICAR_PAQUETE[] =
    "UPDATE paquete SET detalles = $1::text, monto = $2::numeric WHERE numero_paquete = $3::integer"
    " RETURNING numero_paquete::text";

static const char SQL_BORRAR_CONTENIDO[] = "DELETE FROM paquete_producto WHERE numero_paquete = $1::integer";
static const char SQL_GUARDAR_CONTENIDO[] =
    "INSERT INTO paquete_producto (numero_paquete, id_producto)"
    " SELECT $1::integer, unnest($2::int[])";

cJSON *op_guardar_paquete(Cliente *cliente, const cJSON *datos, int es_nuevo)
{
    cJSON *respuesta = exigir_administrador(cliente);
    if (respuesta != NULL)
        return respuesta;

    int numero = 0;
    if (!es_nuevo && !dato_entero(datos, "numero_paquete", 1, 2147483647, &numero))
        return respuesta_error("VALIDACION_FALLIDA", "Falta el paquete o no es válido");
    char detalles[1024];
    const char *recibido = dato_texto(datos, "detalles");
    if (recibido == NULL || !recortar(recibido, detalles, sizeof detalles) || detalles[0] == '\0'
        || caracteres_utf8(detalles) > 255)
        return respuesta_error("VALIDACION_FALLIDA", "El título y la descripción juntos admiten hasta 255 caracteres");
    char monto[32];
    if (!dato_numero(datos, "monto", 99999999.99, 2, monto, sizeof monto))
        return respuesta_error("VALIDACION_FALLIDA", "El monto debe ser un número mayor o igual a 0");
    char productos[TAM_IDS] = "{}";
    const cJSON *lista = cJSON_GetObjectItemCaseSensitive(datos, "productos");
    if (cJSON_GetArraySize(lista) > 0 && dato_ids(datos, "productos", MAX_PRODUCTOS_PAQUETE, productos, sizeof productos) == 0)
        return respuesta_error("VALIDACION_FALLIDA", "Los productos del paquete no son válidos");

    PGconn *pg = bd_tomar();
    if (pg == NULL)
        return bd_respuesta_sin_conexion();
    cJSON *guardado = NULL;
    char txt_numero[16];
    snprintf(txt_numero, sizeof txt_numero, "%d", numero);
    const char *p_revisar[] = { es_nuevo ? NULL : txt_numero, productos };
    const char *p_guardar[] = { detalles, monto, txt_numero };

    PASO(bd_paso(pg, "BEGIN", 0, NULL, NULL));
    PASO(bd_revisar(pg, SQL_REVISAR_PAQUETE, 2, p_revisar));
    PASO(bd_paso(pg, es_nuevo ? SQL_CREAR_PAQUETE : SQL_MODIFICAR_PAQUETE, es_nuevo ? 2 : 3, p_guardar, &guardado));
    if (!cJSON_IsNumber(guardado)) {
        respuesta = respuesta_error("VALIDACION_FALLIDA", "El paquete ya no existe");
        goto fin;
    }
    snprintf(txt_numero, sizeof txt_numero, "%d", guardado->valueint);
    const char *p_borrar[] = { txt_numero };
    const char *p_contenido[] = { txt_numero, productos };
    PASO(bd_paso(pg, SQL_BORRAR_CONTENIDO, 1, p_borrar, NULL));
    PASO(bd_paso(pg, SQL_GUARDAR_CONTENIDO, 2, p_contenido, NULL));
    PASO(bd_paso(pg, "COMMIT", 0, NULL, NULL));

    registrar("%s %s el paquete %s", cliente->sesion.usuario, es_nuevo ? "creó" : "modificó", txt_numero);
    cJSON *resultado = cJSON_CreateObject();
    cJSON_AddNumberToObject(resultado, "numero_paquete", guardado->valueint);
    respuesta = respuesta_ok(resultado);
    avisar_todos("catalogo");

fin:
    cJSON_Delete(guardado);
    bd_soltar(pg);
    return respuesta;
}

// ---------------------------------------------------------------------------
//  OP_ELIMINAR_PAQUETE: { numero_paquete }. No si algún evento lo tiene
//  acordado (acuerdo tiene ON DELETE RESTRICT); su contenido se va con él.
// ---------------------------------------------------------------------------

static const char SQL_REVISAR_BAJA_PAQUETE[] =
    "SELECT CASE WHEN pq.numero_paquete IS NULL THEN 'El paquete ya no existe'"
    "            WHEN EXISTS (SELECT 1 FROM acuerdo a WHERE a.numero_paquete = pq.numero_paquete)"
    "            THEN 'Hay eventos con este paquete; quítalo de esos eventos primero' END"
    "  FROM (SELECT 1) AS uno LEFT JOIN paquete pq ON pq.numero_paquete = $1::integer";

static const char SQL_ELIMINAR_PAQUETE[] =
    "DELETE FROM paquete WHERE numero_paquete = $1::integer"
    " RETURNING json_build_object('detalles', detalles)::text";

cJSON *op_eliminar_paquete(Cliente *cliente, const cJSON *datos)
{
    cJSON *respuesta = exigir_administrador(cliente);
    if (respuesta != NULL)
        return respuesta;
    int numero;
    if (!dato_entero(datos, "numero_paquete", 1, 2147483647, &numero))
        return respuesta_error("VALIDACION_FALLIDA", "Falta el paquete o no es válido");

    PGconn *pg = bd_tomar();
    if (pg == NULL)
        return bd_respuesta_sin_conexion();
    cJSON *eliminado = NULL;
    char txt_numero[16];
    snprintf(txt_numero, sizeof txt_numero, "%d", numero);
    const char *parametros[] = { txt_numero };

    PASO(bd_paso(pg, "BEGIN", 0, NULL, NULL));
    PASO(bd_revisar(pg, SQL_REVISAR_BAJA_PAQUETE, 1, parametros));
    PASO(bd_paso(pg, SQL_ELIMINAR_PAQUETE, 1, parametros, &eliminado));
    PASO(bd_paso(pg, "COMMIT", 0, NULL, NULL));

    registrar("%s eliminó el paquete %s", cliente->sesion.usuario, txt_numero);
    respuesta = respuesta_ok(eliminado);
    eliminado = NULL;
    avisar_todos("catalogo");

fin:
    cJSON_Delete(eliminado);
    bd_soltar(pg);
    return respuesta;
}
