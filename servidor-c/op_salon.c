// Operaciones del salón y de la caja: evento, mesas, cuentas y pagos.
//
// Todo pasa en el evento en curso (SQL_EVENTO). Las reglas de dinero las
// revisa la base con sus triggers (tg_pago_saldo, tg_cuenta_cierre) y sus
// CHECK; aquí solo se revisa la forma de los datos y quién llama, y se dan
// mensajes claros para los casos que el usuario puede provocar.
#include "operaciones.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "avisos.h"
#include "bd.h"
#include "datos.h"
#include "registro.h"
#include "respuesta.h"

// Si un paso falla, deja su respuesta de error y salta al final
#define PASO(llamada)                                                                   \
    do {                                                                                \
        if ((respuesta = (llamada)) != NULL)                                            \
            goto fin;                                                                   \
    } while (0)

// Los campos con que una cuenta llega a las pantallas. "cuenta" es el nombre
// con el folio al final ("Novios · 17-1-1"): el nombre guía al mesero.
#define CUENTA_JSON(c)                                                                  \
    "'secuencial', " c ".secuencial, 'numero_mesa', " c ".numero_mesa,"                 \
    " 'nombre', " c ".nombre, 'folio', " c ".folio,"                                    \
    " 'cuenta', " c ".nombre || ' · ' || " c ".folio"

static cJSON *leer_secuencial(const cJSON *datos, char *salida, size_t tam)
{
    int secuencial;
    if (!dato_entero(datos, "secuencial", 1, SHRT_MAX, &secuencial))
        return respuesta_error("VALIDACION_FALLIDA", "Falta la cuenta o no es válida");
    snprintf(salida, tam, "%d", secuencial);
    return NULL;
}

// ---------------------------------------------------------------------------
//  OP_EVENTO_EN_CURSO (se puede pedir sin sesión: lo muestra el login)
// ---------------------------------------------------------------------------

static const char SQL_EVENTO_EN_CURSO[] =
    "SELECT json_build_object('numero_evento', e.numero_evento, 'fecha', e.fecha::text,"
    "       'tipo_evento', initcap(e.tipo_evento), 'numero_asistentes', e.numero_asistentes,"
    "       'estado', e.estado,"
    "       'organizadores', COALESCE((SELECT json_agg(o.organizador ORDER BY o.organizador)"
    "                                    FROM evento_organizador o"
    "                                   WHERE o.numero_evento = e.numero_evento), '[]'))::text"
    "  FROM evento e"
    " WHERE e.numero_evento = " SQL_EVENTO;

cJSON *op_evento_en_curso(void)
{
    return bd_consultar(SQL_EVENTO_EN_CURSO, 0, NULL, NULL);
}

// ---------------------------------------------------------------------------
//  OP_LISTAR_MESAS: las del mesero con sesión van primero
// ---------------------------------------------------------------------------

static const char SQL_LISTAR_MESAS[] =
    "SELECT COALESCE(json_agg(json_build_object('numero_mesa', m.numero_mesa,"
    "                                            'id_mesero', m.id_mesero, 'mesero', u.nombre,"
    "                                            'mia', m.id_mesero = $1::integer)"
    "                         ORDER BY m.id_mesero = $1::integer DESC, m.numero_mesa), '[]')::text"
    "  FROM mesa m"
    "  JOIN usuario u ON u.id_usuario = m.id_mesero"
    " WHERE m.numero_evento = " SQL_EVENTO;

cJSON *op_listar_mesas(Cliente *cliente)
{
    char id[16];
    snprintf(id, sizeof id, "%d", cliente->sesion.id_usuario);
    const char *parametros[] = { id };
    return bd_consultar(SQL_LISTAR_MESAS, 1, parametros, NULL);
}

// ---------------------------------------------------------------------------
//  OP_LISTAR_CUENTAS: las cuentas abiertas de una mesa
// ---------------------------------------------------------------------------

static const char SQL_LISTAR_CUENTAS[] =
    "SELECT COALESCE(json_agg(json_build_object(" CUENTA_JSON("v") ", 'total', v.total_acumulado)"
    "                         ORDER BY v.secuencial), '[]')::text"
    "  FROM v_cuenta v"
    " WHERE v.numero_evento = " SQL_EVENTO
    "   AND v.numero_mesa = $1::smallint AND v.estado = 'abierta'";

cJSON *op_listar_cuentas(const cJSON *datos)
{
    int numero_mesa;
    if (!dato_entero(datos, "numero_mesa", 1, SHRT_MAX, &numero_mesa))
        return respuesta_error("VALIDACION_FALLIDA", "Falta el número de mesa o no es válido");
    char mesa[8];
    snprintf(mesa, sizeof mesa, "%d", numero_mesa);
    const char *parametros[] = { mesa };
    return bd_consultar(SQL_LISTAR_CUENTAS, 1, parametros, NULL);
}

// ---------------------------------------------------------------------------
//  OP_CREAR_CUENTA: abre una cuenta con nombre en una mesa
// ---------------------------------------------------------------------------

// El secuencial no es IDENTITY: es el siguiente dentro del evento. Se bloquea
// la fila del evento para que dos meseros a la vez no saquen el mismo número.
static const char SQL_BLOQUEAR_EVENTO[] =
    "SELECT numero_evento FROM evento WHERE numero_evento = " SQL_EVENTO " FOR UPDATE";

// Como ux_cuenta_nombre_abierta, pero con un mensaje claro
static const char SQL_REVISAR_CUENTA_NUEVA[] =
    "SELECT CASE"
    "  WHEN NOT EXISTS (SELECT 1 FROM mesa"
    "                    WHERE numero_evento = $1::integer AND numero_mesa = $2::smallint)"
    "  THEN 'La mesa ' || $2::smallint || ' no existe en este evento'"
    "  WHEN EXISTS (SELECT 1 FROM cuenta"
    "                WHERE numero_evento = $1::integer AND numero_mesa = $2::smallint"
    "                  AND estado = 'abierta' AND lower(nombre) = lower($3::text))"
    "  THEN 'Ya hay una cuenta abierta llamada «' || $3::text || '» en la mesa ' || $2::smallint"
    " END";

static const char SQL_CREAR_CUENTA[] =
    "INSERT INTO cuenta (numero_evento, secuencial, numero_mesa, nombre)"
    " SELECT $1::integer, COALESCE(MAX(secuencial), 0) + 1, $2::smallint, $3::text"
    "   FROM cuenta WHERE numero_evento = $1::integer"
    " RETURNING json_build_object(" CUENTA_JSON("cuenta") ", 'total', 0)::text";

cJSON *op_crear_cuenta(Cliente *cliente, const cJSON *datos)
{
    if (strcmp(cliente->sesion.rol, "mesero") != 0 && strcmp(cliente->sesion.rol, "cajero_admin") != 0)
        return respuesta_error("NO_AUTORIZADO", "Tu rol no puede hacer esta operación");

    int numero_mesa;
    if (!dato_entero(datos, "numero_mesa", 1, SHRT_MAX, &numero_mesa))
        return respuesta_error("VALIDACION_FALLIDA", "Falta el número de mesa o no es válido");
    const char *recibido = dato_texto(datos, "nombre");
    char nombre[256];
    if (recibido == NULL || !recortar(recibido, nombre, sizeof nombre) || nombre[0] == '\0')
        return respuesta_error("VALIDACION_FALLIDA", "Escribe un nombre para la cuenta");
    if (caracteres_utf8(nombre) > 60)
        return respuesta_error("VALIDACION_FALLIDA", "El nombre de la cuenta admite hasta 60 caracteres");

    PGconn *pg = bd_tomar();
    if (pg == NULL)
        return bd_respuesta_sin_conexion();
    cJSON *respuesta = NULL, *evento = NULL, *cuenta = NULL;
    char txt_evento[16], txt_mesa[8];

    PASO(bd_paso(pg, "BEGIN", 0, NULL, NULL));
    PASO(bd_paso(pg, SQL_BLOQUEAR_EVENTO, 0, NULL, &evento));
    if (!cJSON_IsNumber(evento)) {
        respuesta = respuesta_error("VALIDACION_FALLIDA", "No hay ningún evento en curso");
        goto fin;
    }
    snprintf(txt_evento, sizeof txt_evento, "%d", evento->valueint);
    snprintf(txt_mesa, sizeof txt_mesa, "%d", numero_mesa);
    const char *parametros[] = { txt_evento, txt_mesa, nombre };
    PASO(bd_revisar(pg, SQL_REVISAR_CUENTA_NUEVA, 3, parametros));
    PASO(bd_paso(pg, SQL_CREAR_CUENTA, 3, parametros, &cuenta));
    PASO(bd_paso(pg, "COMMIT", 0, NULL, NULL));

    registrar("%s abrió la cuenta %s en la mesa %d", cliente->sesion.usuario, nombre, numero_mesa);
    respuesta = respuesta_ok(cuenta);
    cuenta = NULL;
    avisar_todos("cuentas");

fin:
    cJSON_Delete(evento);
    cJSON_Delete(cuenta);
    bd_soltar(pg);
    return respuesta;
}

// ---------------------------------------------------------------------------
//  OP_LISTAR_CUENTAS_CAJA: todas las cuentas del evento con su dinero (v_cuenta)
// ---------------------------------------------------------------------------

// ck_cuenta_estado solo admite abierta y cerrada: una cuenta "cancelada" es una
// cerrada sin pagos y sin ninguna comanda viva.
static const char SQL_LISTAR_CUENTAS_CAJA[] =
    "SELECT COALESCE(json_agg(json_build_object(" CUENTA_JSON("v") ", 'estado', v.estado,"
    "         'es_anfitrion', EXISTS (SELECT 1 FROM evento ev WHERE ev.numero_evento = v.numero_evento"
    "                                                         AND ev.secuencial_anfitrion = v.secuencial),"
    "         'cancelada', v.estado = 'cerrada'"
    "            AND NOT EXISTS (SELECT 1 FROM pago pa"
    "                             WHERE pa.numero_evento = v.numero_evento AND pa.secuencial = v.secuencial)"
    "            AND NOT EXISTS (SELECT 1 FROM comanda co"
    "                             WHERE co.numero_evento = v.numero_evento AND co.secuencial = v.secuencial"
    "                               AND co.estado <> 'cancelada'),"
    "         'id_mesero', m.id_mesero, 'mesero', u.nombre,"
    "         'fecha_hora', COALESCE((SELECT " SQL_MS("max(co.fecha_hora)") " FROM comanda co"
    "                                  WHERE co.numero_evento = v.numero_evento"
    "                                    AND co.secuencial = v.secuencial), 0),"
    "         'total', v.total_acumulado, 'pagado', v.total_pagado, 'saldo', v.saldo,"
    "         'por_terminar', (SELECT count(*) FROM comanda co"
    "                           WHERE co.numero_evento = v.numero_evento AND co.secuencial = v.secuencial"
    "                             AND co.estado NOT IN ('entregada', 'cancelada')))"
    "       ORDER BY v.secuencial), '[]')::text"
    "  FROM v_cuenta v"
    "  JOIN mesa m ON m.numero_evento = v.numero_evento AND m.numero_mesa = v.numero_mesa"
    "  JOIN usuario u ON u.id_usuario = m.id_mesero"
    " WHERE v.numero_evento = COALESCE($1::integer, " SQL_EVENTO ")";

// Las cuentas de otro evento (uno pasado, por ejemplo) solo las consulta el
// administrador, desde la ficha del evento. Sin numero_evento, el en curso.
static cJSON *leer_evento_opcional(const Cliente *cliente, const cJSON *datos, char *salida, size_t tam,
                                   const char **parametro)
{
    *parametro = NULL;
    const cJSON *campo = cJSON_GetObjectItemCaseSensitive(datos, "numero_evento");
    if (campo == NULL || cJSON_IsNull(campo))
        return NULL;
    int numero;
    if (!dato_entero(datos, "numero_evento", 1, 2147483647, &numero))
        return respuesta_error("VALIDACION_FALLIDA", "El evento no es válido");
    cJSON *negado = exigir_administrador(cliente);
    if (negado != NULL)
        return negado;
    snprintf(salida, tam, "%d", numero);
    *parametro = salida;
    return NULL;
}

cJSON *op_listar_cuentas_caja(Cliente *cliente, const cJSON *datos)
{
    cJSON *respuesta = exigir_rol(cliente, "cajero_admin");
    char evento[16];
    const char *p_evento;
    if (respuesta == NULL)
        respuesta = leer_evento_opcional(cliente, datos, evento, sizeof evento, &p_evento);
    if (respuesta != NULL)
        return respuesta;
    const char *parametros[] = { p_evento };
    return bd_consultar(SQL_LISTAR_CUENTAS_CAJA, 1, parametros, NULL);
}

// ---------------------------------------------------------------------------
//  OP_DETALLE_CUENTA: comandas y pagos de una cuenta (el cliente las agrupa)
// ---------------------------------------------------------------------------

static const char SQL_DETALLE_CUENTA[] =
    "SELECT json_build_object('secuencial', c.secuencial,"
    "         'es_anfitrion', e.secuencial_anfitrion IS NOT DISTINCT FROM c.secuencial,"
    // Sus comandas, y si es la cuenta del anfitrión, también los excedentes
    // que se le cargaron desde otras cuentas (migración 003)
    "         'comandas', COALESCE((SELECT json_agg(json_build_object("
    "                                  'id_comanda', co.id_comanda, 'id_producto', co.id_producto,"
    "                                  'producto', p.nombre, 'detalles', COALESCE(co.detalles, ''),"
    "                                  'estado', co.estado, 'precio', co.precio_cobrado,"
    "                                  'tipo_cobro', co.tipo_cobro,"
    "                                  'de_otra_cuenta', CASE WHEN co.secuencial <> c.secuencial"
    "                                                         THEN o.nombre || ' · ' || o.folio END)"
    "                                ORDER BY co.id_comanda)"
    "                                 FROM comanda co"
    "                                 JOIN producto p ON p.id_producto = co.id_producto"
    "                                 JOIN cuenta o ON o.numero_evento = co.numero_evento AND o.secuencial = co.secuencial"
    "                                WHERE co.numero_evento = c.numero_evento"
    "                                  AND (co.secuencial = c.secuencial"
    "                                       OR (co.tipo_cobro = 'anfitrion'"
    "                                           AND e.secuencial_anfitrion = c.secuencial))), '[]'),"
    "         'pagos', COALESCE((SELECT json_agg(json_build_object("
    "                               'numero_pago', pa.numero_pago, 'monto', pa.monto,"
    "                               'forma_pago', pa.forma_pago, 'monto_propina', pa.monto_propina,"
    "                               'fecha_hora', " SQL_MS("pa.fecha_hora") ","
    "                               'mesero_propina', COALESCE(u.nombre, ''))"
    "                             ORDER BY pa.numero_pago)"
    "                              FROM pago pa LEFT JOIN usuario u ON u.id_usuario = pa.id_mesero_propina"
    "                             WHERE pa.numero_evento = c.numero_evento"
    "                               AND pa.secuencial = c.secuencial), '[]'))::text"
    "  FROM cuenta c JOIN evento e ON e.numero_evento = c.numero_evento"
    " WHERE c.numero_evento = COALESCE($2::integer, " SQL_EVENTO ") AND c.secuencial = $1::smallint";

cJSON *op_detalle_cuenta(Cliente *cliente, const cJSON *datos)
{
    cJSON *respuesta = exigir_rol(cliente, "cajero_admin");
    char secuencial[8], evento[16];
    const char *p_evento = NULL;
    if (respuesta == NULL)
        respuesta = leer_secuencial(datos, secuencial, sizeof secuencial);
    if (respuesta == NULL)
        respuesta = leer_evento_opcional(cliente, datos, evento, sizeof evento, &p_evento);
    if (respuesta != NULL)
        return respuesta;
    const char *parametros[] = { secuencial, p_evento };
    return bd_consultar(SQL_DETALLE_CUENTA, 2, parametros, "La cuenta ya no existe");
}

// ---------------------------------------------------------------------------
//  OP_REGISTRAR_PAGO
// ---------------------------------------------------------------------------

static const char SQL_REVISAR_PAGO[] =
    "SELECT CASE WHEN c.estado IS NULL THEN 'La cuenta ya no existe'"
    "            WHEN c.estado <> 'abierta' THEN 'La cuenta está cerrada; ya no recibe pagos' END"
    "  FROM (SELECT 1) AS uno"
    "  LEFT JOIN cuenta c ON c.numero_evento = " SQL_EVENTO " AND c.secuencial = $1::smallint";

// La propina es para el mesero que atiende la mesa de la cuenta. Que el pago
// no pase del saldo lo revisa tg_pago_saldo.
static const char SQL_REGISTRAR_PAGO[] =
    "INSERT INTO pago (monto, forma_pago, numero_evento, secuencial, id_cajero,"
    "                  id_mesero_propina, monto_propina)"
    " SELECT $2::numeric, $3::text, c.numero_evento, c.secuencial, $4::integer,"
    "        CASE WHEN $5::numeric > 0 THEN m.id_mesero END, NULLIF($5::numeric, 0)"
    "   FROM cuenta c"
    "   JOIN mesa m ON m.numero_evento = c.numero_evento AND m.numero_mesa = c.numero_mesa"
    "  WHERE c.numero_evento = " SQL_EVENTO " AND c.secuencial = $1::smallint";

static const char SQL_SALDO[] =
    "SELECT json_build_object('saldo', saldo)::text FROM v_cuenta"
    " WHERE numero_evento = " SQL_EVENTO " AND secuencial = $1::smallint";

cJSON *op_registrar_pago(Cliente *cliente, const cJSON *datos)
{
    cJSON *respuesta = exigir_rol(cliente, "cajero_admin");
    char secuencial[8];
    if (respuesta == NULL)
        respuesta = leer_secuencial(datos, secuencial, sizeof secuencial);
    if (respuesta != NULL)
        return respuesta;

    char monto[32], propina[32] = "0";
    if (!dato_numero(datos, "monto", 99999999.99, 2, monto, sizeof monto) || strtod(monto, NULL) <= 0)
        return respuesta_error("VALIDACION_FALLIDA", "El monto debe ser mayor que 0");
    const cJSON *campo_propina = cJSON_GetObjectItemCaseSensitive(datos, "monto_propina");
    if (campo_propina != NULL && !cJSON_IsNull(campo_propina)
        && !dato_numero(datos, "monto_propina", 99999999.99, 2, propina, sizeof propina))
        return respuesta_error("VALIDACION_FALLIDA", "La propina no puede ser negativa");
    if (strtod(propina, NULL) > strtod(monto, NULL))
        return respuesta_error("VALIDACION_FALLIDA", "La propina no puede ser mayor que el monto del pago");
    const char *forma = dato_texto(datos, "forma_pago");
    if (forma == NULL
        || (strcmp(forma, "efectivo") != 0 && strcmp(forma, "tarjeta") != 0 && strcmp(forma, "transferencia") != 0))
        return respuesta_error("VALIDACION_FALLIDA", "Elige la forma de pago");

    PGconn *pg = bd_tomar();
    if (pg == NULL)
        return bd_respuesta_sin_conexion();
    cJSON *saldo = NULL;
    char cajero[16];
    snprintf(cajero, sizeof cajero, "%d", cliente->sesion.id_usuario);
    const char *p_cuenta[] = { secuencial };
    const char *p_pago[] = { secuencial, monto, forma, cajero, propina };

    PASO(bd_paso(pg, "BEGIN", 0, NULL, NULL));
    PASO(bd_revisar(pg, SQL_REVISAR_PAGO, 1, p_cuenta));
    PASO(bd_paso(pg, SQL_REGISTRAR_PAGO, 5, p_pago, NULL));
    PASO(bd_paso(pg, SQL_SALDO, 1, p_cuenta, &saldo));
    PASO(bd_paso(pg, "COMMIT", 0, NULL, NULL));

    registrar("%s registró un pago de %s en la cuenta %s", cliente->sesion.usuario, monto, secuencial);
    respuesta = respuesta_ok(saldo);
    saldo = NULL;
    avisar_todos("cuentas");

fin:
    cJSON_Delete(saldo);
    bd_soltar(pg);
    return respuesta;
}

// ---------------------------------------------------------------------------
//  OP_CERRAR_CUENTA y OP_CANCELAR_CUENTA
// ---------------------------------------------------------------------------

// tg_cuenta_cierre revisa que no quede saldo ni comandas sin terminar
static const char SQL_CERRAR_CUENTA[] =
    "UPDATE cuenta SET estado = 'cerrada'"
    " WHERE numero_evento = " SQL_EVENTO " AND secuencial = $1::smallint AND estado = 'abierta'"
    " RETURNING json_build_object('cuenta', nombre || ' · ' || folio)::text";

cJSON *op_cerrar_cuenta(Cliente *cliente, const cJSON *datos)
{
    cJSON *respuesta = exigir_rol(cliente, "cajero_admin");
    char secuencial[8];
    if (respuesta == NULL)
        respuesta = leer_secuencial(datos, secuencial, sizeof secuencial);
    if (respuesta != NULL)
        return respuesta;
    const char *parametros[] = { secuencial };
    respuesta = bd_consultar(SQL_CERRAR_CUENTA, 1, parametros, "La cuenta ya está cerrada");
    if (cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(respuesta, "ok"))) {
        registrar("%s cerró la cuenta %s", cliente->sesion.usuario, secuencial);
        avisar_todos("cuentas");
    }
    return respuesta;
}

// Sin tocar ck_cuenta_estado: cancela todas sus comandas y la cierra, lo que
// tg_cuenta_cierre acepta. Con pagos no se puede, porque quedaría saldo a favor.
static const char SQL_REVISAR_CANCELAR_CUENTA[] =
    "SELECT CASE WHEN c.estado IS NULL OR c.estado <> 'abierta' THEN 'La cuenta ya está cerrada'"
    "            WHEN EXISTS (SELECT 1 FROM pago pa"
    "                          WHERE pa.numero_evento = c.numero_evento AND pa.secuencial = c.secuencial)"
    "            THEN 'La cuenta ya tiene pagos registrados; no se puede cancelar' END"
    "  FROM (SELECT 1) AS uno"
    "  LEFT JOIN cuenta c ON c.numero_evento = " SQL_EVENTO " AND c.secuencial = $1::smallint";

static const char SQL_CANCELAR_COMANDAS_CUENTA[] =
    "UPDATE comanda SET estado = 'cancelada', id_cajero_cancela = $2::integer"
    " WHERE numero_evento = " SQL_EVENTO " AND secuencial = $1::smallint AND estado <> 'cancelada'";

cJSON *op_cancelar_cuenta(Cliente *cliente, const cJSON *datos)
{
    cJSON *respuesta = exigir_administrador(cliente);
    char secuencial[8];
    if (respuesta == NULL)
        respuesta = leer_secuencial(datos, secuencial, sizeof secuencial);
    if (respuesta != NULL)
        return respuesta;

    PGconn *pg = bd_tomar();
    if (pg == NULL)
        return bd_respuesta_sin_conexion();
    cJSON *cuenta = NULL;
    char admin[16];
    snprintf(admin, sizeof admin, "%d", cliente->sesion.id_usuario);
    const char *p_cuenta[] = { secuencial };
    const char *p_comandas[] = { secuencial, admin };

    PASO(bd_paso(pg, "BEGIN", 0, NULL, NULL));
    PASO(bd_revisar(pg, SQL_REVISAR_CANCELAR_CUENTA, 1, p_cuenta));
    PASO(bd_paso(pg, SQL_CANCELAR_COMANDAS_CUENTA, 2, p_comandas, NULL));
    PASO(bd_paso(pg, SQL_CERRAR_CUENTA, 1, p_cuenta, &cuenta));
    PASO(bd_paso(pg, "COMMIT", 0, NULL, NULL));

    registrar("%s canceló la cuenta %s", cliente->sesion.usuario, secuencial);
    respuesta = respuesta_ok(cuenta);
    cuenta = NULL;
    avisar_todos("cuentas");
    avisar_todos("comandas");

fin:
    cJSON_Delete(cuenta);
    bd_soltar(pg);
    return respuesta;
}
