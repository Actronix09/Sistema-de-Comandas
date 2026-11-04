import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Barra superior de las pantallas con sesión: marca, evento en curso,
// controles propios de la pantalla, reloj, usuario y "Cerrar sesión".
// Lo que se declare dentro de Encabezado { ... } aparece junto al evento.
Rectangle {
    id: encabezado

    property string titulo: ""
    default property alias extras: zonaExtra.data
    signal salir()

    property var evento: null
    property date ahora: new Date()

    implicitHeight: 64
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

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Tema.margen
        anchors.rightMargin: Tema.margen
        spacing: Tema.espacio

        Rectangle {
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
            Layout.leftMargin: Tema.espacio
            spacing: Tema.espacio
        }

        Item { Layout.fillWidth: true }

        Label {
            text: Qt.formatTime(encabezado.ahora, "HH:mm")
            color: Tema.textoSuave
            font.pixelSize: Tema.letraMediana
        }

        ColumnLayout {
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
            text: "Cerrar sesión"
            flat: true
            onClicked: encabezado.salir()
        }
    }
}
