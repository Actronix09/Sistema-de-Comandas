pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import Comandas

// Prototipo de la interfaz del Sistema de Comandas (datos simulados).
// Ejecutar:  ./ejecutar.sh [Material|FluentWinUI3|Universal|Fusion|Basic] [claro|oscuro]
//
// Esta ventana solo navega: cada pantalla avisa con señales (sesionIniciada,
// salir, volver...) y aquí se decide a dónde ir, como hacía el main() de Cliente.c.
ApplicationWindow {
    id: ventana

    width: 1280
    height: 800
    minimumWidth: 1024
    minimumHeight: 640
    visible: true
    title: "Sistema de Comandas · prototipo"
    color: Tema.fondo
    font.pixelSize: Tema.letraNormal

    // Lo pone el anfitrión de Python en el modo --capturas; con qml6 queda en null.
    property var capturador: null

    // Paleta para los estilos que la respetan (Basic, Fusion, FluentWinUI3).
    // Material y Universal toman su tema de las variables que fija ejecutar.sh.
    palette.window: Tema.fondo
    palette.windowText: Tema.texto
    palette.base: Tema.superficie
    palette.alternateBase: Tema.superficieAlta
    palette.text: Tema.texto
    palette.button: Tema.superficieAlta
    palette.buttonText: Tema.texto
    palette.brightText: "#ffffff"
    palette.highlight: Tema.acento
    palette.highlightedText: Tema.textoSobreAcento
    palette.accent: Tema.acento
    palette.link: Tema.info
    palette.placeholderText: Tema.textoSuave
    palette.toolTipBase: Tema.superficieAlta
    palette.toolTipText: Tema.texto
    palette.light: Tema.superficie
    palette.midlight: Tema.superficieAlta
    palette.mid: Tema.borde
    palette.dark: Qt.darker(Tema.borde, 1.4)
    palette.shadow: "#000000"
    // Sin esto, un control deshabilitado se vería igual que uno activo
    palette.disabled.buttonText: Tema.textoSuave
    palette.disabled.text: Tema.textoSuave
    palette.disabled.windowText: Tema.textoSuave
    palette.disabled.highlightedText: Tema.textoSuave

    // Equivale al switch de Cliente.c que elegía la interfaz según el tipo de usuario
    function abrirInicioPorRol(usuario) {
        switch (usuario.rol) {
        case "mesero":
            pila.replace(null, cMesero)
            break
        case "bar_cocina":
            pila.replace(null, cCocina)
            break
        case "cajero_admin":
            pila.replace(null, cAdmin)
            break
        default:
            Avisos.error("Rol sin interfaz: " + usuario.rol)
        }
    }

    function cerrarSesion() {
        Api.cerrarSesion()
        pila.replace(null, cLogin)
    }

    function abrirPendiente(clave) {
        pila.push(cPendiente, { clave: clave })
    }

    StackView {
        id: pila
        anchors.fill: parent
        initialItem: cLogin
    }

    Component {
        id: cLogin
        PantallaLogin {
            onSesionIniciada: (usuario) => ventana.abrirInicioPorRol(usuario)
            onAbrirPendiente: (clave) => ventana.abrirPendiente(clave)
        }
    }

    Component {
        id: cMesero
        PantallaMesero {
            onSalir: ventana.cerrarSesion()
        }
    }

    Component {
        id: cCocina
        PantallaCocina {
            onSalir: ventana.cerrarSesion()
        }
    }

    Component {
        id: cAdmin
        PantallaAdmin {
            onSalir: ventana.cerrarSesion()
        }
    }

    Component {
        id: cPendiente
        PantallaPendiente {
            onVolver: pila.pop()
        }
    }

    Aviso {
        id: aviso
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 28
        z: 10
    }

    Connections {
        target: Avisos
        function onMostrar(texto, tipo) { aviso.mostrar(texto, tipo) }
    }

    Loader {
        active: Qt.application.arguments.indexOf("--capturas") >= 0
        sourceComponent: Capturas {
            raiz: ventana
            pilaPrincipal: pila
            capturador: ventana.capturador
        }
    }
}
