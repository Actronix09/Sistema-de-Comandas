// Anfitrión en C++ del prototipo QML: carga el mismo Main.qml que ejecutar.sh.
//
// En el sistema real, aquí se registraría la Api de verdad (una clase QObject
// con QML_ELEMENT y QML_SINGLETON que hable HTTP y WebSocket) para que las
// pantallas la usen en lugar de ApiSimulada.qml.
#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QUrl>

static bool tieneArgumento(int argc, char *argv[], const char *buscado)
{
    for (int i = 1; i < argc; ++i)
        if (qstrcmp(argv[i], buscado) == 0)
            return true;
    return false;
}

int main(int argc, char *argv[])
{
    // Las variables de estilo se leen al crear la aplicación, así que van antes
    const bool oscuro = tieneArgumento(argc, argv, "--oscuro");

    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE"))
        qputenv("QT_QUICK_CONTROLS_STYLE", "Material");
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_MATERIAL_THEME"))
        qputenv("QT_QUICK_CONTROLS_MATERIAL_THEME", oscuro ? "Dark" : "Light");
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_MATERIAL_ACCENT"))
        qputenv("QT_QUICK_CONTROLS_MATERIAL_ACCENT", oscuro ? "#FF8A4C" : "#D9531E");

    QGuiApplication app(argc, argv);

    QQmlApplicationEngine motor;
    motor.addImportPath(QStringLiteral(DIR_QML));
    QObject::connect(&motor, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    motor.load(QUrl::fromLocalFile(QStringLiteral(DIR_QML "/Main.qml")));

    return app.exec();
}
