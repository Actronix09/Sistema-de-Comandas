#!/usr/bin/env sh
# Ejecuta el prototipo con el estilo y el tema elegidos.
#
#   ./ejecutar.sh                      Material, claro
#   ./ejecutar.sh FluentWinUI3 oscuro
#   ./ejecutar.sh Fusion
#
# Estilos: Material, FluentWinUI3, Universal, Fusion, Basic, Imagine.
# Usa qml6: el comando "qml" de este sistema es el de Qt 5.15.
set -e

DIR="$(cd "$(dirname "$0")" && pwd)"
ESTILO="${1:-Material}"
TEMA="${2:-claro}"

export QT_QUICK_CONTROLS_STYLE="$ESTILO"

if [ "$TEMA" = "oscuro" ]; then
    export QT_QUICK_CONTROLS_MATERIAL_THEME=Dark
    export QT_QUICK_CONTROLS_UNIVERSAL_THEME=Dark
    export QT_QUICK_CONTROLS_MATERIAL_ACCENT="#FF8A4C"
    export QT_QUICK_CONTROLS_UNIVERSAL_ACCENT="#FF8A4C"
    exec qml6 -I "$DIR" "$DIR/Main.qml" -- --oscuro
else
    export QT_QUICK_CONTROLS_MATERIAL_THEME=Light
    export QT_QUICK_CONTROLS_UNIVERSAL_THEME=Light
    export QT_QUICK_CONTROLS_MATERIAL_ACCENT="#D9531E"
    export QT_QUICK_CONTROLS_UNIVERSAL_ACCENT="#D9531E"
    exec qml6 -I "$DIR" "$DIR/Main.qml"
fi
