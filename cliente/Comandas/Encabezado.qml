import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Barra superior de las pantallas con sesión: marca, evento en curso,
// controles propios de la pantalla, reloj, usuario y "Cerrar sesión".
// Lo que se declare dentro de Encabezado { ... } aparece junto al evento.
// En pantallas angostas se ocultan el reloj y el nombre, y los controles de la
// pantalla bajan a una segunda fila.
Rectangle {
    id: encabezado

    property string titulo: ""
    default property alias extras: zonaExtra.data
    signal salir()

    property var evento: null
    property date ahora: new Date()
    // También se compacta si los controles de la pantalla no dejan sitio al resto
    readonly property bool compacta: width < 760 || width - zonaExtra.implicitWidth < 700

    implicitHeight: rejilla.implicitHeight
    color: Tema.superficie

    Component.onCompleted: Api.eventoEnCurso(function (r) {
        if (encabezado && r.codigo === "OK")
            encabezado.evento = r.datos
    })

    Timer {
        interval: 20000
        running: true
        repeat: true
        onTriggered: encabezado.ahora = new Date()
    }

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 1
        color: Tema.borde
    }

    GridLayout {
        id: rejilla
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Tema.margen
        anchors.rightMargin: Tema.margen
        columnSpacing: Tema.espacio
        rowSpacing: 0

        Rectangle {
            Layout.row: 0
            Layout.column: 0
            Layout.topMargin: 14
            Layout.bottomMargin: 14
            implicitWidth: 36
            implicitHeight: 36
            radius: 10
            color: Tema.acento
            Label {
                anchors.centerIn: parent
                text: "C"
                color: Tema.textoSobreAcento
                font.pixelSize: 20
                font.bold: true
            }
        }

        ColumnLayout {
            Layout.row: 0
            Layout.column: 1
            spacing: 0
            Label {
                text: "Comandas"
                color: Tema.texto
                font.pixelSize: Tema.letraMediana
                font.bold: true
            }
            Label {
                text: encabezado.titulo
                color: Tema.textoSuave
                font.pixelSize: Tema.letraChica
            }
        }

        // Se oculta si no cabe junto a los controles de la pantalla
        Etiqueta {
            Layout.row: 0
            Layout.column: 2
            visible: encabezado.evento !== null && encabezado.width - zonaExtra.implicitWidth > 900
            Layout.leftMargin: Tema.espacio
            implicitHeight: 28
            tono: Tema.texto
            texto: encabezado.evento
                   ? "Evento #" + encabezado.evento.numero_evento + " · " + encabezado.evento.tipo_evento
                     + " · " + encabezado.evento.numero_asistentes + " asistentes"
                   : ""
        }

        RowLayout {
            id: zonaExtra
            Layout.row: encabezado.compacta ? 1 : 0
            Layout.column: encabezado.compacta ? 0 : 3
            Layout.columnSpan: encabezado.compacta ? 8 : 1
            Layout.fillWidth: encabezado.compacta
            Layout.leftMargin: encabezado.compacta ? 0 : Tema.espacio
            Layout.bottomMargin: encabezado.compacta && children.length > 0 ? 8 : 0
            visible: children.length > 0
            spacing: Tema.espacio
        }

        Item {
            Layout.row: 0
            Layout.column: 4
            Layout.fillWidth: true
        }

        Label {
            Layout.row: 0
            Layout.column: 5
            visible: !encabezado.compacta
            text: Qt.formatTime(encabezado.ahora, "HH:mm")
            color: Tema.textoSuave
            font.pixelSize: Tema.letraMediana
        }

        ColumnLayout {
            Layout.row: 0
            Layout.column: 6
            visible: !encabezado.compacta
            spacing: 0
            Layout.maximumWidth: 200
            Label {
                Layout.alignment: Qt.AlignRight
                Layout.maximumWidth: 200
                text: Api.usuarioActual ? Api.usuarioActual.nombre : ""
                color: Tema.texto
                font.bold: true
                elide: Text.ElideRight
            }
            Label {
                Layout.alignment: Qt.AlignRight
                text: Tema.nombreRol(Api.usuarioActual)
                color: Tema.textoSuave
                font.pixelSize: Tema.letraChica
            }
        }

        Button {
            Layout.row: 0
            Layout.column: 7
            text: encabezado.compacta ? "Salir" : "Cerrar sesión"
            flat: true
            onClicked: encabezado.salir()
        }
    }
}
