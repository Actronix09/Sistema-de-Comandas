pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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

    // Botón Atrás de Android: primero retrocede dentro de la pantalla actual. Regresa
    // false solo en el login, y entonces la app se cierra como cualquier otra.
    function atras() {
        const actual = pila.currentItem
        if (actual.atras && actual.atras())
            return true
        if (pila.depth > 1) {
            pila.pop()
            return true
        }
        if (Api.usuarioActual !== null) {
            confirmarSalida.open()
            return true
        }
        return false
    }

    // En Android, Atrás llega aquí; en escritorio es cerrar la ventana y no se detiene
    onClosing: (cierre) => {
        if (Qt.platform.os === "android" && ventana.atras())
            cierre.accepted = false
    }

    // La demo pública lo dice siempre, en todas las pantallas: cualquiera puede
    // entrar y lo que se haga se borra cada noche
    // Todo lo que se ve, en un elemento propio: la raíz de la ventana no se puede
    // capturar desde QML y el recorrido de capturas (Capturas.qml) captura esto
    Item {
        id: lienzo
        anchors.fill: parent

        Rectangle {
            id: letreroDemo
            visible: Api.modoDemo
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: visible ? textoDemo.implicitHeight + 10 : 0
            color: Tema.advertencia
            z: 5

            Label {
                id: textoDemo
                anchors.centerIn: parent
                width: parent.width - 2 * Tema.margen
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                text: ventana.width < 600
                      ? "DEMO · se reinicia cada noche"
                      : "DEMO PÚBLICA · Cualquiera puede entrar con las cuentas de prueba · Los datos se reinician cada noche a las 3:30"
                color: Tema.oscuro ? "#1a1205" : "#ffffff"
                font.pixelSize: Tema.letraChica
                font.bold: true
            }
        }

        StackView {
            id: pila
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: letreroDemo.bottom
            anchors.bottom: parent.bottom
            initialItem: cLogin
        }
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

    DialogoConfirmar {
        id: confirmarSalida
        titulo: "¿Cerrar sesión?"
        mensaje: "Si tienes una orden sin confirmar, se perderá."
        textoAceptar: "Cerrar sesión"
        onConfirmado: ventana.cerrarSesion()
    }

    // Un celular acostado deja muy poco alto (~340 px) para las listas: la app
    // se usa en vertical en el celular. En tablet y computadora sobra alto.
    Rectangle {
        anchors.fill: parent
        visible: ventana.height < 460 && ventana.width > ventana.height
        z: 20
        color: Tema.fondo
        MouseArea { anchors.fill: parent }    // que no pase nada a la pantalla de atrás

        ColumnLayout {
            anchors.centerIn: parent
            width: Math.min(420, parent.width - 48)
            spacing: 10
            Label {
                Layout.alignment: Qt.AlignHCenter
                text: "↻"
                color: Tema.acento
                font.pixelSize: 56
            }
            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: "Gira tu celular"
                color: Tema.texto
                font.pixelSize: Tema.letraTitulo
                font.bold: true
            }
            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: "En el celular, el Sistema de Comandas se usa en vertical."
                color: Tema.textoSuave
                wrapMode: Text.WordWrap
            }
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

    // Recorrido de capturas para revisar el diseño (solo con --capturas)
    Loader {
        active: Qt.application.arguments.indexOf("--capturas") >= 0
        sourceComponent: Capturas {
            ventana: ventana
            pila: pila
            lienzo: lienzo
        }
    }

    // La sesión vive en la conexión: si se cae, el servidor ya no la recuerda
    Connections {
        target: Api
        function onSesionPerdida() {
            pila.replace(null, cLogin)
            Avisos.error("Se perdió la conexión con el servidor; vuelve a entrar")
        }
    }
}
