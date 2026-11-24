// Operaciones sobre comandas ya creadas: listarlas, avanzar su estado y
// cancelarlas. Crearlas está en operaciones.c (OP_CREAR_PEDIDO).
//
// Las comandas viajan sueltas, una por unidad; el cliente las agrupa en rondas
// y líneas ("2× Cerveza") para mostrarlas.
#include "operaciones.h"

#include <stdio.h>
#include <stdlib.h>
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

// Hasta cuántas comandas se cambian de un jalón (una ronda grande de cocina)
#define MAX_IDS 200
// Lo que ocupa "{1,2,...}" con MAX_IDS ids de hasta 10 cifras
#define TAM_IDS (MAX_IDS * 11 + 3)

// Bebidas van al bar; alimentos y dulcería a cocina, como tg_comanda_estacion
#define SQL_ESTACION(p) "CASE WHEN " p ".categoria = 'bebidas' THEN 'bar' ELSE 'cocina' END"

// El nombre de un estado como lo muestran las pantallas (Tema.nombreEstado)
#define SQL_NOMBRE_ESTADO(e)                                                            \
    "CASE " e " WHEN 'pendiente' THEN 'Pendiente' WHEN 'en_preparacion' THEN 'En preparación'" \
    " WHEN 'lista' THEN 'Lista' WHEN 'entregada' THEN 'Entregada' ELSE 'Cancelada' END"

static const char *const ESTADOS[] = { "pendiente", "en_preparacion", "lista", "entregada", "cancelada" };

static int estado_valido(const char *estado)
{
    for (size_t i = 0; i < sizeof ESTADOS / sizeof ESTADOS[0]; i++) {
        if (strcmp(estado, ESTADOS[i]) == 0)
            return 1;
    }
    return 0;
}

// ---------------------------------------------------------------------------
//  OP_LISTAR_PEDIDOS: filtro { estacion: "cocina" | "bar" | "todas",
//                              id_mesero (0 o ausente = todos), estados: [...] }
// ---------------------------------------------------------------------------

static const char SQL_LISTAR_COMANDAS[] =
    "SELECT COALESCE(json_agg(json_build_object("
    "         'id_comanda', co.id_comanda, 'fecha_hora', " SQL_MS("co.fecha_hora") ","
    "         'secuencial', co.secuencial, 'numero_mesa', c.numero_mesa, 'folio', c.folio,"
    "         'nombre_cuenta', c.nombre, 'cuenta', c.nombre || ' · ' || c.folio,"
    "         'id_mesero', co.id_mesero, 'mesero', u.nombre,"
    "         'id_producto', co.id_producto, 'producto', p.nombre, 'categoria', p.categoria,"
    "         'estacion', " SQL_ESTACION("p") ", 'estado', co.estado,"
    "         'detalles', COALESCE(co.detalles, ''), 'precio_cobrado', co.precio_cobrado)"
    "       ORDER BY co.fecha_hora, co.id_comanda), '[]')::text"
    "  FROM comanda co"
    "  JOIN cuenta c ON c.numero_evento = co.numero_evento AND c.secuencial = co.secuencial"
    "  JOIN producto p ON p.id_producto = co.id_producto"
    "  JOIN usuario u ON u.id_usuario = co.id_mesero"
    " WHERE co.numero_evento = " SQL_EVENTO
    "   AND ($1::text = 'todas' OR " SQL_ESTACION("p") " = $1::text)"
    "   AND ($2::integer = 0 OR co.id_mesero = $2::integer)"
    "   AND (cardinality($3::text[]) = 0 OR co.estado = ANY ($3::text[]))";

cJSON *op_listar_comandas(const cJSON *datos)
{
    const char *estacion = dato_texto(datos, "estacion");
    if (estacion == NULL)
        estacion = "todas";
    if (strcmp(estacion, "todas") != 0 && strcmp(estacion, "cocina") != 0 && strcmp(estacion, "bar") != 0)
        return respuesta_error("VALIDACION_FALLIDA", "La estación debe ser cocina, bar o todas");

    int id_mesero = 0;
    const cJSON *campo_mesero = cJSON_GetObjectItemCaseSensitive(datos, "id_mesero");
    if (campo_mesero != NULL && !cJSON_IsNull(campo_mesero) && !dato_entero(datos, "id_mesero", 0, 2147483647, &id_mesero))
        return respuesta_error("VALIDACION_FALLIDA", "El mesero no es válido");

    // Los estados se revisan contra la lista, así que se pueden escribir tal cual
    char estados[96] = "{";
    const cJSON *lista = cJSON_GetObjectItemCaseSensitive(datos, "estados");
    if (lista != NULL && !cJSON_IsNull(lista)) {
        if (!cJSON_IsArray(lista) || cJSON_GetArraySize(lista) > 5)
            return respuesta_error("VALIDACION_FALLIDA", "Los estados deben ser una lista");
        const cJSON *estado;
        cJSON_ArrayForEach(estado, lista) {
            if (!cJSON_IsString(estado) || !estado_valido(estado->valuestring))
                return respuesta_error("VALIDACION_FALLIDA", "Estado desconocido");
            if (estados[1] != '\0')
                strcat(estados, ",");
            strcat(estados, estado->valuestring);
        }
    }
    strcat(estados, "}");

    char txt_mesero[16];
    snprintf(txt_mesero, sizeof txt_mesero, "%d", id_mesero);
    const char *parametros[] = { estacion, txt_mesero, estados };
    return bd_consultar(SQL_LISTAR_COMANDAS, 3, parametros, NULL);
}

// ---------------------------------------------------------------------------
//  OP_CAMBIAR_ESTADO_PEDIDO: { ids: [...], estado }
// ---------------------------------------------------------------------------

// A qué estado se puede llegar, desde cuáles y quién lo hace. tg_comanda_estado
// solo impide regresar; aquí además no se brincan pasos que no tienen sentido
// (de pendiente a entregada sin que cocina la termine). Cancelar es aparte.
static const struct {
    const char *estado;
    const char *desde; // arreglo de PostgreSQL
    const char *rol;
} TRANSICIONES[] = {
    { "en_preparacion", "{pendiente}", "bar_cocina" },
    { "lista", "{pendiente,en_preparacion}", "bar_cocina" },
    { "entregada", "{lista}", "mesero" },
};

static const char SQL_REVISAR_ESTADO[] =
    "SELECT CASE"
    "  WHEN count(*) <> cardinality($1::int[]) THEN 'Alguna de las comandas ya no existe'"
    "  WHEN bool_or(NOT co.estado = ANY ($2::text[]))"
    "  THEN 'No se puede pasar de «' || (array_agg(" SQL_NOMBRE_ESTADO("co.estado") ")"
    "                                     FILTER (WHERE NOT co.estado = ANY ($2::text[])))[1]"
    "       || '» a «' || " SQL_NOMBRE_ESTADO("$3::text") " || '»'"
    " END"
    "  FROM comanda co WHERE co.id_comanda = ANY ($1::int[])";

// Al entrar a preparación (o terminarse sin pasar por ahí) la comanda queda a
// nombre de quien la preparó. tg_comanda_estacion revisa que sea su estación, y
// tg_comanda_stock descuenta los ingredientes.
static const char SQL_CAMBIAR_ESTADO[] =
    "UPDATE comanda"
    "   SET estado = $2::text,"
    "       id_bar_cocina = CASE WHEN $2::text IN ('en_preparacion', 'lista')"
    "                            THEN COALESCE(id_bar_cocina, $3::integer) ELSE id_bar_cocina END"
    " WHERE id_comanda = ANY ($1::int[])";

// Un aviso por ronda (cuenta + envío) para el mesero que la levantó:
// { id_mesero, numero_mesa, cuenta, productos: ["2× Cerveza", ...] }
static const char SQL_ORDENES_LISTAS[] =
    "SELECT COALESCE(json_agg(json_build_object('id_mesero', id_mesero, 'numero_mesa', numero_mesa,"
    "                                            'cuenta', cuenta, 'productos', productos)), '[]')::text"
    "  FROM (SELECT id_mesero, numero_mesa, cuenta, json_agg(n || '× ' || producto ORDER BY producto) AS productos"
    "          FROM (SELECT co.id_mesero, c.numero_mesa, c.nombre || ' · ' || c.folio AS cuenta,"
    "                       co.fecha_hora, p.nombre AS producto, count(*) AS n"
    "                  FROM comanda co"
    "                  JOIN cuenta c ON c.numero_evento = co.numero_evento AND c.secuencial = co.secuencial"
    "                  JOIN producto p ON p.id_producto = co.id_producto"
    "                 WHERE co.id_comanda = ANY ($1::int[])"
    "                 GROUP BY co.id_mesero, c.numero_mesa, cuenta, co.fecha_hora, p.nombre) AS lineas"
    "         GROUP BY id_mesero, numero_mesa, cuenta, fecha_hora) AS rondas";

cJSON *op_cambiar_estado(Cliente *cliente, const cJSON *datos)
{
    const char *estado = dato_texto(datos, "estado");
    int t = -1;
    for (int i = 0; estado != NULL && i < (int)(sizeof TRANSICIONES / sizeof TRANSICIONES[0]); i++) {
        if (strcmp(estado, TRANSICIONES[i].estado) == 0)
            t = i;
    }
    if (t < 0)
        return respuesta_error("VALIDACION_FALLIDA", "Estado desconocido");
    cJSON *respuesta = exigir_rol(cliente, TRANSICIONES[t].rol);
    if (respuesta != NULL)
        return respuesta;

    // Con 200 ids la lista ocupa unos 2 KB: se pide aparte y no en la pila del hilo
    char *lista = malloc(TAM_IDS);
    if (lista == NULL)
        return respuesta_error("ERROR_BD", "Sin memoria");
    int total = dato_ids(datos, "ids", MAX_IDS, lista, TAM_IDS);
    if (total == 0) {
        free(lista);
        return respuesta_error("VALIDACION_FALLIDA", "No se indicó ninguna comanda");
    }

    PGconn *pg = bd_tomar();
    if (pg == NULL) {
        free(lista);
        return bd_respuesta_sin_conexion();
    }
    cJSON *ordenes = NULL;
    char id_usuario[16];
    snprintf(id_usuario, sizeof id_usuario, "%d", cliente->sesion.id_usuario);
    const char *p_revisar[] = { lista, TRANSICIONES[t].desde, estado };
    const char *p_cambiar[] = { lista, estado, id_usuario };
    const char *p_ordenes[] = { lista };

    PASO(bd_paso(pg, "BEGIN", 0, NULL, NULL));
    PASO(bd_revisar(pg, SQL_REVISAR_ESTADO, 3, p_revisar));
    PASO(bd_paso(pg, SQL_CAMBIAR_ESTADO, 3, p_cambiar, NULL));
    if (strcmp(estado, "lista") == 0)
        PASO(bd_paso(pg, SQL_ORDENES_LISTAS, 1, p_ordenes, &ordenes));
    PASO(bd_paso(pg, "COMMIT", 0, NULL, NULL));

    registrar("%s pasó %d comanda(s) a %s", cliente->sesion.usuario, total, estado);
    cJSON *resultado = cJSON_CreateObject();
    cJSON_AddNumberToObject(resultado, "actualizadas", total);
    respuesta = respuesta_ok(resultado);

    avisar_todos("comandas");
    avisar_todos("cuentas");
    if (strcmp(estado, "en_preparacion") == 0 || strcmp(estado, "lista") == 0)
        avisar_todos("inventario");
    const cJSON *orden;
    cJSON_ArrayForEach(orden, ordenes) {
        const cJSON *mesero = cJSON_GetObjectItemCaseSensitive(orden, "id_mesero");
        if (cJSON_IsNumber(mesero))
            avisar_usuario(mesero->valueint, "orden_lista", orden);
    }

fin:
    cJSON_Delete(ordenes);
    bd_soltar(pg);
    free(lista);
    return respuesta;
}

// ---------------------------------------------------------------------------
//  OP_ELIMINAR_PEDIDO: cancela comandas { ids: [...] } (no las borra)
// ---------------------------------------------------------------------------

// Como validar_cambio_estado, se cancela desde cualquier estado y una cancelada
// ya no cambia. Además no se toca una cuenta cerrada ni se deja la cuenta con
// saldo a favor, porque no hay forma de registrar devoluciones.
static const char SQL_REVISAR_CANCELAR[] =
    "WITH elegidas AS (SELECT * FROM comanda WHERE id_comanda = ANY ($1::int[]) FOR UPDATE)"
    "SELECT CASE"
    "  WHEN (SELECT count(*) FROM elegidas) <> cardinality($1::int[])"
    "  THEN 'Alguna de las comandas ya no existe'"
    "  WHEN EXISTS (SELECT 1 FROM elegidas WHERE estado = 'cancelada')"
    "  THEN 'La comanda ' || (SELECT min(id_comanda) FROM elegidas WHERE estado = 'cancelada')"
    "       || ' ya está cancelada'"
    "  WHEN EXISTS (SELECT 1 FROM elegidas e JOIN cuenta c"
    "                   ON c.numero_evento = e.numero_evento AND c.secuencial = e.secuencial"
    "                WHERE c.estado <> 'abierta')"
    "  THEN 'La cuenta está cerrada; sus comandas ya no se pueden cancelar'"
    "  ELSE (SELECT 'No se puede cancelar: el consumo que queda sería menor que lo ya pagado ('"
    "               || v.total_pagado || ')'"
    "          FROM (SELECT numero_evento, secuencial, sum(precio_cobrado) AS importe"
    "                  FROM elegidas GROUP BY numero_evento, secuencial) AS x"
    "          JOIN v_cuenta v ON v.numero_evento = x.numero_evento AND v.secuencial = x.secuencial"
    "         WHERE v.saldo < x.importe LIMIT 1)"
    " END";

static const char SQL_CANCELAR[] =
    "UPDATE comanda SET estado = 'cancelada', id_cajero_cancela = $2::integer"
    " WHERE id_comanda = ANY ($1::int[])"
    " RETURNING numero_evento, secuencial";

// El saldo que le queda a la cuenta de la primera comanda
static const char SQL_SALDO_COMANDA[] =
    "SELECT json_build_object('saldo', v.saldo)::text"
    "  FROM comanda co"
    "  JOIN v_cuenta v ON v.numero_evento = co.numero_evento AND v.secuencial = co.secuencial"
    " WHERE co.id_comanda = ($1::int[])[1]";

cJSON *op_cancelar_comandas(Cliente *cliente, const cJSON *datos)
{
    cJSON *respuesta = exigir_administrador(cliente);
    if (respuesta != NULL)
        return respuesta;
    char *lista = malloc(TAM_IDS);
    if (lista == NULL)
        return respuesta_error("ERROR_BD", "Sin memoria");
    int total = dato_ids(datos, "ids", MAX_IDS, lista, TAM_IDS);
    if (total == 0) {
        free(lista);
        return respuesta_error("VALIDACION_FALLIDA", "No se indicó ninguna comanda");
    }

    PGconn *pg = bd_tomar();
    if (pg == NULL) {
        free(lista);
        return bd_respuesta_sin_conexion();
    }
    cJSON *saldo = NULL;
    char admin[16];
    snprintf(admin, sizeof admin, "%d", cliente->sesion.id_usuario);
    const char *p_lista[] = { lista };
    const char *p_cancelar[] = { lista, admin };

    PASO(bd_paso(pg, "BEGIN", 0, NULL, NULL));
    PASO(bd_revisar(pg, SQL_REVISAR_CANCELAR, 1, p_lista));
    PASO(bd_paso(pg, SQL_CANCELAR, 2, p_cancelar, NULL));
    PASO(bd_paso(pg, SQL_SALDO_COMANDA, 1, p_lista, &saldo));
    PASO(bd_paso(pg, "COMMIT", 0, NULL, NULL));

    registrar("%s canceló %d comanda(s)", cliente->sesion.usuario, total);
    if (saldo == NULL)
        saldo = cJSON_CreateObject();
    cJSON_AddNumberToObject(saldo, "canceladas", total);
    respuesta = respuesta_ok(saldo);
    saldo = NULL;
    avisar_todos("comandas");
    avisar_todos("cuentas");
    avisar_todos("inventario");

fin:
    cJSON_Delete(saldo);
    bd_soltar(pg);
    free(lista);
    return respuesta;
}
