pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// "MENU ADMINISTRADOR": caja, usuarios, productos, ingredientes y logs.
// Caja, Usuarios e Ingredientes están completas; las otras dos son tus ejercicios.
// El nivel cajero no ve el menú: entra directo a la Caja.
Item {
    id: pantalla

    signal salir()

    readonly property bool esAdmin: Api.esAdministrador
    // La sección abierta (la usa el recorrido de capturas, Capturas.qml)
    readonly property Item seccion: interna.currentItem

    // Botón Atrás de Android; regresa false si ya no hay a dónde retroceder
    function atras() {
        const actual = interna.currentItem
        if (actual.atras && actual.atras())
            return true
        if (interna.depth > 1) {
            interna.pop()
            return true
        }
        return false
    }

    function abrirSeccion(clave) {
        const secciones = { caja: cCaja, eventos: cEventos, productos: cProductos, paquetes: cPaquetes,
                            ingredientes: cIngredientes, usuarios: cUsuarios, bitacora: cBitacora }
        if (secciones[clave])
            interna.push(secciones[clave])
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
            titulo: pantalla.esAdmin ? "Administración" : "Caja"
            onSalir: pantalla.salir()
        }

        StackView {
            id: interna
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            initialItem: pantalla.esAdmin ? cInicio : cCaja
        }
    }

    Component {
        id: cInicio

        // Con todas las secciones el menú no cabe en pantallas chicas: se desplaza
        Flickable {
            id: menu
            contentWidth: width
            contentHeight: columnaMenu.implicitHeight + 80
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            ColumnLayout {
                id: columnaMenu
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 40
                width: Math.min(960, parent.width - (parent.width < 640 ? 24 : 64))
                spacing: Tema.margen

                Label {
                    Layout.fillWidth: true
                    text: "Menú de administración"
                    color: Tema.texto
                    font.pixelSize: parent.width < 480 ? 22 : 28
                    font.bold: true
                    wrapMode: Text.WordWrap
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
                            { clave: "caja", titulo: "Caja", lista: true,
                              descripcion: "Cobrar y cerrar cuentas, y cancelar cuentas y comandas." },
                            { clave: "eventos", titulo: "Eventos", lista: true,
                              descripcion: "Calendario, alta con repetición, mesas, paquetes, anticipos, y empezar o terminar el evento." },
                            { clave: "productos", titulo: "Productos", lista: true,
                              descripcion: "El menú con descripción, categoría, precio, variantes y receta." },
                            { clave: "paquetes", titulo: "Paquetes", lista: true,
                              descripcion: "Lo que se vende por evento: título, descripción, monto y productos incluidos." },
                            { clave: "ingredientes", titulo: "Ingredientes", lista: true,
                              descripcion: "Existencias de cada ingrediente y ajustes de stock." },
                            { clave: "usuarios", titulo: "Usuarios", lista: true,
                              descripcion: "Altas, edición y bajas de usuarios, con su rol y los datos de su puesto." },
                            { clave: "bitacora", titulo: "Logs", lista: true,
                              descripcion: "Registro de actividad del servidor: sesiones, cambios y operaciones rechazadas." }
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
        id: cCaja
        PantallaCaja {
            puedeVolver: pantalla.esAdmin
            onVolver: interna.pop()
        }
    }

    Component {
        id: cEventos
        AdminEventos {
            onVolver: interna.pop()
            onAbrirEvento: (evento, fecha) => interna.push(cEditorEvento, { evento: evento, fechaInicial: fecha })
        }
    }

    Component {
        id: cEditorEvento
        EditorEvento {
            onVolver: interna.pop()
        }
    }

    Component {
        id: cProductos
        AdminProductos {
            onVolver: interna.pop()
        }
    }

    Component {
        id: cPaquetes
        AdminPaquetes {
            onVolver: interna.pop()
        }
    }

    Component {
        id: cBitacora
        AdminBitacora {
            onVolver: interna.pop()
        }
    }

    Component {
        id: cUsuarios
        AdminUsuarios {
            onVolver: interna.pop()
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
