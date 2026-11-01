#!/bin/bash
# Compila el APK, arranca el emulador, instala el cliente y deja en /salida una
# captura de pantalla y el log de la app. Termina con error si la app se cae.
set -euo pipefail

PAQUETE=org.qtproject.example.cliente_comandas

"$QT_ANDROID/bin/qt-cmake" -S /src/cliente -B /tmp/build -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DQT_HOST_PATH="$QT_HOST" \
    -DANDROID_SDK_ROOT="$ANDROID_SDK_ROOT" \
    -DANDROID_NDK_ROOT="$ANDROID_NDK_ROOT"
cmake --build /tmp/build --target apk
APK=$(find /tmp/build -name '*.apk' | head -n 1)
cp "$APK" /salida/cliente_comandas.apk

# Con swiftshader_indirect los botones de Material no se dibujan
emulator -avd comandas -no-window -no-audio -no-boot-anim -no-snapshot \
    -gpu swangle_indirect >/salida/emulador.log 2>&1 &
EMULADOR=$!
trap 'kill "$EMULADOR" 2>/dev/null || true' EXIT

adb wait-for-device
for _ in $(seq 150); do
    [ "$(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r')" = "1" ] && break
    sleep 2
done
[ "$(adb shell getprop sys.boot_completed | tr -d '\r')" = "1" ] || { echo "El emulador no terminó de arrancar"; exit 1; }

adb install -r -t "$APK"
adb logcat -c
adb shell am start -W -n "$PAQUETE/org.qtproject.qt.android.bindings.QtActivity"
sleep "${ESPERA_APP:-20}"

adb exec-out screencap -p >/salida/captura.png
adb logcat -d >/salida/logcat.txt

if ! adb shell pidof "$PAQUETE" >/dev/null; then
    echo "El cliente se cerró; revisa /salida/logcat.txt"
    exit 1
fi
echo "El cliente está corriendo en el emulador; captura en /salida/captura.png"
