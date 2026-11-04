import QtQuick
import QtQuick.Controls

// Aviso flotante en la parte de abajo. Se cierra solo o con un clic.
Rectangle {
    id: aviso

    property string tipo: "info"
    readonly property color tono: tipo === "exito" ? Tema.exito : (tipo === "error" ? Tema.peligro : Tema.info)

    function mostrar(texto, tipoAviso) {
        etiqueta.text = texto
        tipo = tipoAviso || "info"
        opacity = 1
        temporizador.restart()
    }

    width: Math.min(etiqueta.implicitWidth + 48, parent ? parent.width - 64 : 600)
    height: etiqueta.implicitHeight + 24
    radius: 12
    color: tono
    opacity: 0
    visible: opacity > 0

    Behavior on opacity { NumberAnimation { duration: 180 } }

    Label {
        id: etiqueta
        anchors.centerIn: parent
        width: parent.width - 32
        wrapMode: Text.WordWrap
        horizontalAlignment: Text.AlignHCenter
        color: Tema.textoSobreAcento
        font.bold: true
    }

    Timer {
        id: temporizador
        interval: 3200
        onTriggered: aviso.opacity = 0
    }

    MouseArea {
        anchors.fill: parent
        onClicked: aviso.opacity = 0
    }
}
