pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// "Administrador de usuarios": el mismo patrón que AdminIngredientes.
//   lista                   → Api.listarUsuarios
//   alta y edición          → DialogoUsuario (el servidor valida)
//   baja con confirmación   → DialogoConfirmar
Item {
    id: pagina

    signal volver()

    property var usuarios: []
    property var visibles: []
    property bool cargando: false
    property var porEliminar: null
    // En angosto el puesto baja debajo del nombre y Editar/Eliminar pasan a un menú
    readonly property bool compacta: width < 760

    function cargar() {
        cargando = true
        Api.listarUsuarios(function (r) {
            if (!pagina)
                return
            pagina.cargando = false
            if (r.codigo !== "OK") {
                Avisos.error(r.mensaje)
                return
            }
            pagina.usuarios = r.datos
            pagina.filtrar()
        })
    }

    function filtrar() {
        const texto = busqueda.text.trim().toLowerCase()
        visibles = usuarios.filter(u => texto === "" || u.nombre.toLowerCase().indexOf(texto) >= 0
                                        || u.usuario.indexOf(texto) >= 0)
    }

    function puesto(usuario) {
        return Tema.nombreRol(usuario) + (usuario.caja_asignada ? " · caja " + usuario.caja_asignada : "")
    }

    function pedirEliminar(usuario) {
        if (usuario.motivo_sin_baja !== "") {
            Avisos.error(usuario.motivo_sin_baja)
            return
        }
        porEliminar = usuario
        confirmarBaja.mensaje = "Se eliminará a " + usuario.nombre + " (" + usuario.usuario + ")."
        confirmarBaja.open()
    }

    function eliminar() {
        Api.eliminarUsuario(porEliminar.id_usuario, function (r) {
            if (r.codigo === "OK")
                Avisos.exito(r.mensaje)
            else
                Avisos.error(r.mensaje)
        })
    }

    Component.onCompleted: cargar()

    Connections {
        target: Api
        function onUsuariosCambiaron() { pagina.cargar() }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tema.margen
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Button {
                text: "← Volver"
                flat: true
                onClicked: pagina.volver()
            }
            Label {
                text: "Usuarios"
                color: Tema.texto
                font.pixelSize: Tema.letraTitulo
                font.bold: true
            }
            Label {
                visible: !pagina.compacta
                text: pagina.usuarios.length + " registrados"
                color: Tema.textoSuave
            }
            Item { Layout.fillWidth: true }
            Button {
                text: pagina.compacta ? "Nuevo" : "Nuevo usuario"
                highlighted: true
                onClicked: dialogo.abrirNuevo()
            }
        }

        TextField {
            id: busqueda
            Layout.fillWidth: true
            Layout.maximumWidth: 360
            placeholderText: "Buscar por nombre o usuario…"
            onTextChanged: pagina.filtrar()
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Tema.radio
            color: Tema.superficie
            border.color: Tema.borde
            clip: true

            ListView {
                id: lista
                anchors.fill: parent
                anchors.margins: 1
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: pagina.visibles
                ScrollBar.vertical: ScrollBar {}

                delegate: Rectangle {
                    id: fila
                    required property var modelData
                    required property int index
                    width: lista.width
                    implicitHeight: 64
                    color: index % 2 === 1 ? Qt.rgba(Tema.superficieAlta.r, Tema.superficieAlta.g, Tema.superficieAlta.b, 0.5)
                                           : Tema.superficie

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 8
                        spacing: 8

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0
                            Label {
                                Layout.fillWidth: true
                                text: fila.modelData.nombre + (fila.modelData.es_yo ? " (tú)" : "")
                                color: Tema.texto
                                font.pixelSize: 15
                                font.bold: true
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                text: fila.modelData.usuario + (pagina.compacta ? " · " + pagina.puesto(fila.modelData) : "")
                                color: Tema.textoSuave
                                font.pixelSize: Tema.letraChica
                                elide: Text.ElideRight
                            }
                        }

                        Label {
                            visible: !pagina.compacta
                            Layout.preferredWidth: 240
                            text: pagina.puesto(fila.modelData)
                            color: Tema.texto
                            elide: Text.ElideRight
                        }

                        Button {
                            visible: !pagina.compacta
                            text: "Editar"
                            flat: true
                            onClicked: dialogo.abrirEdicion(fila.modelData)
                        }
                        Button {
                            visible: !pagina.compacta
                            text: "Eliminar"
                            flat: true
                            onClicked: pagina.pedirEliminar(fila.modelData)
                        }

                        ToolButton {
                            visible: pagina.compacta
                            text: "⋮"
                            font.pixelSize: 18
                            onClicked: menuFila.open()
                            Menu {
                                id: menuFila
                                MenuItem {
                                    text: "Editar"
                                    onTriggered: dialogo.abrirEdicion(fila.modelData)
                                }
                                MenuItem {
                                    text: "Eliminar"
                                    onTriggered: pagina.pedirEliminar(fila.modelData)
                                }
                            }
                        }
                    }

                    Rectangle {
                        anchors.bottom: parent.bottom
                        width: parent.width
                        height: 1
                        color: Tema.borde
                        opacity: 0.6
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: !pagina.cargando && pagina.visibles.length === 0
                    text: "No hay usuarios"
                    color: Tema.textoSuave
                }
            }
        }
    }

    DialogoUsuario {
        id: dialogo
    }

    DialogoConfirmar {
        id: confirmarBaja
        titulo: "¿Eliminar usuario?"
        textoAceptar: "Eliminar"
        peligroso: true
        onConfirmado: pagina.eliminar()
    }
}
