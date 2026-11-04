import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Renglón de la orden en curso: producto, cantidad, nota para cocina y subtotal.
// Es el delegado de un ListModel: cada "required property" es un rol del modelo.
Rectangle {
    id: linea

    required property int index
    required property int id_producto
    required property string nombre
    required property real precio
    required property int cantidad
    required property int maximo
    required property string detalles
    signal incrementar()
    signal decrementar()
    signal notaEditada(string texto)

    implicitHeight: columna.implicitHeight + 20
    radius: 8
    color: Tema.superficieAlta

    ColumnLayout {
        id: columna
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 10
        spacing: 4

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: linea.nombre
                color: Tema.texto
                font.bold: true
                wrapMode: Text.WordWrap
            }
            Label {
                text: Tema.dinero(linea.precio * linea.cantidad)
                color: Tema.texto
                font.bold: true
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 2
            ToolButton {
                text: "−"
                font.pixelSize: 16
                onClicked: linea.decrementar()
            }
            Label {
                Layout.preferredWidth: 26
                horizontalAlignment: Text.AlignHCenter
                text: linea.cantidad
                color: Tema.texto
                font.bold: true
            }
            ToolButton {
                text: "+"
                font.pixelSize: 16
                enabled: linea.cantidad < linea.maximo
                onClicked: linea.incrementar()
            }
            Label {
                Layout.leftMargin: 6
                text: "× " + Tema.dinero(linea.precio)
                color: Tema.textoSuave
                font.pixelSize: Tema.letraChica
            }
            Item { Layout.fillWidth: true }
        }

        TextField {
            Layout.fillWidth: true
            placeholderText: "Nota para cocina (opcional)"
            text: linea.detalles
            maximumLength: 255
            onTextEdited: linea.notaEditada(text)
        }
    }
}
