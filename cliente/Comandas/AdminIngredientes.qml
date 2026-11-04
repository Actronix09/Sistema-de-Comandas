pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// "Administrador de ingredientes": el patrón CRUD completo que vas a repetir
// en Usuarios y Productos.
//   lista                   → Api.listarIngredientes
//   alta y edición          → DialogoIngrediente (el servidor valida)
//   baja con confirmación   → DialogoConfirmar
// Todo se hace con botones; un doble toque en la fila también la edita.
Item {
    id: pagina

    signal volver()

    property var ingredientes: []
    property var visibles: []
    property bool cargando: false
    property var porEliminar: null
    // En angosto se oculta "Se usa en" y Editar/Eliminar pasan a un menú
    readonly property bool compacta: width < 760

    function cargar() {
        cargando = true
        Api.listarIngredientes(function (r) {
            if (!pagina)
                return
            pagina.cargando = false
            if (r.codigo !== "OK") {
                Avisos.error(r.mensaje)
                return
            }
            pagina.ingredientes = r.datos
            pagina.filtrar()
        })
    }

    function filtrar() {
        const texto = busqueda.text.trim().toLowerCase()
        visibles = ingredientes.filter(i => texto === "" || i.nombre.toLowerCase().indexOf(texto) >= 0)
    }

    function ajustar(ingrediente, delta) {
        Api.ajustarStock(ingrediente.id_ingrediente, delta, function (r) {
            if (r.codigo !== "OK")
                Avisos.error(r.mensaje)
        })
    }

    function pedirEliminar(ingrediente) {
        porEliminar = ingrediente
        confirmarBaja.mensaje = "Se eliminará «" + ingrediente.nombre + "»."
                + (ingrediente.usos > 0 ? " También se quitará de la receta de "
                                          + ingrediente.usos + (ingrediente.usos === 1 ? " producto." : " productos.") : "")
        confirmarBaja.open()
    }

    function eliminar() {
        Api.eliminarIngrediente(porEliminar.id_ingrediente, function (r) {
            if (r.codigo === "OK")
                Avisos.exito(r.mensaje)
            else
                Avisos.error(r.mensaje)
        })
    }

    function reiniciar() {
        Api.reiniciarStock(function (r) {
            if (r.codigo === "OK")
                Avisos.exito(r.mensaje)
            else
                Avisos.error(r.mensaje)
        })
    }

    Component.onCompleted: cargar()

    Connections {
        target: Api
        function onInventarioCambio() { pagina.cargar() }
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
                text: "Ingredientes"
                color: Tema.texto
                font.pixelSize: Tema.letraTitulo
                font.bold: true
            }
            Label {
                visible: !pagina.compacta
                text: pagina.ingredientes.length + " registrados"
                color: Tema.textoSuave
            }
            Item { Layout.fillWidth: true }
            Button {
                text: pagina.compacta ? "Nuevo" : "Nuevo ingrediente"
                highlighted: true
                onClicked: dialogo.abrirNuevo()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            TextField {
                id: busqueda
                Layout.fillWidth: true
                Layout.maximumWidth: 360
                placeholderText: "Buscar…"
                onTextChanged: pagina.filtrar()
            }
            Item { Layout.fillWidth: true }
            Button {
                text: "Poner todo en 0"
                flat: true
                onClicked: confirmarReinicio.open()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Tema.radio
            color: Tema.superficie
            border.color: Tema.borde
            clip: true

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 1
                spacing: 0

                // Encabezado de columnas
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 40
                    color: Tema.superficieAlta

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        Label {
                            Layout.fillWidth: true
                            text: "NOMBRE"
                            color: Tema.textoSuave
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Label {
                            Layout.preferredWidth: pagina.compacta ? 150 : 250
                            text: "EXISTENCIA"
                            color: Tema.textoSuave
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Label {
                            visible: !pagina.compacta
                            Layout.preferredWidth: 130
                            text: "SE USA EN"
                            color: Tema.textoSuave
                            font.pixelSize: 11
                            font.bold: true
                        }
                        Item { Layout.preferredWidth: pagina.compacta ? 48 : 190 }
                    }
                }

                ListView {
                    id: lista
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds
                    model: pagina.visibles
                    ScrollBar.vertical: ScrollBar {}

                    delegate: Rectangle {
                        id: fila
                        required property var modelData
                        required property int index
                        width: lista.width
                        implicitHeight: 54
                        color: index % 2 === 1 ? Qt.rgba(Tema.superficieAlta.r, Tema.superficieAlta.g, Tema.superficieAlta.b, 0.5)
                                               : Tema.superficie

                        MouseArea {
                            anchors.fill: parent
                            onDoubleClicked: dialogo.abrirEdicion(fila.modelData)
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 16
                            anchors.rightMargin: 16

                            Label {
                                Layout.fillWidth: true
                                text: fila.modelData.nombre
                                color: Tema.texto
                                font.pixelSize: 15
                                elide: Text.ElideRight
                            }

                            // Un layout anidado se estira por defecto; con fillWidth: false
                            // conserva su ancho y las columnas quedan alineadas.
                            RowLayout {
                                Layout.preferredWidth: pagina.compacta ? 150 : 250
                                Layout.fillWidth: false
                                spacing: 2
                                ToolButton {
                                    text: "−"
                                    font.pixelSize: 16
                                    enabled: fila.modelData.stock > 0
                                    onClicked: pagina.ajustar(fila.modelData, -1)
                                }
                                Label {
                                    Layout.preferredWidth: 48
                                    horizontalAlignment: Text.AlignHCenter
                                    text: fila.modelData.stock
                                    color: Tema.texto
                                    font.bold: true
                                    font.pixelSize: 15
                                }
                                ToolButton {
                                    text: "+"
                                    font.pixelSize: 16
                                    onClicked: pagina.ajustar(fila.modelData, 1)
                                }
                                Etiqueta {
                                    Layout.leftMargin: 6
                                    visible: !pagina.compacta && fila.modelData.stock < 10
                                    texto: fila.modelData.stock === 0 ? "Agotado" : "Bajo"
                                    tono: fila.modelData.stock === 0 ? Tema.peligro : Tema.advertencia
                                }
                                Item { Layout.fillWidth: true }
                            }

                            Label {
                                visible: !pagina.compacta
                                Layout.preferredWidth: 130
                                text: fila.modelData.usos === 0 ? "—"
                                      : fila.modelData.usos + (fila.modelData.usos === 1 ? " producto" : " productos")
                                color: Tema.textoSuave
                            }

                            RowLayout {
                                visible: !pagina.compacta
                                Layout.preferredWidth: 190
                                Layout.fillWidth: false
                                spacing: 4
                                Button {
                                    text: "Editar"
                                    flat: true
                                    onClicked: dialogo.abrirEdicion(fila.modelData)
                                }
                                Button {
                                    text: "Eliminar"
                                    flat: true
                                    onClicked: pagina.pedirEliminar(fila.modelData)
                                }
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
                        text: "No hay ingredientes"
                        color: Tema.textoSuave
                    }
                }
            }
        }
    }

    DialogoIngrediente {
        id: dialogo
    }

    DialogoConfirmar {
        id: confirmarBaja
        titulo: "¿Eliminar ingrediente?"
        textoAceptar: "Eliminar"
        peligroso: true
        onConfirmado: pagina.eliminar()
    }

    DialogoConfirmar {
        id: confirmarReinicio
        titulo: "¿Poner todo en 0?"
        mensaje: "Todas las existencias quedarán en 0 y ningún producto con receta se podrá ordenar."
        textoAceptar: "Poner en 0"
        peligroso: true
        onConfirmado: pagina.reiniciar()
    }
}
