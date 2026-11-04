import QtQuick
import QtQuick.Controls

// Píldora de texto con un tono de color (categorías, folios, contadores).
// Si el layout le da menos ancho del que pide, recorta el texto con "…".
// Para eso, dentro de un RowLayout usa Layout.fillWidth y Layout.maximumWidth: implicitWidth.
Rectangle {
    id: etiqueta

    property string texto: ""
    property color tono: Tema.textoSuave
    property bool solida: false

    implicitWidth: rotulo.implicitWidth + 14
    implicitHeight: 22
    radius: height / 2
    color: solida ? tono : Qt.rgba(tono.r, tono.g, tono.b, 0.14)
    border.width: solida ? 0 : 1
    border.color: Qt.rgba(tono.r, tono.g, tono.b, 0.35)

    Label {
        id: rotulo
        anchors.verticalCenter: parent.verticalCenter
        x: 7
        width: etiqueta.width - 14
        horizontalAlignment: Text.AlignHCenter
        elide: Text.ElideRight
        text: etiqueta.texto
        color: etiqueta.solida ? Tema.textoSobreAcento : etiqueta.tono
        font.pixelSize: 11
        font.bold: true
    }
}
