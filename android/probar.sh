#!/bin/bash
# Compila el APK para ARM64, arranca el emulador, instala el cliente y deja en
# /salida una captura de pantalla y el log de la app. Termina con error si la app
# se cae o si no quedó instalada como ARM64.
#
# Con VENTANA=1 abre una ventana con scrcpy para usar la app con el mouse; termina
# al cerrar la ventana.
#
# Con SOLO_APK=1 termina al tener el APK, sin arrancar el emulador.
set -euo pipefail

PAQUETE=org.qtproject.example.cliente_comandas

# Solo se conserva la clave de firma: si el volumen guardara todo ~/.android, también
# quedarían los bloqueos del emulador y no volvería a arrancar tras un cierre brusco.
mkdir -p ~/.android
[ -f /clave/debug.keystore ] && cp /clave/debug.keystore ~/.android/

"$QT_ANDROID/bin/qt-cmake" -S /src/cliente -B /tmp/build -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DQT_HOST_PATH="$QT_HOST" \
    -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" \
    -DANDROID_NDK_ROOT="$ANDROID_NDK_ROOT"
cmake --build /tmp/build --target apk
APK=$(find /tmp/build -name '*.apk' | head -n 1)
cp "$APK" /salida/cliente_comandas.apk
[ -f /clave/debug.keystore ] || cp ~/.android/debug.keystore /clave/

if [ "${SOLO_APK:-0}" = 1 ]; then
    echo "APK listo en /salida/cliente_comandas.apk"
    exit 0
fi

# Por omisión dibuja por software, porque funciona en cualquier equipo. Con
# swiftshader_indirect los botones de Material no se dibujan.
emulator -avd comandas -no-window -no-audio -no-boot-anim -no-snapshot \
    ${EMULADOR_GRAFICOS:--gpu swangle_indirect} >/salida/emulador.log 2>&1 &
EMULADOR=$!
trap 'kill "$EMULADOR" 2>/dev/null || true' EXIT

timeout 120 adb wait-for-device || { echo "El emulador no arrancó; revisa /salida/emulador.log"; exit 1; }
for _ in $(seq 150); do
    [ "$(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" = "1" ] && break
    sleep 2
done
[ "$(adb shell getprop sys.boot_completed | tr -d '\r')" = "1" ] || { echo "El emulador no terminó de arrancar"; exit 1; }

adb install -r -t "$APK"
adb shell dumpsys package "$PAQUETE" | grep -q 'primaryCpuAbi=arm64-v8a' \
    || { echo "El cliente no se instaló como ARM64"; exit 1; }
adb logcat -c
adb shell am start -W -n "$PAQUETE/org.qtproject.qt.android.bindings.QtActivity"

if [ "${VENTANA:-0}" = 1 ]; then
    adb shell settings put global window_animation_scale 0
    adb shell settings put global transition_animation_scale 0
    adb shell settings put global animator_duration_scale 0
    echo "Listo: usa la app en la ventana que se abre. Termina al cerrarla."
    # Con OpenGL, dentro del contenedor scrcpy se queda sin abrir la ventana
    LIBGL_ALWAYS_SOFTWARE=1 SDL_VIDEODRIVER=x11 /opt/scrcpy/scrcpy --no-audio \
        --render-driver=software --max-size=1024 --max-fps=30 \
        --window-title "Comandas en Android"
    exit 0
fi

sleep "${ESPERA_APP:-20}"

adb exec-out screencap -p >/salida/captura.png
adb logcat -d >/salida/logcat.txt

if ! adb shell pidof "$PAQUETE" >/dev/null; then
    echo "El cliente se cerró; revisa /salida/logcat.txt"
    exit 1
fi
echo "El cliente está corriendo en el emulador; captura en /salida/captura.png"
