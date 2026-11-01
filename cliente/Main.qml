pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import Comandas

// Ventana principal del Sistema de Comandas.
//
// Esta ventana solo navega: cada pantalla avisa con señales (sesionIniciada,
// salir, volver...) y aquí se decide a dónde ir, como hacía el main() de Cliente.c.
ApplicationWindow {
    id: ventana

    width: 1280
    height: 800
    // Permite achicarla a tamaño de teléfono para probar las pantallas compactas
    minimumWidth: 360
    minimumHeight: 600
    visible: true
    title: "Sistema de Comandas"
    color: Tema.fondo
    font.pixelSize: Tema.letraNormal

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
}
