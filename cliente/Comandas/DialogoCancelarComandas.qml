import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Cancelar comandas iguales de una cuenta. Cada comanda es una unidad, así que de
// "3× Cerveza" se puede cancelar solo una parte.
Dialog {
    id: dialogo

    property var linea: null        // { producto, estado, cantidad, ids }
    signal confirmado(var ids)

    function abrir(datos) {
        linea = datos
        campoCantidad.value = 1
        open()
    }

    title: "¿Cancelar comanda?"
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(440, parent ? parent.width - 48 : 440)

    contentItem: ColumnLayout {
        spacing: 10
        Label {
            Layout.fillWidth: true
            text: dialogo.linea ? dialogo.linea.cantidad + "× " + dialogo.linea.producto
                                  + " · " + Tema.nombreEstado(dialogo.linea.estado) : ""
            color: Tema.texto
            font.bold: true
            wrapMode: Text.WordWrap
        }
        Label {
            visible: dialogo.linea !== null && dialogo.linea.cantidad > 1
            text: "¿Cuántas?"
            color: Tema.texto
        }
        SpinBox {
            id: campoCantidad
            visible: dialogo.linea !== null && dialogo.linea.cantidad > 1
            Layout.fillWidth: true
            from: 1
            to: dialogo.linea ? dialogo.linea.cantidad : 1
        }
        Label {
            Layout.fillWidth: true
            text: dialogo.linea && dialogo.linea.estado === "pendiente"
                  ? "Todavía no se prepara: no gasta ingredientes y deja de cobrarse."
                  : "Ya se empezó a preparar: deja de cobrarse, pero sus ingredientes no se regresan."
            color: Tema.textoSuave
            wrapMode: Text.WordWrap
        }
        Label {
            Layout.fillWidth: true
            text: "Esta acción no se puede deshacer."
            color: Tema.peligro
            font.bold: true
        }
    }

    footer: DialogButtonBox {
        Button {
            text: "Regresar"
            flat: true
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
        Button {
            text: "Cancelar comanda"
            highlighted: true
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
    }

    onAccepted: confirmado(linea.ids.slice(0, campoCantidad.value))
}
