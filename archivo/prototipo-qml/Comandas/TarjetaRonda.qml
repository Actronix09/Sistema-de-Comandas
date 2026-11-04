pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Una ronda: las comandas que una cuenta pidió en el mismo envío.
// Equivale a dibujar_recuadro_pedido_horizontal del sistema en terminal.
//   modo "cocina": cada estado se avanza con un clic, más los botones Iniciar y Lista.
//   modo "mesero": botón Entregar en lo que ya está listo.
Rectangle {
    id: tarjeta

    required property var ronda
    property string modo: "cocina"
    property bool seleccionada: false
    property date ahora: new Date()
    signal cambiar(var ids, string estado)
    signal elegida()

    readonly property int minutos: Math.max(0, Math.floor((ahora.getTime() - ronda.fecha_hora) / 60000))
    readonly property var pendientes: idsEn(["pendiente"])
    readonly property var porTerminar: idsEn(["pendiente", "en_preparacion"])
    readonly property var listas: idsEn(["lista"])

    function idsEn(estados) {
        let ids = []
        for (const l of ronda.lineas)
            if (estados.indexOf(l.estado) >= 0)
                ids = ids.concat(l.ids)
        return ids
    }

    function siguiente(estado) {
        if (estado === "pendiente")
            return "en_preparacion"
        if (estado === "en_preparacion")
            return "lista"
        return ""
    }

    implicitHeight: columna.implicitHeight + 24
    radius: Tema.radio
    color: Tema.superficie
    border.width: seleccionada ? 2 : 1
    border.color: seleccionada ? Tema.acento : Tema.borde

    MouseArea {
        anchors.fill: parent
        onClicked: tarjeta.elegida()
    }

    ColumnLayout {
        id: columna
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12
        spacing: 8

        RowLayout {
            spacing: 8
            Label {
                text: "Mesa " + tarjeta.ronda.numero_mesa
                color: Tema.texto
                font.pixelSize: 18
                font.bold: true
            }
            Etiqueta {
                texto: "Cuenta " + tarjeta.ronda.folio
                tono: Tema.textoSuave
            }
            Item { Layout.fillWidth: true }
            // En cocina el tiempo se pinta según la espera, solo si falta algo por preparar
            Etiqueta {
                readonly property bool urgente: tarjeta.modo === "cocina" && tarjeta.porTerminar.length > 0
                texto: tarjeta.minutos + " min"
                tono: urgente ? Tema.colorEspera(tarjeta.minutos) : Tema.textoSuave
                solida: urgente && tarjeta.minutos >= 10
            }
        }

        Label {
            text: tarjeta.ronda.mesero + " · " + Tema.hora(tarjeta.ronda.fecha_hora)
            color: Tema.textoSuave
            font.pixelSize: Tema.letraChica
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 1
            color: Tema.borde
        }

        Repeater {
            model: tarjeta.ronda.lineas
            delegate: RowLayout {
                id: fila
                required property var modelData
                Layout.fillWidth: true
                spacing: 8

                Label {
                    Layout.preferredWidth: 30
                    Layout.alignment: Qt.AlignTop
                    text: fila.modelData.cantidad + "×"
                    color: Tema.texto
                    font.bold: true
                    font.pixelSize: 15
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0
                    Label {
                        Layout.fillWidth: true
                        text: fila.modelData.producto
                        color: fila.modelData.estado === "cancelada" ? Tema.textoSuave : Tema.texto
                        font.pixelSize: 15
                        font.strikeout: fila.modelData.estado === "cancelada"
                        elide: Text.ElideRight
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: fila.modelData.detalles.length > 0
                        text: "“" + fila.modelData.detalles + "”"
                        color: Tema.advertencia
                        font.italic: true
                        font.pixelSize: Tema.letraChica
                        wrapMode: Text.WordWrap
                    }
                }
                ChipEstado {
                    Layout.alignment: Qt.AlignTop
                    estado: fila.modelData.estado
                    interactivo: tarjeta.modo === "cocina" && tarjeta.siguiente(fila.modelData.estado) !== ""
                    ayuda: "Pasar a " + Tema.nombreEstado(tarjeta.siguiente(fila.modelData.estado))
                    onPulsado: tarjeta.cambiar(fila.modelData.ids, tarjeta.siguiente(fila.modelData.estado))
                }
                Button {
                    Layout.alignment: Qt.AlignTop
                    visible: tarjeta.modo === "mesero" && fila.modelData.estado === "lista"
                    text: "Entregar"
                    onClicked: tarjeta.cambiar(fila.modelData.ids, "entregada")
                }
            }
        }

        RowLayout {
            visible: tarjeta.modo === "cocina" && tarjeta.porTerminar.length > 0
            spacing: 8
            Item { Layout.fillWidth: true }
            Button {
                text: "Iniciar (1)"
                enabled: tarjeta.pendientes.length > 0
                onClicked: tarjeta.cambiar(tarjeta.pendientes, "en_preparacion")
            }
            Button {
                text: "Lista (2)"
                highlighted: true
                enabled: tarjeta.porTerminar.length > 0
                onClicked: tarjeta.cambiar(tarjeta.porTerminar, "lista")
            }
        }

        RowLayout {
            visible: tarjeta.modo === "mesero"
            spacing: 8
            Label {
                text: "Total de la ronda: " + Tema.dinero(tarjeta.ronda.total)
                color: Tema.textoSuave
            }
            Item { Layout.fillWidth: true }
            Button {
                visible: tarjeta.listas.length > 1
                text: "Entregar todo lo listo"
                onClicked: tarjeta.cambiar(tarjeta.listas, "entregada")
            }
        }
    }
}
