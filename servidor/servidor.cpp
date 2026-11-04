#include "servidor.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QWebSocket>

#include <sodium.h>

namespace {

// Un mensaje de este sistema nunca se acerca a este tamaño
const qint64 TAM_MAXIMO_MENSAJE = 1024 * 1024;

QJsonObject respuestaOk(const QJsonValue &datos = QJsonValue())
{
    return { { "ok", true }, { "datos", datos } };
}

QJsonObject respuestaError(const QString &codigo, const QString &mensaje)
{
    return { { "ok", false }, { "error", QJsonObject{ { "codigo", codigo }, { "mensaje", mensaje } } } };
}

QJsonObject errorDeBase()
{
    return respuestaError("ERROR_BD", "No se pudo consultar la base de datos");
}

// Los nombres de protocolo.h, que son los que viajan en "op"
const QHash<QString, TipoOperacion> OPERACIONES = {
    { "OP_LOGIN", OP_LOGIN },
    { "OP_CREAR_USUARIO", OP_CREAR_USUARIO },
    { "OP_RECUPERAR_PASS", OP_RECUPERAR_PASS },
    { "OP_VERIFICAR_USUARIO", OP_VERIFICAR_USUARIO },
    { "OP_LOGOUT", OP_LOGOUT },
    { "OP_LISTAR_PRODUCTOS", OP_LISTAR_PRODUCTOS },
    { "OP_CREAR_PEDIDO", OP_CREAR_PEDIDO },
    { "OP_LISTAR_PEDIDOS", OP_LISTAR_PEDIDOS },
    { "OP_CAMBIAR_ESTADO_PEDIDO", OP_CAMBIAR_ESTADO_PEDIDO },
    { "OP_LISTAR_USUARIOS", OP_LISTAR_USUARIOS },
    { "OP_MODIFICAR_USUARIO", OP_MODIFICAR_USUARIO },
    { "OP_ELIMINAR_USUARIO", OP_ELIMINAR_USUARIO },
    { "OP_OBTENER_USUARIO", OP_OBTENER_USUARIO },
    { "OP_MODIFICAR_PRODUCTO", OP_MODIFICAR_PRODUCTO },
    { "OP_ELIMINAR_PRODUCTO", OP_ELIMINAR_PRODUCTO },
    { "OP_CREAR_PRODUCTO", OP_CREAR_PRODUCTO },
    { "OP_OBTENER_PRODUCTO", OP_OBTENER_PRODUCTO },
    { "OP_LISTAR_INGREDIENTES", OP_LISTAR_INGREDIENTES },
    { "OP_CREAR_INGREDIENTE", OP_CREAR_INGREDIENTE },
    { "OP_MODIFICAR_INGREDIENTE", OP_MODIFICAR_INGREDIENTE },
    { "OP_ELIMINAR_INGREDIENTE", OP_ELIMINAR_INGREDIENTE },
    { "OP_OBTENER_INGREDIENTE", OP_OBTENER_INGREDIENTE },
    { "OP_ELIMINAR_PEDIDO", OP_ELIMINAR_PEDIDO },
    { "OP_OBTENER_PEDIDO", OP_OBTENER_PEDIDO },
    { "OP_VERIFICAR_DISPONIBILIDAD_PRODUCTO", OP_VERIFICAR_DISPONIBILIDAD_PRODUCTO },
    { "OP_LISTAR_LOGS", OP_LISTAR_LOGS },
    { "OP_VER_LOG", OP_VER_LOG },
};

} // namespace

Servidor::Servidor(QObject *padre)
    : QObject(padre)
    , m_servidor(QStringLiteral("Sistema de Comandas"), QWebSocketServer::NonSecureMode)
{
    connect(&m_servidor, &QWebSocketServer::newConnection, this, &Servidor::conexionNueva);
}

bool Servidor::iniciar(quint16 puerto)
{
    if (!m_servidor.listen(QHostAddress::Any, puerto)) {
        qCritical().noquote() << "No se pudo escuchar en el puerto" << puerto << ":" << m_servidor.errorString();
        return false;
    }
    qInfo().noquote() << "Servidor escuchando en el puerto" << m_servidor.serverPort();
    return true;
}

void Servidor::conexionNueva()
{
    while (QWebSocket *socket = m_servidor.nextPendingConnection()) {
        socket->setMaxAllowedIncomingMessageSize(TAM_MAXIMO_MENSAJE);
        connect(socket, &QWebSocket::textMessageReceived, this,
                [this, socket](const QString &texto) { mensajeRecibido(socket, texto); });
        connect(socket, &QWebSocket::disconnected, this, [this, socket] {
            m_sesiones.remove(socket);
            socket->deleteLater();
        });
    }
}

void Servidor::mensajeRecibido(QWebSocket *socket, const QString &texto)
{
    const QJsonDocument documento = QJsonDocument::fromJson(texto.toUtf8());
    const QJsonObject peticion = documento.object();
    const QString op = peticion.value("op").toString();

    QJsonObject respuesta;
    if (!documento.isObject() || !peticion.contains("id") || op.isEmpty())
        respuesta = respuestaError("PETICION_INVALIDA", "La petición debe ser JSON con id y op");
    else if (!OPERACIONES.contains(op))
        respuesta = respuestaError("OP_DESCONOCIDA", "Operación desconocida: " + op);
    else
        respuesta = procesarPeticion(OPERACIONES.value(op), peticion.value("datos").toObject(), socket);

    respuesta.insert("id", peticion.value("id"));
    socket->sendTextMessage(QString::fromUtf8(QJsonDocument(respuesta).toJson(QJsonDocument::Compact)));
}

// Es el switch de procesarPeticion en Servidor.c. A diferencia del original, se
// revisa quién llama: sin sesión solo se puede hacer login.
QJsonObject Servidor::procesarPeticion(TipoOperacion operacion, const QJsonObject &datos, QWebSocket *socket)
{
    if (operacion != OP_LOGIN && !m_sesiones.contains(socket))
        return respuestaError("NO_AUTENTICADO", "Primero inicia sesión");

    switch (operacion) {
    case OP_LOGIN:
        return login(datos, socket);

    case OP_LOGOUT:
        m_sesiones.remove(socket);
        return respuestaOk();

    case OP_LISTAR_PRODUCTOS:
        return listarProductos();

    default:
        return respuestaError("NO_IMPLEMENTADA", "Esta operación todavía no está en el servidor");
    }
}

QJsonObject Servidor::login(const QJsonObject &datos, QWebSocket *socket)
{
    const QString usuario = datos.value("usuario").toString().trimmed();
    const QByteArray contrasena = datos.value("contrasena").toString().toUtf8();

    QSqlQuery consulta;
    consulta.prepare(R"(
        SELECT u.id_usuario, u.nombre, u.usuario, u.fecha_nacimiento::text, u.rol, u.contrasena_hash,
               m.turno_asignado, b.estacion_asignada, c.nivel_acceso, c.caja_asignada
        FROM usuario u
        LEFT JOIN mesero m ON m.id_usuario = u.id_usuario
        LEFT JOIN bar_cocina b ON b.id_usuario = u.id_usuario
        LEFT JOIN cajero_admin c ON c.id_usuario = u.id_usuario
        WHERE lower(u.usuario) = lower(:usuario))");
    consulta.bindValue(":usuario", usuario);
    if (!ejecutar(consulta))
        return errorDeBase();

    // El mismo mensaje si no existe o si la contraseña no coincide
    const QByteArray hash = consulta.next() ? consulta.value("contrasena_hash").toByteArray() : QByteArray();
    if (hash.isEmpty()
        || crypto_pwhash_str_verify(hash.constData(), contrasena.constData(),
                                    static_cast<unsigned long long>(contrasena.size())) != 0) {
        qInfo().noquote() << "Login rechazado:" << usuario;
        return respuestaError("CREDENCIALES_INVALIDAS", "Usuario o contraseña incorrectos");
    }

    Sesion sesion;
    sesion.idUsuario = consulta.value("id_usuario").toInt();
    sesion.usuario = consulta.value("usuario").toString();
    sesion.rol = consulta.value("rol").toString();
    sesion.nivelAcceso = consulta.value("nivel_acceso").toString();
    m_sesiones.insert(socket, sesion);
    qInfo().noquote() << "Login exitoso:" << sesion.usuario << "(" << sesion.rol << ")";

    // Los mismos campos que usuarioActual en la Api simulada; el hash nunca sale
    QJsonObject perfil{
        { "id_usuario", sesion.idUsuario },
        { "nombre", consulta.value("nombre").toString() },
        { "usuario", sesion.usuario },
        { "fecha_nacimiento", consulta.value("fecha_nacimiento").toString() },
        { "rol", sesion.rol },
    };
    if (sesion.rol == "mesero") {
        perfil.insert("turno_asignado", consulta.value("turno_asignado").toString());
    } else if (sesion.rol == "bar_cocina") {
        perfil.insert("estacion_asignada", consulta.value("estacion_asignada").toString());
    } else {
        perfil.insert("nivel_acceso", sesion.nivelAcceso);
        perfil.insert("caja_asignada", consulta.value("caja_asignada").toInt());
    }
    return respuestaOk(perfil);
}

// PostgreSQL arma el JSON: así el precio llega como texto ("95.00") y nunca pasa
// por un float. La disponibilidad sale de v_producto, no del código.
QJsonObject Servidor::listarProductos()
{
    QSqlQuery consulta;
    consulta.prepare(R"(
        SELECT COALESCE(json_agg(fila ORDER BY fila.id_producto), '[]')::text
        FROM (
            SELECT p.id_producto, p.nombre, p.descripcion, p.categoria, p.precio::text AS precio,
                   p.id_producto_principal, COALESCE(principal.nombre, '') AS variante_de,
                   p.disponible,
                   COALESCE((SELECT json_agg(i.nombre ORDER BY i.nombre)
                             FROM receta r
                             JOIN ingrediente i ON i.id_ingrediente = r.id_ingrediente
                             WHERE r.id_producto = p.id_producto), '[]') AS ingredientes,
                   COALESCE((SELECT FLOOR(MIN(i.stock_actual / r.cantidad_por_porcion))::integer
                             FROM receta r
                             JOIN ingrediente i ON i.id_ingrediente = r.id_ingrediente
                             WHERE r.id_producto = p.id_producto), 99) AS maximo
            FROM v_producto p
            LEFT JOIN producto principal ON principal.id_producto = p.id_producto_principal
        ) fila)");
    if (!ejecutar(consulta) || !consulta.next())
        return errorDeBase();
    return respuestaOk(QJsonDocument::fromJson(consulta.value(0).toByteArray()).array());
}

// Si la base se reinició, la conexión queda rota: esta petición falla, pero se
// reconecta para que la siguiente ya funcione.
bool Servidor::ejecutar(QSqlQuery &consulta)
{
    if (consulta.exec())
        return true;

    qWarning().noquote() << "Error en la base:" << consulta.lastError().text();
    QSqlDatabase base = QSqlDatabase::database();
    if (!QSqlQuery(base).exec(QStringLiteral("SELECT 1"))) {
        base.close();
        if (base.open())
            qInfo() << "Reconectado a la base";
        else
            qWarning().noquote() << "Sigue sin conexión:" << base.lastError().text();
    }
    return false;
}
