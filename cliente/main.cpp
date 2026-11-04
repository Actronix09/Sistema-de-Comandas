// Cliente del Sistema de Comandas: carga Main.qml desde el módulo Comandas,
// que va compilado dentro del ejecutable.
//
// El estilo (Material, claro) lo fija qtquickcontrols2.conf.
// Más adelante aquí se registrará la Api real en C++ (Fase 7.2) en lugar de
// Api.qml, y las pantallas no cambiarán.
#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    // QSettings los usa para guardar, por ejemplo, la IP del servidor
    QCoreApplication::setOrganizationName(QStringLiteral("Sistema de Comandas"));
    QCoreApplication::setApplicationName(QStringLiteral("Comandas"));

    QQmlApplicationEngine motor;
    QObject::connect(&motor, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    motor.loadFromModule("Comandas", "Main");

    return app.exec();
}
