pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Menú principal + "Iniciar Sesion" del sistema en terminal. "Crear Usuario" y
// "Recuperar Contrasena" siguen aquí; esas pantallas son ejercicios tuyos.
Item {
    id: pantalla

    signal sesionIniciada(var usuario)
    signal abrirPendiente(string clave)

    property bool ocupado: false
    property string error: ""
    property var evento: null

    function entrar() {
        if (ocupado)
            return
        if (campoUsuario.text.trim() === "") {
            error = "Escribe tu usuario"
            campoUsuario.forceActiveFocus()
            return
        }
        ocupado = true
        error = ""
        Api.iniciarSesion(campoUsuario.text, campoContrasena.text, function (r) {
            if (!pantalla)
                return
            pantalla.ocupado = false
            if (r.codigo === "OK") {
                Avisos.exito(r.mensaje)
                pantalla.sesionIniciada(r.datos)
            } else {
                pantalla.error = r.mensaje
                campoContrasena.selectAll()
                campoContrasena.forceActiveFocus()
            }
        })
    }

    function entrarComo(usuario) {
        campoUsuario.text = usuario
        campoContrasena.text = Api.contrasenaDemo
        entrar()
    }

    Component.onCompleted: {
        Api.eventoEnCurso(function (r) {
            if (pantalla && r.codigo === "OK")
                pantalla.evento = r.datos
        })
        campoUsuario.forceActiveFocus()
    }

    Rectangle {
        anchors.fill: parent
        color: Tema.fondo
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ---------------- Panel de marca ----------------
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: pantalla.width * 0.44
            // En pantallas angostas solo queda el formulario
            visible: pantalla.width >= 760
            gradient: Gradient {
                GradientStop { position: 0.0; color: Tema.acento }
                GradientStop { position: 1.0; color: Qt.darker(Tema.acento, 1.7) }
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 48
                spacing: 14

                Item { Layout.fillHeight: true }

                Rectangle {
                    implicitWidth: 64
                    implicitHeight: 64
                    radius: 18
                    color: Qt.rgba(1, 1, 1, 0.18)
                    Label {
                        anchors.centerIn: parent
                        text: "C"
                        color: "#ffffff"
                        font.pixelSize: 34
                        font.bold: true
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: "Sistema de Comandas"
                    color: "#ffffff"
                    font.pixelSize: 36
                    font.bold: true
                    wrapMode: Text.WordWrap
                }
                Label {
                    Layout.fillWidth: true
                    text: "Salón de eventos · meseros, cocina, bar y administración"
                    color: Qt.rgba(1, 1, 1, 0.85)
                    font.pixelSize: 16
                    wrapMode: Text.WordWrap
                }

                Rectangle {
                    visible: pantalla.evento !== null
                    Layout.fillWidth: true
                    Layout.topMargin: 24
                    implicitHeight: datosEvento.implicitHeight + 32
                    radius: 14
                    color: Qt.rgba(1, 1, 1, 0.14)

                    ColumnLayout {
                        id: datosEvento
                        anchors.fill: parent
                        anchors.margins: 16
                        spacing: 4
                        Label {
                            text: "EVENTO EN CURSO"
                            color: Qt.rgba(1, 1, 1, 0.75)
                            font.pixelSize: 11
                            font.bold: true
                            font.letterSpacing: 1.2
                        }
                        Label {
                            text: pantalla.evento ? "#" + pantalla.evento.numero_evento + " · " + pantalla.evento.tipo_evento : ""
                            color: "#ffffff"
                            font.pixelSize: 22
                            font.bold: true
                        }
                        Label {
                            Layout.fillWidth: true
                            text: pantalla.evento ? pantalla.evento.numero_asistentes + " asistentes · Organizan "
                                                    + pantalla.evento.organizadores.join(" y ") : ""
                            color: Qt.rgba(1, 1, 1, 0.85)
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                Label {
                    text: "Datos simulados en memoria"
                    color: Qt.rgba(1, 1, 1, 0.6)
                    font.pixelSize: Tema.letraChica
                }
            }
        }

        // ---------------- Formulario ----------------
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.centerIn: parent
                width: Math.min(420, parent.width - (parent.width < 480 ? 32 : 64))
                spacing: 10

                Label {
                    text: "Iniciar sesión"
                    color: Tema.texto
                    font.pixelSize: 30
                    font.bold: true
                }
                Label {
                    text: "Entra con tu usuario del salón."
                    color: Tema.textoSuave
                }

                Label {
                    Layout.topMargin: 12
                    text: "Usuario"
                    color: Tema.texto
                    font.bold: true
                }
                TextField {
                    id: campoUsuario
                    Layout.fillWidth: true
                    placeholderText: "p. ej. lmendez"
                    enabled: !pantalla.ocupado
                    onAccepted: campoContrasena.forceActiveFocus()
                }

                Label {
                    text: "Contraseña"
                    color: Tema.texto
                    font.bold: true
                }
                RowLayout {
                    Layout.fillWidth: true
                    TextField {
                        id: campoContrasena
                        Layout.fillWidth: true
                        echoMode: verContrasena.checked ? TextInput.Normal : TextInput.Password
                        placeholderText: "Tu contraseña"
                        enabled: !pantalla.ocupado
                        onAccepted: pantalla.entrar()
                    }
                    Button {
                        id: verContrasena
                        checkable: true
                        flat: true
                        text: checked ? "Ocultar" : "Mostrar"
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: pantalla.error !== ""
                    text: pantalla.error
                    color: Tema.peligro
                    wrapMode: Text.WordWrap
                }

                Button {
                    Layout.fillWidth: true
                    Layout.topMargin: 6
                    text: pantalla.ocupado ? "Entrando…" : "Entrar"
                    highlighted: true
                    enabled: !pantalla.ocupado
                    onClicked: pantalla.entrar()
                }

                RowLayout {
                    Layout.fillWidth: true
                    Button {
                        text: "Crear usuario"
                        flat: true
                        onClicked: pantalla.abrirPendiente("crear_usuario")
                    }
                    Button {
                        text: "Recuperar contraseña"
                        flat: true
                        onClicked: pantalla.abrirPendiente("recuperar")
                    }
                    Item { Layout.fillWidth: true }
                    Button {
                        text: "Salir"
                        flat: true
                        onClicked: Qt.quit()
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.topMargin: 14
                    implicitHeight: accesoDemo.implicitHeight + 24
                    radius: 12
                    color: Tema.superficieAlta
                    border.color: Tema.borde

                    ColumnLayout {
                        id: accesoDemo
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        anchors.margins: 12
                        spacing: 8

                        Label {
                            text: "Acceso rápido (demo) · contraseña " + Api.contrasenaDemo
                            color: Tema.textoSuave
                            font.pixelSize: Tema.letraChica
                            font.bold: true
                        }
                        Flow {
                            Layout.fillWidth: true
                            spacing: 8
                            Repeater {
                                model: [
                                    { texto: "Mesero · lmendez", usuario: "lmendez" },
                                    { texto: "Mesero · operez", usuario: "operez" },
                                    { texto: "Cocina · rgamez", usuario: "rgamez" },
                                    { texto: "Bar · isoto", usuario: "isoto" },
                                    { texto: "Admin · mrivas", usuario: "mrivas" }
                                ]
                                delegate: Button {
                                    id: botonDemo
                                    required property var modelData
                                    text: botonDemo.modelData.texto
                                    enabled: !pantalla.ocupado
                                    onClicked: pantalla.entrarComo(botonDemo.modelData.usuario)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
