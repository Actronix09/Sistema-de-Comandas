pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// "Confirmar Orden": resumen antes de enviar. La mesa y la cuenta ya se
// eligieron antes de los productos, así que aquí solo se muestran.
Dialog {
    id: dialogo

    property var lineas: []        // [{ id_producto, nombre, precio, cantidad, detalles }]
    property string destino: ""    // "Mesa 1 · Novios · 17-1-1"
    property bool enviando: false
    property string error: ""
    signal confirmar()

    readonly property int unidades: lineas.reduce((suma, l) => suma + l.cantidad, 0)
    readonly property real total: lineas.reduce((suma, l) => suma + l.precio * l.cantidad, 0)

    title: "Confirmar orden"
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(560, parent ? parent.width - 32 : 560)
    closePolicy: enviando ? Popup.NoAutoClose : (Popup.CloseOnEscape | Popup.CloseOnPressOutside)

    onOpened: error = ""

    contentItem: ColumnLayout {
        spacing: 8

        Label {
            Layout.fillWidth: true
            text: dialogo.destino
            color: Tema.texto
            font.pixelSize: Tema.letraMediana
            font.bold: true
            wrapMode: Text.WordWrap
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: resumen.implicitHeight + 16
            radius: 8
            color: Tema.superficieAlta

            ColumnLayout {
                id: resumen
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 8
                spacing: 4

                Repeater {
                    model: dialogo.lineas
                    delegate: RowLayout {
                        id: renglon
                        required property var modelData
                        Layout.fillWidth: true
                        Label {
                            Layout.preferredWidth: 30
                            Layout.alignment: Qt.AlignTop
                            text: renglon.modelData.cantidad + "×"
                            color: Tema.texto
                            font.bold: true
                        }
                        Label {
                            Layout.fillWidth: true
                            text: renglon.modelData.nombre
                                  + (renglon.modelData.detalles ? "  —  “" + renglon.modelData.detalles + "”" : "")
                            color: Tema.texto
                            wrapMode: Text.WordWrap
                        }
                        Label {
                            Layout.alignment: Qt.AlignTop
                            text: Tema.dinero(renglon.modelData.precio * renglon.modelData.cantidad)
                            color: Tema.texto
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: "Se crearán " + dialogo.unidades + (dialogo.unidades === 1 ? " comanda" : " comandas")
                      + " (una por unidad)"
                color: Tema.textoSuave
                font.pixelSize: Tema.letraChica
                wrapMode: Text.WordWrap
            }
            Label {
                text: "Total " + Tema.dinero(dialogo.total)
                color: Tema.acento
                font.pixelSize: 18
                font.bold: true
            }
        }

        Label {
            Layout.fillWidth: true
            visible: dialogo.error !== ""
            text: dialogo.error
            color: Tema.peligro
            wrapMode: Text.WordWrap
        }
    }

    footer: DialogButtonBox {
        Button {
            text: "Cancelar"
            flat: true
            enabled: !dialogo.enviando
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
        // ActionRole: no cierra el diálogo; se cierra cuando el servidor responde OK
        Button {
            text: dialogo.enviando ? "Enviando…" : "Enviar a cocina y bar"
            highlighted: true
            enabled: !dialogo.enviando
            DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
            onClicked: dialogo.confirmar()
        }
    }
}
