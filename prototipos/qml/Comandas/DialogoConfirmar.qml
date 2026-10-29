import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Confirmación genérica. Equivale a los botones CONFIRMAR / CANCELAR y SI / NO
// del sistema en terminal (dibujar_botones_confirmacion).
Dialog {
    id: dialogo

    property string titulo: "¿Confirmar?"
    property string mensaje: ""
    property string textoAceptar: "Confirmar"
    property bool peligroso: false
    signal confirmado()

    title: titulo
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(440, parent ? parent.width - 48 : 440)

    contentItem: ColumnLayout {
        spacing: 10
        Label {
            Layout.fillWidth: true
            text: dialogo.mensaje
            color: Tema.texto
            wrapMode: Text.WordWrap
        }
        Label {
            Layout.fillWidth: true
            visible: dialogo.peligroso
            text: "Esta acción no se puede deshacer."
            color: Tema.peligro
            font.bold: true
        }
    }

    footer: DialogButtonBox {
        Button {
            text: "Cancelar"
            flat: true
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
        Button {
            text: dialogo.textoAceptar
            highlighted: true
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
    }

    onAccepted: confirmado()
}
