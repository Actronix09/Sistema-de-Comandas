import QtQuick
import QtQuick.Controls

// Modo de verificación: recorre las pantallas y guarda una captura de cada una.
//   qml6 -I . Main.qml -- --capturas /carpeta          (solo el contenido: sin diálogos)
//   python3 anfitrion/main.py --capturas /carpeta      (ventana completa, con diálogos)
// La carpeta debe existir.
Item {
    id: capturas

    required property var raiz              // la ventana de Main.qml
    required property StackView pilaPrincipal
    required property var capturador        // objeto del anfitrión Python, o null

    readonly property string carpeta: {
        const args = Qt.application.arguments
        const i = args.indexOf("--capturas")
        return i >= 0 && i + 1 < args.length ? args[i + 1] : "."
    }
    property int paso: 0

    function guardar(nombre) {
        const ruta = carpeta + "/" + nombre + ".png"
        if (capturador) {
            capturador.guardar(ruta)
            return
        }
        // Con qml6 solo se puede capturar algo que haya creado el motor QML:
        // la pila de pantallas sirve; los diálogos viven fuera de ella.
        pilaPrincipal.grabToImage(function (resultado) {
            resultado.saveToFile(ruta)
        })
    }

    function entrar(usuario) {
        raiz.cerrarSesion()
        Api.iniciarSesion(usuario, Api.contrasenaDemo, function (r) {
            if (r.codigo === "OK")
                capturas.raiz.abrirInicioPorRol(r.datos)
        })
    }

    function mesero() { return pilaPrincipal.currentItem as PantallaMesero }
    function admin() { return pilaPrincipal.currentItem as PantallaAdmin }

    readonly property var pasos: [
        function () { capturas.guardar("01-login") },
        function () { capturas.entrar("lmendez") },
        function () { capturas.guardar("02-mesero-catalogo") },
        function () { capturas.mesero().demoLlenarCarrito() },
        function () { capturas.guardar("03-mesero-orden") },
        function () { capturas.mesero().demoConfirmar() },
        function () { },
        function () { capturas.guardar("04-confirmar-orden") },
        function () { capturas.mesero().demoEnviar() },
        function () { capturas.mesero().mostrarPestana(1) },
        function () { },
        function () { capturas.guardar("05-mis-ordenes") },
        function () { capturas.entrar("rgamez") },
        function () { },
        function () { capturas.guardar("06-cocina") },
        function () { capturas.entrar("isoto") },
        function () { },
        function () { capturas.guardar("07-bar") },
        function () { capturas.entrar("mrivas") },
        function () { capturas.guardar("08-admin") },
        function () { capturas.admin().abrirSeccion("ingredientes") },
        function () { },
        function () { capturas.guardar("09-admin-ingredientes") },
        function () { capturas.admin().demoNuevoIngrediente() },
        function () { capturas.guardar("10-dialogo-ingrediente") },
        function () { capturas.raiz.cerrarSesion(); capturas.raiz.abrirPendiente("crear_usuario") },
        function () { capturas.guardar("11-por-construir") },
        function () { },
        function () { Qt.quit() }
    ]

    Timer {
        interval: 700
        running: true
        repeat: true
        onTriggered: {
            if (capturas.paso < capturas.pasos.length) {
                console.info("Capturas: paso", capturas.paso + 1, "de", capturas.pasos.length)
                capturas.pasos[capturas.paso++]()
            }
        }
    }

    // Por si algo se atora
    Timer {
        interval: 60000
        running: true
        onTriggered: {
            console.warn("Capturas: tiempo agotado")
            Qt.quit()
        }
    }
}
