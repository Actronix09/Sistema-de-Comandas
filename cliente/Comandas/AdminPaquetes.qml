pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Admin › Paquetes: lo que el salón vende por evento (Paquete Plata, Postres…),
// con su monto y los productos que incluye. Un paquete asignado a algún evento
// no se puede eliminar.
Item {
    id: pagina

    signal volver()

    property var paquetes: []
    property var productos: []
    property bool cargando: false
    property var porEliminar: null
    readonly property bool compacta: width < 760

    function cargar() {
        cargando = true
        Api.listarPaquetes(function (r) {
            if (!pagina)
                return
            pagina.cargando = false
            if (r.codigo !== "OK") {
                Avisos.error(r.mensaje)
                return
            }
            pagina.paquetes = r.datos
        })
        Api.listarProductos(function (r) {
            if (pagina && r.codigo === "OK")
                pagina.productos = r.datos
        })
    }

    function pedirEliminar(paquete) {
        if (paquete.eventos > 0) {
            Avisos.error("Hay " + paquete.eventos + (paquete.eventos === 1 ? " evento" : " eventos")
                         + " con este paquete; quítalo de esos eventos primero")
            return
        }
        porEliminar = paquete
        confirmarBaja.mensaje = "Se eliminará «" + paquete.titulo + "»."
        confirmarBaja.open()
    }

    Component.onCompleted: cargar()

    Connections {
        target: Api
        function onCatalogoCambio() { pagina.cargar() }
        function onEventosCambiaron() { pagina.cargar() }
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
                text: "Paquetes"
                color: Tema.texto
                font.pixelSize: Tema.letraTitulo
                font.bold: true
            }
            Item { Layout.fillWidth: true }
            Button {
                text: pagina.compacta ? "Nuevo" : "Nuevo paquete"
                highlighted: true
                onClicked: dialogo.abrirNuevo()
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            GridLayout {
                width: parent.width
                columns: pagina.width < 700 ? 1 : 2
                columnSpacing: Tema.margen
                rowSpacing: Tema.margen

                Repeater {
                    model: pagina.paquetes
                    delegate: Rectangle {
                        id: tarjeta
                        required property var modelData
                        Layout.fillWidth: true
                        Layout.preferredWidth: 1
                        implicitHeight: contenido.implicitHeight + 32
                        radius: Tema.radio
                        color: Tema.superficie
                        border.color: Tema.borde

                        ColumnLayout {
                            id: contenido
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 16
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true
                                Label {
                                    Layout.fillWidth: true
                                    text: tarjeta.modelData.titulo
                                    color: Tema.texto
                                    font.pixelSize: 18
                                    font.bold: true
                                    wrapMode: Text.WordWrap
                                }
                                Label {
                                    text: Tema.dinero(tarjeta.modelData.monto)
                                    color: Tema.acento
                                    font.pixelSize: 18
                                    font.bold: true
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: text !== ""
                                text: tarjeta.modelData.descripcion
                                color: Tema.textoSuave
                                wrapMode: Text.WordWrap
                            }
                            Flow {
                                Layout.fillWidth: true
                                spacing: 6
                                Repeater {
                                    model: tarjeta.modelData.productos
                                    delegate: Etiqueta {
                                        required property var modelData
                                        texto: modelData.nombre
                                        tono: Tema.colorCategoria(modelData.categoria)
                                    }
                                }
                                Label {
                                    visible: tarjeta.modelData.productos.length === 0
                                    text: "Sin productos"
                                    color: Tema.textoSuave
                                }
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Label {
                                    text: tarjeta.modelData.eventos === 0 ? "En ningún evento"
                                          : "En " + tarjeta.modelData.eventos + (tarjeta.modelData.eventos === 1 ? " evento" : " eventos")
                                    color: Tema.textoSuave
                                    font.pixelSize: Tema.letraChica
                                }
                                Item { Layout.fillWidth: true }
                                Button {
                                    text: "Editar"
                                    flat: true
                                    onClicked: dialogo.abrirEdicion(tarjeta.modelData)
                                }
                                Button {
                                    text: "Eliminar"
                                    flat: true
                                    opacity: tarjeta.modelData.eventos > 0 ? 0.5 : 1
                                    onClicked: pagina.pedirEliminar(tarjeta.modelData)
                                }
                            }
                        }
                    }
                }
            }
        }

        Label {
            Layout.alignment: Qt.AlignHCenter
            visible: pagina.paquetes.length === 0 && !pagina.cargando
            text: "Todavía no hay paquetes"
            color: Tema.textoSuave
        }
    }

    DialogoPaquete {
        id: dialogo
        productos: pagina.productos
    }

    DialogoConfirmar {
        id: confirmarBaja
        titulo: "¿Eliminar paquete?"
        textoAceptar: "Eliminar"
        peligroso: true
        onConfirmado: Api.eliminarPaquete(pagina.porEliminar.numero_paquete, function (r) {
            if (r.codigo === "OK")
                Avisos.exito(r.mensaje)
            else
                Avisos.error(r.mensaje)
        })
    }
}
