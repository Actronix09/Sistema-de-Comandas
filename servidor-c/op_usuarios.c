// Operaciones de usuarios: solo las hace un administrador.
//
// Un usuario es una fila de "usuario" más una de su subtipo (mesero,
// bar_cocina o cajero_admin) en la misma transacción: la especialización es
// total y la revisan los triggers diferidos ct_*_total al confirmar.
#include "operaciones.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#include <sodium.h>

#include "avisos.h"
#include "bd.h"
#include "datos.h"
#include "registro.h"
#include "respuesta.h"
#include "validaciones.h"

#define PASO(llamada)                                                                   \
    do {                                                                                \
        if ((respuesta = (llamada)) != NULL)                                            \
            goto fin;                                                                   \
    } while (0)

// Las claves foráneas hacia los subtipos son ON DELETE RESTRICT: quien ya
// atendió mesas, preparó, canceló o cobró se queda para no perder la historia.
#define SQL_TIENE_HISTORIA(u)                                                           \
    "(EXISTS (SELECT 1 FROM mesa x WHERE x.id_mesero = " u ".id_usuario)"               \
    " OR EXISTS (SELECT 1 FROM comanda x"                                               \
    "             WHERE " u ".id_usuario IN (x.id_mesero, x.id_bar_cocina, x.id_cajero_cancela))" \
    " OR EXISTS (SELECT 1 FROM pago x WHERE " u ".id_usuario IN (x.id_cajero, x.id_mesero_propina)))"

// $1 = quien pregunta, $2 = si es la demo. motivo_sin_baja explica por qué no se
// puede eliminar; vacío si sí se puede. El hash de la contraseña nunca sale.
#define SQL_USUARIO_JSON                                                                \
    "json_strip_nulls(json_build_object('id_usuario', u.id_usuario, 'nombre', u.nombre,"  \
    "  'usuario', u.usuario, 'fecha_nacimiento', u.fecha_nacimiento::text, 'rol', u.rol,"  \
    "  'turno_asignado', m.turno_asignado, 'estacion_asignada', b.estacion_asignada,"     \
    "  'nivel_acceso', c.nivel_acceso, 'caja_asignada', c.caja_asignada,"                 \
    "  'es_yo', u.id_usuario = $1::integer,"                                              \
    "  'motivo_sin_baja', CASE"                                                           \
    "     WHEN u.id_usuario = $1::integer THEN 'No puedes eliminar tu propia cuenta'"      \
    "     WHEN $2::boolean THEN 'En la demo no se eliminan usuarios'"                       \
    "     WHEN " SQL_TIENE_HISTORIA("u")                                                  \
    "     THEN u.nombre || ' tiene mesas, comandas o pagos registrados; no se puede eliminar'" \
    "     ELSE '' END))"

#define SQL_USUARIO_DESDE                                                               \
    " FROM usuario u"                                                                   \
    " LEFT JOIN mesero m ON m.id_usuario = u.id_usuario"                                \
    " LEFT JOIN bar_cocina b ON b.id_usuario = u.id_usuario"                            \
    " LEFT JOIN cajero_admin c ON c.id_usuario = u.id_usuario"

static const char SQL_LISTAR_USUARIOS[] =
    "SELECT COALESCE(json_agg(" SQL_USUARIO_JSON " ORDER BY u.nombre), '[]')::text" SQL_USUARIO_DESDE;

static const char SQL_USUARIO[] =
    "SELECT " SQL_USUARIO_JSON "::text" SQL_USUARIO_DESDE " WHERE u.id_usuario = $3::integer";

cJSON *op_listar_usuarios(Cliente *cliente)
{
    cJSON *negado = exigir_administrador(cliente);
    if (negado != NULL)
        return negado;
    char yo[16];
    snprintf(yo, sizeof yo, "%d", cliente->sesion.id_usuario);
    const char *parametros[] = { yo, modo_demo() ? "true" : "false" };
    return bd_consultar(SQL_LISTAR_USUARIOS, 2, parametros, NULL);
}

// ---------------------------------------------------------------------------
//  OP_CREAR_USUARIO y OP_MODIFICAR_USUARIO
//  datos: { id_usuario (solo al modificar), nombre, usuario, fecha_nacimiento,
//           contrasena (vacía al modificar = no cambia), rol,
//           turno_asignado | estacion_asignada | nivel_acceso y caja_asignada }
// ---------------------------------------------------------------------------

// "AAAA-MM-DD", que exista y que esté entre 1900 y hoy
static int fecha_valida(const char *texto, const char **error)
{
    *error = "Escribe la fecha de nacimiento como AAAA-MM-DD";
    if (!fecha_iso(texto))
        return 0;

    char hoy[11];
    time_t ahora = time(NULL);
    struct tm local;
    localtime_r(&ahora, &local);
    strftime(hoy, sizeof hoy, "%Y-%m-%d", &local);
    *error = "La fecha de nacimiento está fuera de rango";
    return strcmp(texto, "1900-01-01") >= 0 && strcmp(texto, hoy) <= 0;
}

static int en_lista(const char *valor, const char *const *lista)
{
    for (; valor != NULL && *lista != NULL; lista++) {
        if (strcmp(valor, *lista) == 0)
            return 1;
    }
    return 0;
}

static const char *const ROLES[] = { "mesero", "bar_cocina", "cajero_admin", NULL };
static const char *const TURNOS[] = { "matutino", "vespertino", "nocturno", NULL };
static const char *const ESTACIONES[] = { "bar", "cocina", NULL };
static const char *const NIVELES[] = { "cajero", "administrador", NULL };

static const char SQL_REVISAR_USUARIO_NUEVO[] =
    "SELECT 'El usuario «' || $1::text || '» ya existe' FROM usuario"
    " WHERE lower(usuario) = lower($1::text)";

static const char SQL_REVISAR_USUARIO_EDITADO[] =
    "SELECT CASE"
    "  WHEN u.id_usuario IS NULL THEN 'El usuario ya no existe'"
    "  WHEN u.rol <> $2::text THEN 'El rol no se puede cambiar; crea otro usuario'"
    "  WHEN $4::boolean AND lower(u.usuario) <> lower($3::text)"
    "  THEN 'En la demo no se cambia el nombre de acceso'"
    "  WHEN EXISTS (SELECT 1 FROM usuario o WHERE lower(o.usuario) = lower($3::text)"
    "                                         AND o.id_usuario <> u.id_usuario)"
    "  THEN 'El usuario «' || $3::text || '» ya existe'"
    " END"
    "  FROM (SELECT 1) AS uno LEFT JOIN usuario u ON u.id_usuario = $1::integer";

static const char SQL_CREAR_USUARIO[] =
    "INSERT INTO usuario (nombre, usuario, fecha_nacimiento, contrasena_hash, rol)"
    " VALUES ($1::text, $2::text, $3::date, $4::text, $5::text)"
    " RETURNING id_usuario::text";

static const char SQL_MODIFICAR_USUARIO[] =
    "UPDATE usuario SET nombre = $2::text, usuario = $3::text, fecha_nacimiento = $4::date,"
    "       contrasena_hash = COALESCE($5::text, contrasena_hash)"
    " WHERE id_usuario = $1::integer";

// El subtipo: [0] al crear, [1] al modificar. Mesero y bar_cocina usan $1 y $2;
// cajero_admin también $3.
static const char *const SQL_SUBTIPO[3][2] = {
    { "INSERT INTO mesero (id_usuario, turno_asignado) VALUES ($1::integer, $2::text)",
      "UPDATE mesero SET turno_asignado = $2::text WHERE id_usuario = $1::integer" },
    { "INSERT INTO bar_cocina (id_usuario, estacion_asignada) VALUES ($1::integer, $2::text)",
      "UPDATE bar_cocina SET estacion_asignada = $2::text WHERE id_usuario = $1::integer" },
    { "INSERT INTO cajero_admin (id_usuario, nivel_acceso, caja_asignada)"
      " VALUES ($1::integer, $2::text, $3::smallint)",
      "UPDATE cajero_admin SET nivel_acceso = $2::text, caja_asignada = $3::smallint"
      " WHERE id_usuario = $1::integer" },
};

cJSON *op_guardar_usuario(Cliente *cliente, const cJSON *datos, int es_nuevo)
{
    cJSON *respuesta = exigir_administrador(cliente);
    if (respuesta != NULL)
        return respuesta;

    int id_usuario = 0;
    if (!es_nuevo && !dato_entero(datos, "id_usuario", 1, 2147483647, &id_usuario))
        return respuesta_error("VALIDACION_FALLIDA", "Falta el usuario o no es válido");

    char nombre[512], usuario[48];
    const char *recibido = dato_texto(datos, "nombre");
    if (recibido == NULL || !recortar(recibido, nombre, sizeof nombre) || nombre[0] == '\0'
        || caracteres_utf8(nombre) > 120)
        return respuesta_error("VALIDACION_FALLIDA", "El nombre es obligatorio y admite hasta 120 caracteres");
    recibido = dato_texto(datos, "usuario");
    if (recibido == NULL || !recortar(recibido, usuario, sizeof usuario) || !usuario_con_formato(usuario))
        return respuesta_error("VALIDACION_FALLIDA",
                               "El usuario empieza con letra y lleva de 3 a 40 letras sin acento o números");

    const char *fecha = dato_texto(datos, "fecha_nacimiento");
    const char *error_fecha = "Escribe la fecha de nacimiento como AAAA-MM-DD";
    char fecha_limpia[16];
    if (fecha == NULL || !recortar(fecha, fecha_limpia, sizeof fecha_limpia)
        || !fecha_valida(fecha_limpia, &error_fecha))
        return respuesta_error("VALIDACION_FALLIDA", error_fecha);

    const char *contrasena = dato_texto(datos, "contrasena");
    if (contrasena != NULL && contrasena[0] == '\0')
        contrasena = NULL;
    if (es_nuevo && contrasena == NULL)
        return respuesta_error("VALIDACION_FALLIDA", "Escribe una contraseña");
    if (contrasena != NULL && (strlen(contrasena) > 128 || validar_password(contrasena) != 0))
        return respuesta_error("VALIDACION_FALLIDA", "La contraseña necesita 8 caracteres o más, una mayúscula, "
                                                     "una minúscula, un número y 2 símbolos de * / _ - + .");
    if (!es_nuevo && contrasena != NULL && modo_demo())
        return respuesta_error("NO_AUTORIZADO", "En la demo no se cambian contraseñas");

    const char *rol = dato_texto(datos, "rol");
    if (!en_lista(rol, ROLES))
        return respuesta_error("VALIDACION_FALLIDA", "Elige el rol");
    int subtipo = strcmp(rol, "mesero") == 0 ? 0 : strcmp(rol, "bar_cocina") == 0 ? 1 : 2;
    const char *dato_subtipo;
    char caja[8] = "";
    if (subtipo == 0) {
        dato_subtipo = dato_texto(datos, "turno_asignado");
        if (!en_lista(dato_subtipo, TURNOS))
            return respuesta_error("VALIDACION_FALLIDA", "Elige el turno");
    } else if (subtipo == 1) {
        dato_subtipo = dato_texto(datos, "estacion_asignada");
        if (!en_lista(dato_subtipo, ESTACIONES))
            return respuesta_error("VALIDACION_FALLIDA", "Elige la estación");
    } else {
        dato_subtipo = dato_texto(datos, "nivel_acceso");
        if (!en_lista(dato_subtipo, NIVELES))
            return respuesta_error("VALIDACION_FALLIDA", "Elige el nivel de acceso");
        int numero_caja;
        if (!dato_entero(datos, "caja_asignada", 1, 32767, &numero_caja))
            return respuesta_error("VALIDACION_FALLIDA", "La caja debe ser un número entero mayor que 0");
        snprintf(caja, sizeof caja, "%d", numero_caja);
        // Como nadie se puede bajar de nivel a sí mismo, siempre queda un administrador
        if (id_usuario == cliente->sesion.id_usuario && strcmp(dato_subtipo, "administrador") != 0)
            return respuesta_error("VALIDACION_FALLIDA", "No puedes quitarte a ti mismo el nivel de administrador");
    }

    // Argon2id, el mismo que revisa el login. Tarda y usa 64 MB: se hace antes
    // de tomar una conexión.
    char hash[crypto_pwhash_STRBYTES];
    if (contrasena != NULL && operaciones_hash(contrasena, hash) != 0)
        return respuesta_error("ERROR_BD", "No se pudo guardar la contraseña");

    PGconn *pg = bd_tomar();
    if (pg == NULL)
        return bd_respuesta_sin_conexion();
    cJSON *nuevo_id = NULL, *perfil = NULL;
    char txt_id[16], yo[16];
    snprintf(txt_id, sizeof txt_id, "%d", id_usuario);
    snprintf(yo, sizeof yo, "%d", cliente->sesion.id_usuario);
    const char *demo = modo_demo() ? "true" : "false";

    PASO(bd_paso(pg, "BEGIN", 0, NULL, NULL));
    if (es_nuevo) {
        const char *p_revisar[] = { usuario };
        const char *p_crear[] = { nombre, usuario, fecha_limpia, hash, rol };
        PASO(bd_revisar(pg, SQL_REVISAR_USUARIO_NUEVO, 1, p_revisar));
        PASO(bd_paso(pg, SQL_CREAR_USUARIO, 5, p_crear, &nuevo_id));
        if (!cJSON_IsNumber(nuevo_id)) {
            respuesta = respuesta_error("ERROR_BD", "No se pudo crear el usuario");
            goto fin;
        }
        snprintf(txt_id, sizeof txt_id, "%d", nuevo_id->valueint);
    } else {
        const char *p_revisar[] = { txt_id, rol, usuario, demo };
        const char *p_modificar[] = { txt_id, nombre, usuario, fecha_limpia, contrasena != NULL ? hash : NULL };
        PASO(bd_revisar(pg, SQL_REVISAR_USUARIO_EDITADO, 4, p_revisar));
        PASO(bd_paso(pg, SQL_MODIFICAR_USUARIO, 5, p_modificar, NULL));
    }
    const char *p_subtipo[] = { txt_id, dato_subtipo, caja };
    PASO(bd_paso(pg, SQL_SUBTIPO[subtipo][es_nuevo ? 0 : 1], subtipo == 2 ? 3 : 2, p_subtipo, NULL));
    const char *p_perfil[] = { yo, demo, txt_id };
    PASO(bd_paso(pg, SQL_USUARIO, 3, p_perfil, &perfil));
    PASO(bd_paso(pg, "COMMIT", 0, NULL, NULL));

    // Si se editó a sí mismo, la sesión lleva el nombre de acceso nuevo
    if (id_usuario == cliente->sesion.id_usuario) {
        avisos_bloquear();
        snprintf(cliente->sesion.usuario, sizeof cliente->sesion.usuario, "%s", usuario);
        avisos_desbloquear();
    }
    registrar("%s %s el usuario %s", cliente->sesion.usuario, es_nuevo ? "creó" : "modificó", usuario);
    respuesta = respuesta_ok(perfil);
    perfil = NULL;
    avisar_todos("usuarios");

fin:
    sodium_memzero(hash, sizeof hash);
    cJSON_Delete(nuevo_id);
    cJSON_Delete(perfil);
    bd_soltar(pg);
    return respuesta;
}

// ---------------------------------------------------------------------------
//  OP_ELIMINAR_USUARIO: { id_usuario }
// ---------------------------------------------------------------------------

static const char SQL_REVISAR_BAJA[] =
    "SELECT CASE WHEN u.id_usuario IS NULL THEN 'El usuario ya no existe'"
    "            WHEN " SQL_TIENE_HISTORIA("u")
    "            THEN u.nombre || ' tiene mesas, comandas o pagos registrados; no se puede eliminar' END"
    "  FROM (SELECT 1) AS uno LEFT JOIN usuario u ON u.id_usuario = $1::integer";

// Los subtipos se van con él (ON DELETE CASCADE)
static const char SQL_ELIMINAR_USUARIO[] =
    "DELETE FROM usuario WHERE id_usuario = $1::integer RETURNING json_build_object('nombre', nombre)::text";

cJSON *op_eliminar_usuario(Cliente *cliente, const cJSON *datos)
{
    cJSON *respuesta = exigir_administrador(cliente);
    if (respuesta != NULL)
        return respuesta;
    int id_usuario;
    if (!dato_entero(datos, "id_usuario", 1, 2147483647, &id_usuario))
        return respuesta_error("VALIDACION_FALLIDA", "Falta el usuario o no es válido");
    if (id_usuario == cliente->sesion.id_usuario)
        return respuesta_error("VALIDACION_FALLIDA", "No puedes eliminar tu propia cuenta");
    if (modo_demo())
        return respuesta_error("NO_AUTORIZADO", "En la demo no se eliminan usuarios");

    PGconn *pg = bd_tomar();
    if (pg == NULL)
        return bd_respuesta_sin_conexion();
    cJSON *eliminado = NULL;
    char id[16];
    snprintf(id, sizeof id, "%d", id_usuario);
    const char *parametros[] = { id };

    PASO(bd_paso(pg, "BEGIN", 0, NULL, NULL));
    PASO(bd_revisar(pg, SQL_REVISAR_BAJA, 1, parametros));
    PASO(bd_paso(pg, SQL_ELIMINAR_USUARIO, 1, parametros, &eliminado));
    PASO(bd_paso(pg, "COMMIT", 0, NULL, NULL));

    registrar("%s eliminó al usuario %s", cliente->sesion.usuario, id);
    respuesta = respuesta_ok(eliminado);
    eliminado = NULL;
    avisar_todos("usuarios");

fin:
    cJSON_Delete(eliminado);
    bd_soltar(pg);
    return respuesta;
}
