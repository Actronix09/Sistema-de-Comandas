#ifndef VALIDACIONES_QML_H
#define VALIDACIONES_QML_H

#include <QObject>
#include <QString>
#include <QtQml/qqmlregistration.h>

// Deja usar desde QML las validaciones escritas en C (validaciones.c)
class Validaciones : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    using QObject::QObject;

    Q_INVOKABLE bool contrasenaValida(const QString &contrasena) const;
};

#endif
