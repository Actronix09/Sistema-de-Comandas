// Operaciones de eventos: el calendario, alta (con repetición), edición con
// sus organizadores, mesas y paquetes acordados, cambios de estado y anticipos.
// Solo las hace un administrador.
//
// Un evento no se borra (la app no tiene DELETE sobre evento): se cancela. Sus
// mesas tampoco se borran, igual que las cuentas: solo se agregan o cambian de
// mesero. El evento "en curso" es con el que trabaja todo el salón (SQL_EVENTO).
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

#define MAX_ORGANIZADORES 10
#define MAX_MESAS 200
#define MAX_ACUERDOS 500
#define MAX_REPETICIONES 52

// ---------------------------------------------------------------------------
//  OP_LISTAR_EVENTOS: { desde, hasta } opcionales ("AAAA-MM-DD")
// ---------------------------------------------------------------------------

// Cada evento con todo lo que muestra su ficha. "paquetes" agrupa los
// acuerdos por paquete, con la cantidad acordada de cada producto.
static const char SQL_LISTAR_EVENTOS[] =
    "SELECT COALESCE(json_agg(json_build_object("
    "  'numero_evento', e.numero_evento, 'fecha', e.fecha::text, 'tipo_evento', e.tipo_evento,"
    "  'numero_asistentes', e.numero_asistentes, 'estado', e.estado,"
    "  'organizadores', COALESCE((SELECT json_agg(o.organizador ORDER BY o.organizador)"
    "                               FROM evento_organizador o WHERE o.numero_evento = e.numero_evento), '[]'),"
    "  'mesas', COALESCE((SELECT json_agg(json_build_object('numero_mesa', m.numero_mesa,"
    "                              'id_mesero', m.id_mesero, 'mesero', u.nombre,"
    "                              'cuentas', (SELECT count(*) FROM cuenta c WHERE c.numero_evento = m.numero_evento"
    "                                                                          AND c.numero_mesa = m.numero_mesa))"
    "                            ORDER BY m.numero_mesa)"
    "                       FROM mesa m JOIN usuario u ON u.id_usuario = m.id_mesero"
    "                      WHERE m.numero_evento = e.numero_evento), '[]'),"
    "  'paquetes', COALESCE((SELECT json_agg(json_build_object('numero_paquete', pq.numero_paquete,"
    "                                 'detalles', pq.detalles, 'monto', pq.monto,"
    "                                 'productos', (SELECT json_agg(json_build_object('id_producto', a.id_producto,"
    "                                                                 'nombre', p.nombre,"
    "                                                                 'cantidad', a.cantidad_acordada,"
    "                                                                 'modo_cobro', a.modo_cobro)"
    "                                                               ORDER BY p.nombre)"
    "                                                 FROM acuerdo a JOIN producto p ON p.id_producto = a.id_producto"
    "                                                WHERE a.numero_evento = e.numero_evento"
    "                                                  AND a.numero_paquete = pq.numero_paquete))"
    "                               ORDER BY pq.numero_paquete)"
    "                          FROM paquete pq"
    "                         WHERE EXISTS (SELECT 1 FROM acuerdo a WHERE a.numero_evento = e.numero_evento"
    "                                                                 AND a.numero_paquete = pq.numero_paquete)), '[]'),"
    "  'anticipos', COALESCE((SELECT json_agg(json_build_object('id_anticipo', an.id_anticipo, 'monto', an.monto,"
    "                                                            'fecha', an.fecha::text)"
    "                                          ORDER BY an.fecha, an.id_anticipo)"
    "                           FROM anticipo an WHERE an.numero_evento = e.numero_evento), '[]'),"
    "  'total_anticipos', COALESCE((SELECT sum(an.monto) FROM anticipo an WHERE an.numero_evento = e.numero_evento), 0),"
    "  'total_paquetes', COALESCE((SELECT sum(pq.monto) FROM paquete pq"
    "                               WHERE EXISTS (SELECT 1 FROM acuerdo a WHERE a.numero_evento = e.numero_evento"
    "                                                                       AND a.numero_paquete = pq.numero_paquete)), 0),"
    "  'cuentas_abiertas', (SELECT count(*) FROM cuenta c WHERE c.numero_evento = e.numero_evento"
    "                                                       AND c.estado = 'abierta'),"
    "  'secuencial_anfitrion', e.secuencial_anfitrion,"
    "  'cuentas', COALESCE((SELECT json_agg(json_build_object('secuencial', c.secuencial, 'estado', c.estado,"
    "                                 'cuenta', c.nombre || ' · ' || c.folio) ORDER BY c.secuencial)"
    "                        FROM cuenta c WHERE c.numero_evento = e.numero_evento), '[]'))"
    "  ORDER BY e.fecha, e.numero_evento), '[]')::text"
    "  FROM evento e"
    " WHERE ($1::date IS NULL OR e.fecha >= $1::date) AND ($2::date IS NULL OR e.fecha <= $2::date)";

cJSON *op_listar_eventos(Cliente *cliente, const cJSON *datos)
{
    cJSON *respuesta = exigir_administrador(cliente);
    if (respuesta != NULL)
        return respuesta;
    const char *desde = dato_texto(datos, "desde");
    const char *hasta = dato_texto(datos, "hasta");
    if ((desde != NULL && !fecha_iso(desde)) || (hasta != NULL && !fecha_iso(hasta)))
        return respuesta_error("VALIDACION_FALLIDA", "Las fechas van como AAAA-MM-DD");
    const char *parametros[] = { desde, hasta };
    return bd_consultar(SQL_LISTAR_EVENTOS, 2, parametros, NULL);
}

// ---------------------------------------------------------------------------
//  Lectura del evento que manda el cliente
//  datos: { numero_evento (al modificar), fecha, tipo_evento, numero_asistentes,
//           organizadores: ["Ana Torres", ...],
//           mesas: [{ numero_mesa, id_mesero }],
//           paquetes: [{ numero_paquete, productos: [{ id_producto, cantidad, modo_cobro }] }],
//           secuencial_anfitrion (solo al modificar; null = ninguna),
//           repetir (solo al crear): { cada: "dias" | "semanas" | "meses", intervalo, veces } }
// ---------------------------------------------------------------------------

typedef struct {
    char fecha[16];
    char tipo[256];
    char asistentes[16];
    char *organizadores; // JSON: ["Ana Torres", ...]
    char *mesas;         // JSON: [{numero_mesa, id_mesero}]
    char *acuerdos;      // JSON: [{numero_paquete, id_producto, cantidad}]
} Evento;

static void liberar_evento(Evento *e)
{
    cJSON_free(e->organizadores);
    cJSON_free(e->mesas);
    cJSON_free(e->acuerdos);
}

static cJSON *leer_organizadores(const cJSON *lista, cJSON *salida)
{
    if (lista != NULL && !cJSON_IsNull(lista) && !cJSON_IsArray(lista))
        return respuesta_error("VALIDACION_FALLIDA", "Los organizadores deben ser una lista");
    if (cJSON_GetArraySize(lista) > MAX_ORGANIZADORES)
        return respuesta_error("VALIDACION_FALLIDA", "Un evento admite hasta 10 organizadores");
    const cJSON *nombre;
    cJSON_ArrayForEach(nombre, lista) {
        char limpio[512];
        if (!cJSON_IsString(nombre) || !recortar(nombre->valuestring, limpio, sizeof limpio) || limpio[0] == '\0'
            || caracteres_utf8(limpio) > 120)
            return respuesta_error("VALIDACION_FALLIDA", "Cada organizador lleva un nombre de hasta 120 caracteres");
        // La clave de evento_organizador es (evento, organizador): sin repetidos
        int repetido = 0;
        const cJSON *previo;
        cJSON_ArrayForEach(previo, salida) {
            repetido |= strcmp(previo->valuestring, limpio) == 0;
        }
        if (!repetido)
            cJSON_AddItemToArray(salida, cJSON_CreateString(limpio));
    }
    return NULL;
}

static cJSON *leer_mesas(const cJSON *lista, cJSON *salida)
{
    if (lista != NULL && !cJSON_IsNull(lista) && !cJSON_IsArray(lista))
        return respuesta_error("VALIDACION_FALLIDA", "Las mesas deben ser una lista");
    if (cJSON_GetArraySize(lista) > MAX_MESAS)
        return respuesta_error("VALIDACION_FALLIDA", "Un evento admite hasta 200 mesas");
    const cJSON *mesa;
    char mensaje[96];
    cJSON_ArrayForEach(mesa, lista) {
        int numero, mesero;
        if (!dato_entero(mesa, "numero_mesa", 1, 32767, &numero))
            return respuesta_error("VALIDACION_FALLIDA", "El número de mesa debe ser un entero mayor que 0");
        if (!dato_entero(mesa, "id_mesero", 1, 2147483647, &mesero)) {
            snprintf(mensaje, sizeof mensaje, "Elige el mesero de la mesa %d", numero);
            return respuesta_error("VALIDACION_FALLIDA", mensaje);
        }
        const cJSON *previa;
        cJSON_ArrayForEach(previa, salida) {
            if (cJSON_GetObjectItemCaseSensitive(previa, "numero_mesa")->valueint == numero) {
                snprintf(mensaje, sizeof mensaje, "La mesa %d aparece dos veces", numero);
                return respuesta_error("VALIDACION_FALLIDA", mensaje);
            }
        }
        cJSON *limpia = cJSON_CreateObject();
        cJSON_AddNumberToObject(limpia, "numero_mesa", numero);
        cJSON_AddNumberToObject(limpia, "id_mesero", mesero);
        cJSON_AddItemToArray(salida, limpia);
    }
    return NULL;
}

// Los paquetes se aplanan a filas de acuerdo: (paquete, producto, cantidad)
static cJSON *leer_paquetes(const cJSON *lista, cJSON *salida)
{
    if (lista != NULL && !cJSON_IsNull(lista) && !cJSON_IsArray(lista))
        return respuesta_error("VALIDACION_FALLIDA", "Los paquetes deben ser una lista");
    const cJSON *paquete;
    cJSON_ArrayForEach(paquete, lista) {
        int numero;
        if (!dato_entero(paquete, "numero_paquete", 1, 2147483647, &numero))
            return respuesta_error("VALIDACION_FALLIDA", "Uno de los paquetes no es válido");
        const cJSON *productos = cJSON_GetObjectItemCaseSensitive(paquete, "productos");
        if (!cJSON_IsArray(productos) || cJSON_GetArraySize(productos) == 0)
            return respuesta_error("VALIDACION_FALLIDA", "Cada paquete del evento necesita al menos un producto");
        const cJSON *producto;
        cJSON_ArrayForEach(producto, productos) {
            int id, cantidad;
            if (!dato_entero(producto, "id_producto", 1, 2147483647, &id))
                return respuesta_error("VALIDACION_FALLIDA", "Uno de los productos del paquete no es válido");
            if (!dato_entero(producto, "cantidad", 1, 1000000, &cantidad))
                return respuesta_error("VALIDACION_FALLIDA", "La cantidad acordada debe ser un entero mayor que 0");
            // Qué pasa al pedirlo: ver sql/migraciones/003-cobro-paquetes.sql
            const char *modo = dato_texto(producto, "modo_cobro");
            if (modo == NULL)
                modo = "cobrar";
            if (strcmp(modo, "aparte") != 0 && strcmp(modo, "cobrar") != 0 && strcmp(modo, "bloquear") != 0
                && strcmp(modo, "anfitrion") != 0)
                return respuesta_error("VALIDACION_FALLIDA", "El modo de cobro debe ser aparte, cobrar, bloquear o anfitrion");
            if (cJSON_GetArraySize(salida) >= MAX_ACUERDOS)
                return respuesta_error("VALIDACION_FALLIDA", "El evento tiene demasiados productos acordados");
            const cJSON *previo;
            cJSON_ArrayForEach(previo, salida) {
                if (cJSON_GetObjectItemCaseSensitive(previo, "numero_paquete")->valueint == numero
                    && cJSON_GetObjectItemCaseSensitive(previo, "id_producto")->valueint == id)
                    return respuesta_error("VALIDACION_FALLIDA", "Un producto aparece dos veces en el mismo paquete");
            }
            cJSON *fila = cJSON_CreateObject();
            cJSON_AddNumberToObject(fila, "numero_paquete", numero);
            cJSON_AddNumberToObject(fila, "id_producto", id);
            cJSON_AddNumberToObject(fila, "cantidad", cantidad);
            cJSON_AddStringToObject(fila, "modo_cobro", modo);
            cJSON_AddItemToArray(salida, fila);
        }
    }
    return NULL;
}

static cJSON *leer_evento(const cJSON *datos, Evento *e)
{
    memset(e, 0, sizeof *e);
    const char *fecha = dato_texto(datos, "fecha");
    if (fecha == NULL || !recortar(fecha, e->fecha, sizeof e->fecha) || !fecha_iso(e->fecha)
        || strcmp(e->fecha, "2000-01-01") < 0 || strcmp(e->fecha, "2100-12-31") > 0)
        return respuesta_error("VALIDACION_FALLIDA", "Escribe la fecha del evento como AAAA-MM-DD");
    const char *tipo = dato_texto(datos, "tipo_evento");
    if (tipo == NULL || !recortar(tipo, e->tipo, sizeof e->tipo) || e->tipo[0] == '\0' || caracteres_utf8(e->tipo) > 40)
        return respuesta_error("VALIDACION_FALLIDA", "El tipo de evento es obligatorio y admite hasta 40 caracteres");
    int asistentes;
    if (!dato_entero(datos, "numero_asistentes", 1, 100000, &asistentes))
        return respuesta_error("VALIDACION_FALLIDA", "El número de asistentes debe ser un entero mayor que 0");
    snprintf(e->asistentes, sizeof e->asistentes, "%d", asistentes);

    cJSON *organizadores = cJSON_CreateArray(), *mesas = cJSON_CreateArray(), *acuerdos = cJSON_CreateArray();
    cJSON *respuesta = leer_organizadores(cJSON_GetObjectItemCaseSensitive(datos, "organizadores"), organizadores);
    if (respuesta == NULL)
        respuesta = leer_mesas(cJSON_GetObjectItemCaseSensitive(datos, "mesas"), mesas);
    if (respuesta == NULL)
        respuesta = leer_paquetes(cJSON_GetObjectItemCaseSensitive(datos, "paquetes"), acuerdos);
    if (respuesta == NULL) {
        e->organizadores = cJSON_PrintUnformatted(organizadores);
        e->mesas = cJSON_PrintUnformatted(mesas);
        e->acuerdos = cJSON_PrintUnformatted(acuerdos);
    }
    cJSON_Delete(organizadores);
    cJSON_Delete(mesas);
    cJSON_Delete(acuerdos);
    return respuesta;
}

// Lo que no revisa una restricción con un mensaje claro: que los meseros,
// paquetes y productos existan. $1 mesas, $2 acuerdos.
static const char SQL_REVISAR_DETALLE[] =
    "SELECT COALESCE("
    "  (SELECT 'El mesero de la mesa ' || m.numero_mesa || ' ya no existe o no es mesero'"
    "     FROM json_to_recordset($1::json) AS m(numero_mesa integer, id_mesero integer)"
    "    WHERE NOT EXISTS (SELECT 1 FROM mesero x WHERE x.id_usuario = m.id_mesero) LIMIT 1),"
    "  (SELECT 'Uno de los paquetes ya no existe'"
    "     FROM json_to_recordset($2::json) AS a(numero_paquete integer, id_producto integer, cantidad integer)"
    "    WHERE NOT EXISTS (SELECT 1 FROM paquete x WHERE x.numero_paquete = a.numero_paquete) LIMIT 1),"
    "  (SELECT 'Uno de los productos de un paquete ya no existe'"
    "     FROM json_to_recordset($2::json) AS a(numero_paquete integer, id_producto integer, cantidad integer)"
    "    WHERE NOT EXISTS (SELECT 1 FROM producto x WHERE x.id_producto = a.id_producto) LIMIT 1))";

// Organizadores, mesas y acuerdos para una lista de eventos ($1 = "{17,18}").
// Al modificar, los organizadores y acuerdos se reemplazan; las mesas se
// agregan o cambian de mesero, porque no se borran.
static const char SQL_BORRAR_ORGANIZADORES[] = "DELETE FROM evento_organizador WHERE numero_evento = ANY ($1::int[])";
static const char SQL_BORRAR_ACUERDOS[] = "DELETE FROM acuerdo WHERE numero_evento = ANY ($1::int[])";
static const char SQL_GUARDAR_ORGANIZADORES[] =
    "INSERT INTO evento_organizador (numero_evento, organizador)"
    " SELECT e, o FROM unnest($1::int[]) AS e, json_array_elements_text($2::json) AS o";
static const char SQL_GUARDAR_MESAS[] =
    "INSERT INTO mesa (numero_evento, numero_mesa, id_mesero)"
    " SELECT e, m.numero_mesa, m.id_mesero"
    "   FROM unnest($1::int[]) AS e, json_to_recordset($2::json) AS m(numero_mesa smallint, id_mesero integer)"
    " ON CONFLICT (numero_evento, numero_mesa) DO UPDATE SET id_mesero = EXCLUDED.id_mesero";
static const char SQL_GUARDAR_ACUERDOS[] =
    "INSERT INTO acuerdo (numero_evento, numero_paquete, id_producto, cantidad_acordada, modo_cobro)"
    " SELECT e, a.numero_paquete, a.id_producto, a.cantidad, a.modo_cobro"
    "   FROM unnest($1::int[]) AS e,"
    "        json_to_recordset($2::json) AS a(numero_paquete integer, id_producto integer, cantidad integer,"
    "                                        modo_cobro text)";

static cJSON *guardar_detalle(PGconn *pg, const char *eventos, const Evento *e, int reemplazar)
{
    cJSON *respuesta;
    const char *p_eventos[] = { eventos };
    const char *p_organizadores[] = { eventos, e->organizadores };
    const char *p_mesas[] = { eventos, e->mesas };
    const char *p_acuerdos[] = { eventos, e->acuerdos };
    if (reemplazar) {
        if ((respuesta = bd_paso(pg, SQL_BORRAR_ORGANIZADORES, 1, p_eventos, NULL)) != NULL)
            return respuesta;
        if ((respuesta = bd_paso(pg, SQL_BORRAR_ACUERDOS, 1, p_eventos, NULL)) != NULL)
            return respuesta;
    }
    if ((respuesta = bd_paso(pg, SQL_GUARDAR_ORGANIZADORES, 2, p_organizadores, NULL)) != NULL)
        return respuesta;
    if ((respuesta = bd_paso(pg, SQL_GUARDAR_MESAS, 2, p_mesas, NULL)) != NULL)
        return respuesta;
    return bd_paso(pg, SQL_GUARDAR_ACUERDOS, 2, p_acuerdos, NULL);
}

// ---------------------------------------------------------------------------
//  OP_CREAR_EVENTO: uno, o varios si "repetir" pide copias cada cierto tiempo.
//  Todas las copias llevan los mismos organizadores, mesas y paquetes.
// ---------------------------------------------------------------------------

// $1 fecha, $2 intervalo ("7 days"), $3 cuántos, $4 tipo, $5 asistentes
static const char SQL_CREAR_EVENTOS[] =
    "WITH nuevos AS ("
    "  INSERT INTO evento (fecha, tipo_evento, numero_asistentes)"
    "  SELECT ($1::date + n * $2::interval)::date, $4::text, $5::integer"
    "    FROM generate_series(0, $3::integer - 1) AS n"
    "  RETURNING numero_evento)"
    "SELECT '{' || string_agg(numero_evento::text, ',' ORDER BY numero_evento) || '}' FROM nuevos";

cJSON *op_crear_evento(Cliente *cliente, const cJSON *datos)
{
    cJSON *respuesta = exigir_administrador(cliente);
    if (respuesta != NULL)
        return respuesta;
    Evento e;
    if ((respuesta = leer_evento(datos, &e)) != NULL) {
        liberar_evento(&e);
        return respuesta;
    }

    // Repetición: "cada" 1 a 365 días, semanas o meses, "veces" eventos en total
    char intervalo[32] = "0 days", veces[8] = "1";
    const cJSON *repetir = cJSON_GetObjectItemCaseSensitive(datos, "repetir");
    if (repetir != NULL && !cJSON_IsNull(repetir)) {
        const char *cada = dato_texto(repetir, "cada");
        int cuanto, total;
        const char *unidad = cada == NULL                   ? NULL
                             : strcmp(cada, "dias") == 0    ? "days"
                             : strcmp(cada, "semanas") == 0 ? "weeks"
                             : strcmp(cada, "meses") == 0   ? "months"
                                                            : NULL;
        if (unidad == NULL || !dato_entero(repetir, "intervalo", 1, 365, &cuanto)
            || !dato_entero(repetir, "veces", 1, MAX_REPETICIONES, &total)) {
            liberar_evento(&e);
            return respuesta_error("VALIDACION_FALLIDA",
                                   "Para repetir elige cada cuántos días, semanas o meses y cuántas veces (hasta 52)");
        }
        snprintf(intervalo, sizeof intervalo, "%d %s", cuanto, unidad);
        snprintf(veces, sizeof veces, "%d", total);
    }

    PGconn *pg = bd_tomar();
    if (pg == NULL) {
        liberar_evento(&e);
        return bd_respuesta_sin_conexion();
    }
    PGresult *creados = NULL;
    const char *p_revisar[] = { e.mesas, e.acuerdos };
    const char *p_crear[] = { e.fecha, intervalo, veces, e.tipo, e.asistentes };

    PASO(bd_paso(pg, "BEGIN", 0, NULL, NULL));
    PASO(bd_revisar(pg, SQL_REVISAR_DETALLE, 2, p_revisar));
    creados = PQexecParams(pg, SQL_CREAR_EVENTOS, 5, NULL, p_crear, NULL, NULL, 0);
    if (!bd_correcto(creados)) {
        respuesta = bd_respuesta_error(creados);
        goto fin;
    }
    const char *eventos = PQgetvalue(creados, 0, 0); // "{18,19,20}"
    PASO(guardar_detalle(pg, eventos, &e, 0));
    PASO(bd_paso(pg, "COMMIT", 0, NULL, NULL));

    registrar("%s creó %s evento(s) de %s desde el %s: %s", cliente->sesion.usuario, veces, e.tipo, e.fecha, eventos);
    cJSON *resultado = cJSON_CreateObject();
    cJSON_AddNumberToObject(resultado, "creados", atoi(veces));
    respuesta = respuesta_ok(resultado);
    avisar_todos("eventos");

fin:
    PQclear(creados);
    liberar_evento(&e);
    bd_soltar(pg);
    return respuesta;
}

// ---------------------------------------------------------------------------
//  OP_MODIFICAR_EVENTO
// ---------------------------------------------------------------------------

// $1 evento, $2 mesas. Un evento terminado o cancelado ya no se edita, y una
// mesa guardada no se puede quitar (la app no tiene DELETE sobre mesa).
static const char SQL_REVISAR_EVENTO[] =
    "SELECT CASE"
    "  WHEN e.numero_evento IS NULL THEN 'El evento ya no existe'"
    "  WHEN e.estado IN ('finalizado', 'cancelado')"
    "  THEN 'El evento está ' || e.estado || '; ya no se puede modificar'"
    "  ELSE (SELECT 'La mesa ' || m.numero_mesa || ' ya está guardada y no se puede quitar'"
    "          FROM mesa m"
    "         WHERE m.numero_evento = e.numero_evento"
    "           AND NOT EXISTS (SELECT 1 FROM json_to_recordset($2::json) AS x(numero_mesa integer, id_mesero integer)"
    "                            WHERE x.numero_mesa = m.numero_mesa)"
    "         ORDER BY m.numero_mesa LIMIT 1)"
    " END"
    "  FROM (SELECT 1) AS uno LEFT JOIN evento e ON e.numero_evento = $1::integer";

// $1 evento, $2 cuenta del anfitrión (NULL = ninguna). Debe ser una cuenta del
// evento, y no se cambia si ya se le cargó algo: esos cargos son de esa cuenta.
static const char SQL_REVISAR_ANFITRION[] =
    "SELECT CASE"
    "  WHEN $2::smallint IS NOT NULL"
    "   AND NOT EXISTS (SELECT 1 FROM cuenta c WHERE c.numero_evento = $1::integer AND c.secuencial = $2::smallint)"
    "  THEN 'La cuenta del anfitrión no es de este evento'"
    "  WHEN $2::smallint IS DISTINCT FROM e.secuencial_anfitrion"
    "   AND EXISTS (SELECT 1 FROM comanda co WHERE co.numero_evento = e.numero_evento"
    "                                           AND co.tipo_cobro = 'anfitrion' AND co.estado <> 'cancelada')"
    "  THEN 'Ya hay consumos cargados a la cuenta del anfitrión; ya no se puede cambiar'"
    " END"
    "  FROM evento e WHERE e.numero_evento = $1::integer";

static const char SQL_MODIFICAR_EVENTO[] =
    "UPDATE evento SET fecha = $2::date, tipo_evento = $3::text, numero_asistentes = $4::integer,"
    "                  secuencial_anfitrion = $5::smallint"
    " WHERE numero_evento = $1::integer";

cJSON *op_modificar_evento(Cliente *cliente, const cJSON *datos)
{
    cJSON *respuesta = exigir_administrador(cliente);
    if (respuesta != NULL)
        return respuesta;
    int numero;
    if (!dato_entero(datos, "numero_evento", 1, 2147483647, &numero))
        return respuesta_error("VALIDACION_FALLIDA", "Falta el evento o no es válido");
    Evento e;
    if ((respuesta = leer_evento(datos, &e)) != NULL) {
        liberar_evento(&e);
        return respuesta;
    }

    PGconn *pg = bd_tomar();
    if (pg == NULL) {
        liberar_evento(&e);
        return bd_respuesta_sin_conexion();
    }
    char txt_numero[16], lista[24], anfitrion[8] = "";
    snprintf(txt_numero, sizeof txt_numero, "%d", numero);
    snprintf(lista, sizeof lista, "{%d}", numero);
    const char *p_anfitrion = NULL;
    const cJSON *campo_anfitrion = cJSON_GetObjectItemCaseSensitive(datos, "secuencial_anfitrion");
    if (campo_anfitrion != NULL && !cJSON_IsNull(campo_anfitrion)) {
        int secuencial;
        if (!dato_entero(datos, "secuencial_anfitrion", 1, 32767, &secuencial)) {
            liberar_evento(&e);
            bd_soltar(pg);
            return respuesta_error("VALIDACION_FALLIDA", "La cuenta del anfitrión no es válida");
        }
        snprintf(anfitrion, sizeof anfitrion, "%d", secuencial);
        p_anfitrion = anfitrion;
    }
    const char *p_revisar_evento[] = { txt_numero, e.mesas };
    const char *p_revisar_detalle[] = { e.mesas, e.acuerdos };
    const char *p_revisar_anfitrion[] = { txt_numero, p_anfitrion };
    const char *p_modificar[] = { txt_numero, e.fecha, e.tipo, e.asistentes, p_anfitrion };

    PASO(bd_paso(pg, "BEGIN", 0, NULL, NULL));
    PASO(bd_revisar(pg, SQL_REVISAR_EVENTO, 2, p_revisar_evento));
    PASO(bd_revisar(pg, SQL_REVISAR_DETALLE, 2, p_revisar_detalle));
    PASO(bd_revisar(pg, SQL_REVISAR_ANFITRION, 2, p_revisar_anfitrion));
    PASO(bd_paso(pg, SQL_MODIFICAR_EVENTO, 5, p_modificar, NULL));
    PASO(guardar_detalle(pg, lista, &e, 1));
    PASO(bd_paso(pg, "COMMIT", 0, NULL, NULL));

    registrar("%s modificó el evento %d", cliente->sesion.usuario, numero);
    respuesta = respuesta_ok(NULL);
    avisar_todos("eventos");
    avisar_todos("cuentas"); // las mesas y su mesero las ven el mesero y la caja

fin:
    liberar_evento(&e);
    bd_soltar(pg);
    return respuesta;
}

// ---------------------------------------------------------------------------
//  OP_CAMBIAR_ESTADO_EVENTO: { numero_evento, estado }
//    programado → en_curso   si no hay otro en curso y tiene mesas
//    en_curso   → finalizado si ya no quedan cuentas abiertas
//    programado → cancelado
// ---------------------------------------------------------------------------

// $1 evento, $2 estado nuevo
static const char SQL_REVISAR_ESTADO_EVENTO[] =
    "SELECT CASE"
    "  WHEN e.numero_evento IS NULL THEN 'El evento ya no existe'"
    "  WHEN $2::text = 'en_curso' AND e.estado <> 'programado'"
    "  THEN 'Solo se inicia un evento programado'"
    "  WHEN $2::text = 'en_curso' AND EXISTS (SELECT 1 FROM evento o WHERE o.estado = 'en_curso')"
    "  THEN 'Ya hay un evento en curso (#' || (SELECT min(o.numero_evento) FROM evento o WHERE o.estado = 'en_curso')"
    "       || '); finalízalo primero'"
    "  WHEN $2::text = 'en_curso' AND NOT EXISTS (SELECT 1 FROM mesa m WHERE m.numero_evento = e.numero_evento)"
    "  THEN 'Agrega las mesas del evento antes de iniciarlo'"
    "  WHEN $2::text = 'finalizado' AND e.estado <> 'en_curso'"
    "  THEN 'Solo se finaliza un evento en curso'"
    "  WHEN $2::text = 'finalizado'"
    "   AND EXISTS (SELECT 1 FROM cuenta c WHERE c.numero_evento = e.numero_evento AND c.estado = 'abierta')"
    "  THEN 'Quedan ' || (SELECT count(*) FROM cuenta c WHERE c.numero_evento = e.numero_evento"
    "                                                    AND c.estado = 'abierta')"
    "       || ' cuentas abiertas; ciérralas antes de finalizar'"
    "  WHEN $2::text = 'cancelado' AND e.estado <> 'programado'"
    "  THEN 'Solo se cancela un evento que no ha empezado'"
    " END"
    "  FROM (SELECT 1) AS uno LEFT JOIN evento e ON e.numero_evento = $1::integer";

static const char SQL_CAMBIAR_ESTADO_EVENTO[] =
    "UPDATE evento SET estado = $2::text WHERE numero_evento = $1::integer";

cJSON *op_cambiar_estado_evento(Cliente *cliente, const cJSON *datos)
{
    cJSON *respuesta = exigir_administrador(cliente);
    if (respuesta != NULL)
        return respuesta;
    int numero;
    if (!dato_entero(datos, "numero_evento", 1, 2147483647, &numero))
        return respuesta_error("VALIDACION_FALLIDA", "Falta el evento o no es válido");
    const char *estado = dato_texto(datos, "estado");
    if (estado == NULL || (strcmp(estado, "en_curso") != 0 && strcmp(estado, "finalizado") != 0
                           && strcmp(estado, "cancelado") != 0))
        return respuesta_error("VALIDACION_FALLIDA", "El estado debe ser en_curso, finalizado o cancelado");

    PGconn *pg = bd_tomar();
    if (pg == NULL)
        return bd_respuesta_sin_conexion();
    char txt_numero[16];
    snprintf(txt_numero, sizeof txt_numero, "%d", numero);
    const char *parametros[] = { txt_numero, estado };

    // El evento en curso es uno solo: se bloquean los eventos para que dos
    // administradores no inicien dos a la vez
    PASO(bd_paso(pg, "BEGIN", 0, NULL, NULL));
    PASO(bd_paso(pg, "LOCK TABLE evento IN SHARE ROW EXCLUSIVE MODE", 0, NULL, NULL));
    PASO(bd_revisar(pg, SQL_REVISAR_ESTADO_EVENTO, 2, parametros));
    PASO(bd_paso(pg, SQL_CAMBIAR_ESTADO_EVENTO, 2, parametros, NULL));
    PASO(bd_paso(pg, "COMMIT", 0, NULL, NULL));

    registrar("%s cambió el evento %d a %s", cliente->sesion.usuario, numero, estado);
    respuesta = respuesta_ok(NULL);
    // Iniciar o finalizar cambia el evento con el que trabaja todo el salón
    avisar_todos("eventos");
    avisar_todos("comandas");
    avisar_todos("cuentas");

fin:
    bd_soltar(pg);
    return respuesta;
}

// ---------------------------------------------------------------------------
//  OP_REGISTRAR_ANTICIPO: { numero_evento, monto, fecha (opcional, hoy) }
//  El dinero recibido no se edita ni se borra (099-permisos.sql).
// ---------------------------------------------------------------------------

static const char SQL_REVISAR_ANTICIPO[] =
    "SELECT CASE WHEN e.numero_evento IS NULL THEN 'El evento ya no existe'"
    "            WHEN e.estado = 'cancelado' THEN 'El evento está cancelado; ya no recibe anticipos' END"
    "  FROM (SELECT 1) AS uno LEFT JOIN evento e ON e.numero_evento = $1::integer";

static const char SQL_REGISTRAR_ANTICIPO[] =
    "INSERT INTO anticipo (numero_evento, monto, fecha)"
    " VALUES ($1::integer, $2::numeric, COALESCE($3::date, CURRENT_DATE))";

cJSON *op_registrar_anticipo(Cliente *cliente, const cJSON *datos)
{
    cJSON *respuesta = exigir_administrador(cliente);
    if (respuesta != NULL)
        return respuesta;
    int numero;
    if (!dato_entero(datos, "numero_evento", 1, 2147483647, &numero))
        return respuesta_error("VALIDACION_FALLIDA", "Falta el evento o no es válido");
    char monto[32];
    if (!dato_numero(datos, "monto", 99999999.99, 2, monto, sizeof monto) || strtod(monto, NULL) <= 0)
        return respuesta_error("VALIDACION_FALLIDA", "El monto del anticipo debe ser mayor que 0");
    const char *fecha = dato_texto(datos, "fecha");
    if (fecha != NULL && fecha[0] == '\0')
        fecha = NULL;
    if (fecha != NULL && !fecha_iso(fecha))
        return respuesta_error("VALIDACION_FALLIDA", "Escribe la fecha del anticipo como AAAA-MM-DD");

    PGconn *pg = bd_tomar();
    if (pg == NULL)
        return bd_respuesta_sin_conexion();
    char txt_numero[16];
    snprintf(txt_numero, sizeof txt_numero, "%d", numero);
    const char *p_revisar[] = { txt_numero };
    const char *p_registrar[] = { txt_numero, monto, fecha };

    PASO(bd_paso(pg, "BEGIN", 0, NULL, NULL));
    PASO(bd_revisar(pg, SQL_REVISAR_ANTICIPO, 1, p_revisar));
    PASO(bd_paso(pg, SQL_REGISTRAR_ANTICIPO, 3, p_registrar, NULL));
    PASO(bd_paso(pg, "COMMIT", 0, NULL, NULL));

    registrar("%s registró un anticipo de %s para el evento %d", cliente->sesion.usuario, monto, numero);
    respuesta = respuesta_ok(NULL);
    avisar_todos("eventos");

fin:
    bd_soltar(pg);
    return respuesta;
}
