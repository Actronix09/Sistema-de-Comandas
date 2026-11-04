#ifndef SERVIDOR_H
#define SERVIDOR_H

#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QWebSocketServer>

#include "protocolo.h"

class QSqlQuery;
class QWebSocket;

// Lo que el servidor recuerda de cada conexión después de OP_LOGIN
struct Sesion
{
    int idUsuario = 0;
    QString usuario;
    QString rol;
    QString nivelAcceso;
};

// Reemplaza a Servidor.c: en lugar de memoria compartida y semáforos, cada
// cliente manda sus peticiones en JSON por un WebSocket.
class Servidor : public QObject
{
    Q_OBJECT

public:
    explicit Servidor(QObject *padre = nullptr);

    bool iniciar(quint16 puerto);

private:
    void conexionNueva();
    void mensajeRecibido(QWebSocket *socket, const QString &texto);
    QJsonObject procesarPeticion(TipoOperacion operacion, const QJsonObject &datos, QWebSocket *socket);

    QJsonObject login(const QJsonObject &datos, QWebSocket *socket);
    QJsonObject listarProductos();

    bool ejecutar(QSqlQuery &consulta);

    QWebSocketServer m_servidor;
    QHash<QWebSocket *, Sesion> m_sesiones;
};

#endif
