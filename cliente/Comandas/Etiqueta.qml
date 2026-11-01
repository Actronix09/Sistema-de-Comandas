import QtQuick
import QtQuick.Controls

// Píldora de texto con un tono de color (categorías, folios, contadores).
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
        anchors.centerIn: parent
        text: etiqueta.texto
        color: etiqueta.solida ? Tema.textoSobreAcento : etiqueta.tono
        font.pixelSize: 11
        font.bold: true
    }
}
