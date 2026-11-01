// Servidor del Sistema de Comandas: lo único que toca la base de datos.
//
// Se configura con variables de entorno, nunca con datos escritos en el código:
//   COMANDAS_BD_HOST        (localhost)
//   COMANDAS_BD_PUERTO      (5433)
//   COMANDAS_BD_NOMBRE      (comandas)
//   COMANDAS_BD_USUARIO     obligatoria
//   COMANDAS_BD_CONTRASENA  obligatoria
//   COMANDAS_PUERTO         (9000), donde se conectan los clientes
//
// Con --generar-hash lee una contraseña de la entrada estándar y escribe el hash
// que se guarda en usuario.contrasena_hash.
#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>
#include <QVariant>

#include <csignal>
#include <cstdlib>

#include <sodium.h>

#include "servidor.h"
#include "validaciones.h"

namespace {

int generarHash()
{
    const QByteArray contrasena = QTextStream(stdin).readLine().toUtf8();
    if (validar_password(contrasena.constData()) != 0) {
        qCritical() << "La contraseña necesita 8 caracteres o más, una mayúscula, una minúscula,"
                       " un número y 2 símbolos de * / _ - + .";
        return EXIT_FAILURE;
    }

    char hash[crypto_pwhash_STRBYTES];
    if (crypto_pwhash_str(hash, contrasena.constData(), static_cast<unsigned long long>(contrasena.size()),
                          crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        qCritical() << "No alcanzó la memoria para calcular el hash";
        return EXIT_FAILURE;
    }
    QTextStream(stdout) << hash << Qt::endl;
    return EXIT_SUCCESS;
}

bool conectarBase()
{
    const QString usuario = qEnvironmentVariable("COMANDAS_BD_USUARIO");
    const QString contrasena = qEnvironmentVariable("COMANDAS_BD_CONTRASENA");
    if (usuario.isEmpty() || contrasena.isEmpty()) {
        qCritical() << "Faltan COMANDAS_BD_USUARIO o COMANDAS_BD_CONTRASENA";
        return false;
    }

    QSqlDatabase base = QSqlDatabase::addDatabase(QStringLiteral("QPSQL"));
    base.setHostName(qEnvironmentVariable("COMANDAS_BD_HOST", QStringLiteral("localhost")));
    base.setPort(qEnvironmentVariable("COMANDAS_BD_PUERTO", QStringLiteral("5433")).toInt());
    base.setDatabaseName(qEnvironmentVariable("COMANDAS_BD_NOMBRE", QStringLiteral("comandas")));
    base.setUserName(usuario);
    base.setPassword(contrasena);
    if (!base.open()) {
        qCritical().noquote() << "No se pudo conectar a la base:" << base.lastError().text();
        return false;
    }

    // De paso comprueba que el esquema ya esté cargado
    QSqlQuery consulta(base);
    if (!consulta.exec(QStringLiteral("SELECT count(*) FROM v_cuenta")) || !consulta.next()) {
        qCritical().noquote() << "La base no tiene el esquema:" << consulta.lastError().text();
        return false;
    }
    qInfo().noquote() << "Conectado a" << base.databaseName() << "en" << base.hostName()
                      << "con" << consulta.value(0).toInt() << "cuentas";
    return true;
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    if (sodium_init() < 0) {
        qCritical() << "No se pudo iniciar libsodium";
        return EXIT_FAILURE;
    }
    if (app.arguments().contains(QStringLiteral("--generar-hash")))
        return generarHash();

    // Dentro de un contenedor es el proceso 1 y sin esto ignora la orden de parar.
    // No hay nada que guardar: todo lo importante ya está en la base.
    std::signal(SIGTERM, [](int) { std::_Exit(EXIT_SUCCESS); });

    if (!conectarBase())
        return EXIT_FAILURE;

    Servidor servidor;
    const int puerto = qEnvironmentVariable("COMANDAS_PUERTO", QStringLiteral("9000")).toInt();
    if (!servidor.iniciar(static_cast<quint16>(puerto)))
        return EXIT_FAILURE;

    return app.exec();
}
