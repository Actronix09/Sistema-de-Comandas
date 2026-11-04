"""Anfitrión en Python del prototipo QML.

Carga exactamente el mismo Main.qml que ejecutar.sh. Usa PySide6 si está
instalado (la opción recomendada para el sistema real: licencia LGPL) y, si
no, el PyQt6 del sistema (GPL), que alcanza para esta prueba.

Uso:
    python3 main.py                         # Material, claro
    python3 main.py --oscuro
    QT_QUICK_CONTROLS_STYLE=Fusion python3 main.py
    python3 main.py --capturas /carpeta     # guarda una captura de cada pantalla

En el sistema real, aquí es donde se registraría la Api de verdad (cliente
HTTP + WebSocket) para que las pantallas la usen en lugar de ApiSimulada.
"""

import os
import sys
from pathlib import Path

try:
    from PySide6.QtCore import QObject, QUrl, Slot
    from PySide6.QtGui import QGuiApplication
    from PySide6.QtQml import QQmlApplicationEngine
    from PySide6.QtQuick import QQuickWindow

    ENLACE = "PySide6"
except ImportError:
    from PyQt6.QtCore import QObject, QUrl
    from PyQt6.QtCore import pyqtSlot as Slot
    from PyQt6.QtGui import QGuiApplication
    from PyQt6.QtQml import QQmlApplicationEngine
    from PyQt6.QtQuick import QQuickWindow

    ENLACE = "PyQt6"

DIR_QML = Path(__file__).resolve().parent.parent


class Capturador(QObject):
    """Guarda la ventana completa (con diálogos) para el modo --capturas."""

    def __init__(self, motor: QQmlApplicationEngine) -> None:
        super().__init__()
        self._motor = motor

    @Slot(str)
    def guardar(self, ruta: str) -> None:
        ventana = self._motor.rootObjects()[0]
        if isinstance(ventana, QQuickWindow):
            ventana.grabWindow().save(ruta)


def main() -> int:
    oscuro = "--oscuro" in sys.argv
    os.environ.setdefault("QT_QUICK_CONTROLS_STYLE", "Material")
    os.environ.setdefault("QT_QUICK_CONTROLS_MATERIAL_THEME", "Dark" if oscuro else "Light")
    os.environ.setdefault("QT_QUICK_CONTROLS_MATERIAL_ACCENT", "#FF8A4C" if oscuro else "#D9531E")

    app = QGuiApplication(sys.argv)
    motor = QQmlApplicationEngine()
    motor.addImportPath(str(DIR_QML))

    capturador = Capturador(motor)
    if "--capturas" in sys.argv:
        motor.setInitialProperties({"capturador": capturador})

    motor.load(QUrl.fromLocalFile(str(DIR_QML / "Main.qml")))
    if not motor.rootObjects():
        print("No se pudo cargar Main.qml", file=sys.stderr)
        return 1

    print(f"Prototipo cargado con {ENLACE} · estilo {os.environ['QT_QUICK_CONTROLS_STYLE']}")
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
