#include "validaciones_qml.h"

#include "validaciones.h"

bool Validaciones::contrasenaValida(const QString &contrasena) const
{
    return validar_password(contrasena.toUtf8().constData()) == 0;
}
