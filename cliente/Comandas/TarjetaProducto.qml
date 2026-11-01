import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Tarjeta de un producto del catálogo (equivale a dibujar_recuadro_producto).
// Un toque o clic en la tarjeta agrega una unidad.
Rectangle {
    id: tarjeta

    required property var producto
    property int cantidad: 0
    signal agregar()
    signal quitar()

    readonly property bool agotado: !producto.disponible
    readonly property bool enTope: cantidad >= producto.maximo

    radius: Tema.radio
    color: raton.containsMouse && !agotado ? Tema.superficieAlta : Tema.superficie
    border.width: cantidad > 0 ? 2 : 1
    border.color: cantidad > 0 ? Qt.rgba(Tema.acento.r, Tema.acento.g, Tema.acento.b, 0.45) : Tema.borde
    opacity: agotado ? 0.55 : 1

    MouseArea {
        id: raton
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: tarjeta.agotado ? Qt.ForbiddenCursor : Qt.PointingHandCursor
        onClicked: tarjeta.agregar()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 6

        RowLayout {
            spacing: 6
            Etiqueta {
                texto: Tema.nombreCategoria(tarjeta.producto.categoria)
                tono: Tema.colorCategoria(tarjeta.producto.categoria)
            }
            Etiqueta {
                visible: tarjeta.producto.variante_de !== ""
                texto: "Variante de " + tarjeta.producto.variante_de
                tono: Tema.textoSuave
            }
            Item { Layout.fillWidth: true }
            Etiqueta {
                visible: tarjeta.cantidad > 0
                texto: "×" + tarjeta.cantidad
                tono: Tema.acento
                solida: true
            }
        }

        Label {
            Layout.fillWidth: true
            text: tarjeta.producto.nombre
            color: Tema.texto
            font.pixelSize: 17
            font.bold: true
            elide: Text.ElideRight
        }

        Label {
            Layout.fillWidth: true
            text: tarjeta.producto.descripcion
            color: Tema.textoSuave
            font.pixelSize: Tema.letraChica
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }

        Label {
            Layout.fillWidth: true
            visible: tarjeta.producto.ingredientes.length > 0
            text: tarjeta.producto.ingredientes.join(" · ")
            color: Tema.textoSuave
            font.pixelSize: 11
            font.italic: true
            elide: Text.ElideRight
        }

        Item { Layout.fillHeight: true }

        RowLayout {
            spacing: 4
            Label {
                text: Tema.dinero(tarjeta.producto.precio)
                color: Tema.acento
                font.pixelSize: 18
                font.bold: true
            }
            Item { Layout.fillWidth: true }
            Label {
                visible: tarjeta.agotado
                text: "Sin stock"
                color: Tema.peligro
                font.bold: true
            }
            ToolButton {
                visible: tarjeta.cantidad > 0 && !tarjeta.agotado
                text: "−"
                font.pixelSize: 18
                onClicked: tarjeta.quitar()
            }
            ToolButton {
                visible: !tarjeta.agotado
                enabled: !tarjeta.enTope
                text: "+"
                font.pixelSize: 18
                onClicked: tarjeta.agregar()
                ToolTip.visible: hovered && tarjeta.enTope
                ToolTip.text: "No hay stock para más"
            }
        }
    }
}
