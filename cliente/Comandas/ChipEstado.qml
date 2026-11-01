import QtQuick
import QtQuick.Controls

// Estado de una comanda. Si es interactivo, un toque avanza al siguiente estado.
// El área que recibe el toque mide 48 px de alto aunque el chip se vea más chico.
Rectangle {
    id: chip

    property string estado: "pendiente"
    property bool interactivo: false
    property string ayuda: ""
    signal pulsado()

    readonly property color tono: Tema.colorEstado(estado)

    implicitWidth: rotulo.implicitWidth + 18
    implicitHeight: 24
    radius: height / 2
    color: Qt.rgba(tono.r, tono.g, tono.b, raton.containsMouse ? 0.30 : 0.15)
    border.width: 1
    border.color: Qt.rgba(tono.r, tono.g, tono.b, 0.45)

    Label {
        id: rotulo
        anchors.centerIn: parent
        text: Tema.nombreEstado(chip.estado) + (chip.interactivo ? "  ›" : "")
        color: chip.tono
        font.pixelSize: Tema.letraChica
        font.bold: true
    }

    MouseArea {
        id: raton
        anchors.fill: parent
        anchors.topMargin: -12
        anchors.bottomMargin: -12
        enabled: chip.interactivo
        hoverEnabled: chip.interactivo
        cursorShape: chip.interactivo ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: chip.pulsado()
    }

    ToolTip.visible: chip.interactivo && raton.containsMouse && chip.ayuda.length > 0
    ToolTip.text: chip.ayuda
    ToolTip.delay: 400
}
