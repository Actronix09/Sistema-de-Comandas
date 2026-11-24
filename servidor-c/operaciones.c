#include "operaciones.h"

#include <ctype.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sodium.h>

#include "avisos.h"
#include "bd.h"
#include "datos.h"
#include "registro.h"
#include "respuesta.h"

// Límites de una orden. Una mesa real nunca pide tanto de una vez; sirven para
// que nadie mande una orden gigante desde internet.
#define MAX_LINEAS     50
#define MAX_CANTIDAD   50
#define MAX_UNIDADES   100
#define MAX_DETALLES   255 // comanda.detalles es VARCHAR(255)

// Verificar o guardar una contraseña con Argon2id usa 64 MB de memoria. Se hace
// una a la vez para que muchos intentos de login juntos no se acaben la memoria.
static pthread_mutex_t candado_verificar = PTHREAD_MUTEX_INITIALIZER;

int operaciones_iniciar(void)
{
    return sodium_init() < 0 ? -1 : 0;
}

int operaciones_hash(const char *contrasena, char hash[crypto_pwhash_STRBYTES])
{
    pthread_mutex_lock(&candado_verificar);
    int resultado = crypto_pwhash_str(hash, contrasena, strlen(contrasena), crypto_pwhash_OPSLIMIT_INTERACTIVE,
                                      crypto_pwhash_MEMLIMIT_INTERACTIVE);
    pthread_mutex_unlock(&candado_verificar);
    return resultado;
}

// Ejecuta una consulta con parámetros. Si falla, deja la respuesta de error en
// *error y regresa NULL.
static PGresult *ejecutar(PGconn *pg, const char *sql, int num_parametros, const char *const *parametros,
                          cJSON **error)
{
    PGresult *resultado = PQexecParams(pg, sql, num_parametros, NULL, parametros, NULL, NULL, 0);
    if (bd_correcto(resultado))
        return resultado;
    *error = bd_respuesta_error(resultado);
    PQclear(resultado);
    return NULL;
}

// ---------------------------------------------------------------------------
//  OP_LOGIN y OP_LOGOUT
// ---------------------------------------------------------------------------

static const char SQL_LOGIN[] =
    "SELECT u.id_usuario, u.nombre, u.usuario, u.fecha_nacimiento::text, u.rol, u.contrasena_hash,"
    "       m.turno_asignado, b.estacion_asignada, c.nivel_acceso, c.caja_asignada"
    "  FROM usuario u"
    "  LEFT JOIN mesero m ON m.id_usuario = u.id_usuario"
    "  LEFT JOIN bar_cocina b ON b.id_usuario = u.id_usuario"
    "  LEFT JOIN cajero_admin c ON c.id_usuario = u.id_usuario"
    " WHERE lower(u.usuario) = lower($1)";

enum { L_ID, L_NOMBRE, L_USUARIO, L_NACIMIENTO, L_ROL, L_HASH, L_TURNO, L_ESTACION, L_NIVEL, L_CAJA };

static cJSON *credenciales_invalidas(const Cliente *cliente, const char *usuario)
{
    registrar("Login rechazado de %s: %s", cliente->ip, usuario);
    return respuesta_error("CREDENCIALES_INVALIDAS", "Usuario o contraseña incorrectos");
}

cJSON *op_login(Cliente *cliente, const cJSON *datos)
{
    const char *recibido = dato_texto(datos, "usuario");
    const char *contrasena = dato_texto(datos, "contrasena");
    if (recibido == NULL || contrasena == NULL || contrasena[0] == '\0')
        return respuesta_error("VALIDACION_FALLIDA", "Escribe tu usuario y tu contraseña");

    // Se ignoran los espacios alrededor del usuario, como en el cliente
    char usuario[48];
    while (isspace((unsigned char)*recibido))
        recibido++;
    snprintf(usuario, sizeof usuario, "%s", recibido);
    size_t largo = strlen(usuario);
    while (largo > 0 && isspace((unsigned char)usuario[largo - 1]))
        usuario[--largo] = '\0';

    // Los mismos mensajes si no existe o si la contraseña está mal, para no
    // revelar qué usuarios existen
    if (!usuario_con_formato(usuario) || strlen(contrasena) > 128)
        return credenciales_invalidas(cliente, usuario_con_formato(usuario) ? usuario : "(formato inválido)");

    PGconn *pg = bd_tomar();
    if (pg == NULL)
        return bd_respuesta_sin_conexion();
    cJSON *error = NULL;
    const char *parametros[] = { usuario };
    PGresult *fila = ejecutar(pg, SQL_LOGIN, 1, parametros, &error);
    // El resultado no depende de la conexión: se regresa antes de verificar la
    // contraseña, que es lo que tarda
    bd_soltar(pg);
    if (fila == NULL)
        return error;

    int valida = 0;
    if (PQntuples(fila) == 1) {
        const char *hash = PQgetvalue(fila, 0, L_HASH);
        pthread_mutex_lock(&candado_verificar);
        valida = crypto_pwhash_str_verify(hash, contrasena, strlen(contrasena)) == 0;
        pthread_mutex_unlock(&candado_verificar);
    }
    if (!valida) {
        PQclear(fila);
        return credenciales_invalidas(cliente, usuario);
    }

    Sesion *sesion = &cliente->sesion;
    avisos_bloquear();
    memset(sesion, 0, sizeof *sesion);
    sesion->activa = 1;
    sesion->id_usuario = atoi(PQgetvalue(fila, 0, L_ID));
    snprintf(sesion->usuario, sizeof sesion->usuario, "%s", PQgetvalue(fila, 0, L_USUARIO));
    snprintf(sesion->rol, sizeof sesion->rol, "%s", PQgetvalue(fila, 0, L_ROL));
    snprintf(sesion->nivel_acceso, sizeof sesion->nivel_acceso, "%s", PQgetvalue(fila, 0, L_NIVEL));
    avisos_desbloquear();
    registrar("Login de %s: %s (%s)", cliente->ip, sesion->usuario, sesion->rol);

    // Los mismos campos que usuarioActual en Api.qml; el hash nunca sale
    cJSON *perfil = cJSON_CreateObject();
    cJSON_AddNumberToObject(perfil, "id_usuario", sesion->id_usuario);
    cJSON_AddStringToObject(perfil, "nombre", PQgetvalue(fila, 0, L_NOMBRE));
    cJSON_AddStringToObject(perfil, "usuario", sesion->usuario);
    cJSON_AddStringToObject(perfil, "fecha_nacimiento", PQgetvalue(fila, 0, L_NACIMIENTO));
    cJSON_AddStringToObject(perfil, "rol", sesion->rol);
    if (strcmp(sesion->rol, "mesero") == 0) {
        cJSON_AddStringToObject(perfil, "turno_asignado", PQgetvalue(fila, 0, L_TURNO));
    } else if (strcmp(sesion->rol, "bar_cocina") == 0) {
        cJSON_AddStringToObject(perfil, "estacion_asignada", PQgetvalue(fila, 0, L_ESTACION));
    } else {
        cJSON_AddStringToObject(perfil, "nivel_acceso", sesion->nivel_acceso);
        cJSON_AddNumberToObject(perfil, "caja_asignada", atoi(PQgetvalue(fila, 0, L_CAJA)));
    }
    PQclear(fila);
    return respuesta_ok(perfil);
}

cJSON *op_logout(Cliente *cliente)
{
    registrar("Logout de %s: %s", cliente->ip, cliente->sesion.usuario);
    avisos_bloquear();
    memset(&cliente->sesion, 0, sizeof cliente->sesion);
    avisos_desbloquear();
    return respuesta_ok(NULL);
}

// ---------------------------------------------------------------------------
//  OP_LISTAR_PRODUCTOS
// ---------------------------------------------------------------------------

// PostgreSQL arma el JSON: así el precio llega como texto ("95.00") y nunca
// pasa por un double. La disponibilidad sale de v_producto, no del código.
static const char SQL_LISTAR_PRODUCTOS[] =
    "SELECT COALESCE(json_agg(fila ORDER BY fila.id_producto), '[]')::text"
    "  FROM ("
    "    SELECT p.id_producto, p.nombre, p.descripcion, p.categoria, p.precio::text AS precio,"
    "           p.id_producto_principal, COALESCE(principal.nombre, '') AS variante_de,"
    "           p.disponible,"
    "           COALESCE((SELECT json_agg(i.nombre ORDER BY i.nombre)"
    "                       FROM receta r"
    "                       JOIN ingrediente i ON i.id_ingrediente = r.id_ingrediente"
    "                      WHERE r.id_producto = p.id_producto), '[]') AS ingredientes,"
    "           COALESCE((SELECT FLOOR(MIN(i.stock_actual / r.cantidad_por_porcion))::integer"
    "                       FROM receta r"
    "                       JOIN ingrediente i ON i.id_ingrediente = r.id_ingrediente"
    "                      WHERE r.id_producto = p.id_producto), 99) AS maximo"
    "      FROM v_producto p"
    "      LEFT JOIN producto principal ON principal.id_producto = p.id_producto_principal"
    "  ) fila";

cJSON *op_listar_productos(void)
{
    PGconn *pg = bd_tomar();
    if (pg == NULL)
        return bd_respuesta_sin_conexion();
    cJSON *error = NULL;
    PGresult *resultado = ejecutar(pg, SQL_LISTAR_PRODUCTOS, 0, NULL, &error);
    bd_soltar(pg);
    if (resultado == NULL)
        return error;

    cJSON *productos = cJSON_Parse(PQgetvalue(resultado, 0, 0));
    PQclear(resultado);
    if (productos == NULL)
        return respuesta_error("ERROR_BD", "La base regresó una lista de productos que no se pudo leer");
    return respuesta_ok(productos);
}

// ---------------------------------------------------------------------------
//  OP_CREAR_PEDIDO
// ---------------------------------------------------------------------------

// Revisa las líneas de la orden y las deja como JSON limpio para la base:
// [{"id_producto": 1, "cantidad": 2, "detalles": "sin hielo"}, ...]
// Regresa NULL si están bien, o la respuesta de error.
static cJSON *validar_lineas(const cJSON *lineas, char **json_lineas)
{
    if (!cJSON_IsArray(lineas) || cJSON_GetArraySize(lineas) == 0)
        return respuesta_error("VALIDACION_FALLIDA", "La orden está vacía");
    if (cJSON_GetArraySize(lineas) > MAX_LINEAS)
        return respuesta_error("VALIDACION_FALLIDA", "La orden tiene demasiadas líneas");

    cJSON *limpias = cJSON_CreateArray();
    int unidades = 0;
    const cJSON *linea;
    char mensaje[160];
    cJSON_ArrayForEach(linea, lineas) {
        int id_producto, cantidad;
        if (!cJSON_IsObject(linea) || !dato_entero(linea, "id_producto", 1, INT_MAX, &id_producto)) {
            cJSON_Delete(limpias);
            return respuesta_error("VALIDACION_FALLIDA", "Una línea de la orden no trae un producto válido");
        }
        if (!dato_entero(linea, "cantidad", 1, MAX_CANTIDAD, &cantidad)) {
            cJSON_Delete(limpias);
            snprintf(mensaje, sizeof mensaje, "Cantidad inválida para el producto %d (de 1 a %d)", id_producto,
                     MAX_CANTIDAD);
            return respuesta_error("VALIDACION_FALLIDA", mensaje);
        }
        const cJSON *detalles = cJSON_GetObjectItemCaseSensitive(linea, "detalles");
        if (detalles != NULL && !cJSON_IsNull(detalles)
            && (!cJSON_IsString(detalles) || caracteres_utf8(detalles->valuestring) > MAX_DETALLES)) {
            cJSON_Delete(limpias);
            snprintf(mensaje, sizeof mensaje, "Los detalles deben ser texto de hasta %d caracteres", MAX_DETALLES);
            return respuesta_error("VALIDACION_FALLIDA", mensaje);
        }
        unidades += cantidad;
        if (unidades > MAX_UNIDADES) {
            cJSON_Delete(limpias);
            snprintf(mensaje, sizeof mensaje, "Una orden no puede pasar de %d unidades", MAX_UNIDADES);
            return respuesta_error("VALIDACION_FALLIDA", mensaje);
        }

        cJSON *limpia = cJSON_CreateObject();
        cJSON_AddNumberToObject(limpia, "id_producto", id_producto);
        cJSON_AddNumberToObject(limpia, "cantidad", cantidad);
        if (cJSON_IsString(detalles))
            cJSON_AddStringToObject(limpia, "detalles", detalles->valuestring);
        else
            cJSON_AddNullToObject(limpia, "detalles");
        cJSON_AddItemToArray(limpias, limpia);
    }
    *json_lineas = cJSON_PrintUnformatted(limpias);
    cJSON_Delete(limpias);
    return NULL;
}

// La cuenta en el evento en curso, y si la mesa existe en ese evento
static const char SQL_CUENTA[] =
    "SELECT e.numero_evento, c.numero_mesa, c.nombre, c.estado, c.folio,"
    "       EXISTS (SELECT 1 FROM mesa m"
    "                WHERE m.numero_evento = e.numero_evento AND m.numero_mesa = $2::smallint)"
    "  FROM evento e"
    "  LEFT JOIN cuenta c ON c.numero_evento = e.numero_evento AND c.secuencial = $1::smallint"
    " WHERE e.estado = 'en_curso'"
    " ORDER BY e.fecha DESC, e.numero_evento DESC"
    " LIMIT 1";

enum { C_EVENTO, C_MESA, C_NOMBRE, C_ESTADO, C_FOLIO, C_MESA_EXISTE };

#define LINEAS_SQL \
    "json_to_recordset($1::json) AS l(id_producto integer, cantidad integer, detalles text)"

static const char SQL_PRODUCTO_FALTANTE[] =
    "SELECT l.id_producto FROM " LINEAS_SQL
    " WHERE NOT EXISTS (SELECT 1 FROM producto p WHERE p.id_producto = l.id_producto)"
    " LIMIT 1";

// Suma lo que pide la orden completa por ingrediente. Es un aviso temprano: el
// stock se descuenta cuando la comanda entra a preparación (tg_comanda_stock),
// y ahí la base hace la revisión definitiva.
static const char SQL_STOCK[] =
    "SELECT i.nombre, trim_scale(i.stock_actual)::text, i.unidad_medida,"
    "       trim_scale(SUM(r.cantidad_por_porcion * l.cantidad))::text"
    "  FROM " LINEAS_SQL
    "  JOIN receta r ON r.id_producto = l.id_producto"
    "  JOIN ingrediente i ON i.id_ingrediente = r.id_ingrediente"
    " GROUP BY i.id_ingrediente, i.nombre, i.stock_actual, i.unidad_medida"
    " HAVING SUM(r.cantidad_por_porcion * l.cantidad) > i.stock_actual"
    " ORDER BY i.nombre"
    " LIMIT 1";

// Una comanda por unidad, en el orden de la orden. Todas en un solo INSERT:
// now() vale lo mismo en toda la transacción, así que cocina las ve como una
// sola ronda. El precio lo copia tg_comanda_precio del catálogo.
static const char SQL_INSERTAR[] =
    "INSERT INTO comanda (numero_evento, secuencial, id_mesero, id_producto, detalles)"
    " SELECT $2::integer, $3::smallint, $4::integer, l.id_producto, NULLIF(btrim(l.detalles), '')"
    "   FROM ROWS FROM (json_to_recordset($1::json) AS (id_producto integer, cantidad integer, detalles text))"
    "        WITH ORDINALITY AS l(id_producto, cantidad, detalles, orden)"
    "  CROSS JOIN LATERAL generate_series(1, l.cantidad) AS u(n)"
    "  ORDER BY l.orden, u.n"
    " RETURNING id_comanda";

cJSON *op_crear_pedido(Cliente *cliente, const cJSON *datos)
{
    if (strcmp(cliente->sesion.rol, "mesero") != 0)
        return respuesta_error("NO_AUTORIZADO", "Tu rol no puede hacer esta operación");

    int numero_mesa, secuencial;
    char mensaje[200];
    if (!dato_entero(datos, "numero_mesa", 1, SHRT_MAX, &numero_mesa))
        return respuesta_error("VALIDACION_FALLIDA", "Falta el número de mesa o no es válido");
    if (!dato_entero(datos, "secuencial", 1, SHRT_MAX, &secuencial)) {
        snprintf(mensaje, sizeof mensaje, "Elige una cuenta de la mesa %d", numero_mesa);
        return respuesta_error("VALIDACION_FALLIDA", mensaje);
    }
    char *json_lineas = NULL;
    cJSON *respuesta = validar_lineas(cJSON_GetObjectItemCaseSensitive(datos, "lineas"), &json_lineas);
    if (respuesta != NULL)
        return respuesta;

    PGconn *pg = bd_tomar();
    if (pg == NULL) {
        cJSON_free(json_lineas);
        return bd_respuesta_sin_conexion();
    }

    char txt_mesa[8], txt_secuencial[8], txt_mesero[16];
    snprintf(txt_mesa, sizeof txt_mesa, "%d", numero_mesa);
    snprintf(txt_secuencial, sizeof txt_secuencial, "%d", secuencial);
    snprintf(txt_mesero, sizeof txt_mesero, "%d", cliente->sesion.id_usuario);

    PGresult *cuenta = NULL, *resultado = NULL;

    // Todo o nada: si algo falla, bd_soltar() hace ROLLBACK
    if ((resultado = ejecutar(pg, "BEGIN", 0, NULL, &respuesta)) == NULL)
        goto fin;
    PQclear(resultado);
    resultado = NULL;

    const char *p_cuenta[] = { txt_secuencial, txt_mesa };
    if ((cuenta = ejecutar(pg, SQL_CUENTA, 2, p_cuenta, &respuesta)) == NULL)
        goto fin;
    if (PQntuples(cuenta) == 0) {
        respuesta = respuesta_error("VALIDACION_FALLIDA", "No hay ningún evento en curso");
        goto fin;
    }
    if (strcmp(PQgetvalue(cuenta, 0, C_MESA_EXISTE), "t") != 0) {
        snprintf(mensaje, sizeof mensaje, "La mesa %d no existe en este evento", numero_mesa);
        respuesta = respuesta_error("VALIDACION_FALLIDA", mensaje);
        goto fin;
    }
    if (PQgetisnull(cuenta, 0, C_MESA) || atoi(PQgetvalue(cuenta, 0, C_MESA)) != numero_mesa) {
        snprintf(mensaje, sizeof mensaje, "Elige una cuenta de la mesa %d", numero_mesa);
        respuesta = respuesta_error("VALIDACION_FALLIDA", mensaje);
        goto fin;
    }
    // Mismo texto que tg_comanda_cuenta_abierta, que de todos modos lo revisa
    if (strcmp(PQgetvalue(cuenta, 0, C_ESTADO), "abierta") != 0) {
        respuesta = respuesta_error("VALIDACION_FALLIDA", "No se pueden agregar comandas a una cuenta cerrada");
        goto fin;
    }

    const char *p_lineas[] = { json_lineas };
    if ((resultado = ejecutar(pg, SQL_PRODUCTO_FALTANTE, 1, p_lineas, &respuesta)) == NULL)
        goto fin;
    if (PQntuples(resultado) > 0) {
        snprintf(mensaje, sizeof mensaje, "No existe el producto %s", PQgetvalue(resultado, 0, 0));
        respuesta = respuesta_error("VALIDACION_FALLIDA", mensaje);
        goto fin;
    }
    PQclear(resultado);
    resultado = NULL;

    if ((resultado = ejecutar(pg, SQL_STOCK, 1, p_lineas, &respuesta)) == NULL)
        goto fin;
    if (PQntuples(resultado) > 0) {
        snprintf(mensaje, sizeof mensaje, "No alcanza %s: la orden necesita %s %s y hay %s",
                 PQgetvalue(resultado, 0, 0), PQgetvalue(resultado, 0, 3), PQgetvalue(resultado, 0, 2),
                 PQgetvalue(resultado, 0, 1));
        respuesta = respuesta_error("SIN_STOCK", mensaje);
        goto fin;
    }
    PQclear(resultado);
    resultado = NULL;

    const char *p_insertar[] = { json_lineas, PQgetvalue(cuenta, 0, C_EVENTO), txt_secuencial, txt_mesero };
    if ((resultado = ejecutar(pg, SQL_INSERTAR, 4, p_insertar, &respuesta)) == NULL)
        goto fin;
    PGresult *confirmar = ejecutar(pg, "COMMIT", 0, NULL, &respuesta);
    if (confirmar == NULL)
        goto fin;
    PQclear(confirmar);

    int creadas = PQntuples(resultado);
    cJSON *ids = cJSON_CreateArray();
    for (int i = 0; i < creadas; i++)
        cJSON_AddItemToArray(ids, cJSON_CreateNumber(atoi(PQgetvalue(resultado, i, 0))));

    cJSON *datos_respuesta = cJSON_CreateObject();
    cJSON_AddNumberToObject(datos_respuesta, "creadas", creadas);
    cJSON_AddItemToObject(datos_respuesta, "id_comandas", ids);
    cJSON_AddNumberToObject(datos_respuesta, "numero_evento", atoi(PQgetvalue(cuenta, 0, C_EVENTO)));
    cJSON_AddNumberToObject(datos_respuesta, "numero_mesa", numero_mesa);
    cJSON_AddNumberToObject(datos_respuesta, "secuencial", secuencial);
    cJSON_AddStringToObject(datos_respuesta, "folio", PQgetvalue(cuenta, 0, C_FOLIO));
    cJSON_AddStringToObject(datos_respuesta, "cuenta", PQgetvalue(cuenta, 0, C_NOMBRE));
    respuesta = respuesta_ok(datos_respuesta);
    registrar("%s creó %d comanda(s) en la cuenta %s", cliente->sesion.usuario, creadas,
              PQgetvalue(cuenta, 0, C_FOLIO));
    avisar_todos("comandas");
    avisar_todos("cuentas");

fin:
    PQclear(resultado);
    PQclear(cuenta);
    bd_soltar(pg);
    cJSON_free(json_lineas);
    return respuesta;
}
