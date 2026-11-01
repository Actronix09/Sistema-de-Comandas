pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// "MENU ADMINISTRADOR": usuarios, productos, ingredientes y logs.
// Ingredientes está completo; las otras tres secciones son tus ejercicios.
Item {
    id: pantalla

    signal salir()

    function abrirSeccion(clave) {
        if (clave === "ingredientes")
            interna.push(cIngredientes)
        else
            interna.push(cPendiente, { clave: clave })
    }

    Rectangle {
        anchors.fill: parent
        color: Tema.fondo
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Encabezado {
            Layout.fillWidth: true
            titulo: "Administración"
            onSalir: pantalla.salir()
        }

        StackView {
            id: interna
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            initialItem: cInicio
        }
    }

    Component {
        id: cInicio

        Item {
            ColumnLayout {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 40
                width: Math.min(960, parent.width - (parent.width < 640 ? 24 : 64))
                spacing: Tema.margen

                Label {
                    text: "Menú de administración"
                    color: Tema.texto
                    font.pixelSize: 28
                    font.bold: true
                }
                Label {
                    text: "Elige una sección."
                    color: Tema.textoSuave
                }

                GridLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 8
                    columns: parent.width < 560 ? 1 : 2
                    columnSpacing: Tema.margen
                    rowSpacing: Tema.margen

                    Repeater {
                        model: [
                            { clave: "usuarios", titulo: "Usuarios", lista: false,
                              descripcion: "Altas, edición y bajas de cuentas, con su rol y los datos de su puesto." },
                            { clave: "productos", titulo: "Productos", lista: false,
                              descripcion: "Catálogo con descripción, categoría, precio, variantes y receta." },
                            { clave: "ingredientes", titulo: "Ingredientes", lista: true,
                              descripcion: "Existencias de cada ingrediente y ajustes de stock." },
                            { clave: "logs", titulo: "Ver logs", lista: false,
                              descripcion: "Registros de autenticación y de cocina." }
                        ]

                        delegate: Rectangle {
                            id: seccion
                            required property var modelData
                            Layout.fillWidth: true
                            Layout.preferredWidth: 1
                            Layout.preferredHeight: 140
                            radius: Tema.radio
                            color: raton.containsMouse ? Tema.superficieAlta : Tema.superficie
                            border.width: raton.containsMouse ? 2 : 1
                            border.color: raton.containsMouse ? Tema.acento : Tema.borde

                            MouseArea {
                                id: raton
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: pantalla.abrirSeccion(seccion.modelData.clave)
                            }

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 20
                                spacing: 6

                                RowLayout {
                                    Layout.fillWidth: true
                                    Label {
                                        text: seccion.modelData.titulo
                                        color: Tema.texto
                                        font.pixelSize: 20
                                        font.bold: true
                                    }
                                    Item { Layout.fillWidth: true }
                                    Etiqueta {
                                        texto: seccion.modelData.lista ? "Listo" : "Por construir"
                                        tono: seccion.modelData.lista ? Tema.exito : Tema.advertencia
                                    }
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: seccion.modelData.descripcion
                                    color: Tema.textoSuave
                                    wrapMode: Text.WordWrap
                                }
                                Item { Layout.fillHeight: true }
                                Label {
                                    text: "Abrir →"
                                    color: Tema.acento
                                    font.bold: true
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Component {
        id: cIngredientes
        AdminIngredientes {
            onVolver: interna.pop()
        }
    }

    Component {
        id: cPendiente
        PantallaPendiente {
            onVolver: interna.pop()
        }
    }
}
