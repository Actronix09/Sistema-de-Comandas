import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Alta y edición de un ingrediente: el patrón de formulario con validación.
// La regla definitiva la aplica el servidor (aquí, Api); el diálogo solo evita
// enviar algo vacío y muestra el mensaje que regrese el servidor.
Dialog {
    id: dialogo

    property var ingrediente: null      // null = alta
    property bool guardando: false
    property string error: ""
    signal guardado(var ingrediente)

    function abrirNuevo() {
        ingrediente = null
        campoNombre.text = ""
        campoStock.value = 0
        error = ""
        open()
    }

    function abrirEdicion(datos) {
        ingrediente = datos
        campoNombre.text = datos.nombre
        campoStock.value = datos.stock
        error = ""
        open()
    }

    function guardar() {
        if (campoNombre.text.trim() === "") {
            error = "Escribe el nombre del ingrediente"
            campoNombre.forceActiveFocus()
            return
        }
        guardando = true
        error = ""
        const datos = { nombre: campoNombre.text, stock: campoStock.value }
        if (ingrediente)
            datos.id_ingrediente = ingrediente.id_ingrediente
        Api.guardarIngrediente(datos, function (r) {
            if (!dialogo)
                return
            dialogo.guardando = false
            if (r.codigo === "OK") {
                dialogo.close()
                Avisos.exito(r.mensaje)
                dialogo.guardado(r.datos)
            } else {
                dialogo.error = r.mensaje
            }
        })
    }

    title: ingrediente ? "Editar ingrediente" : "Nuevo ingrediente"
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(420, parent ? parent.width - 48 : 420)
    onOpened: campoNombre.forceActiveFocus()

    contentItem: ColumnLayout {
        spacing: 6

        Label {
            text: "Nombre"
            color: Tema.texto
            font.bold: true
        }
        TextField {
            id: campoNombre
            Layout.fillWidth: true
            maximumLength: 60
            placeholderText: "p. ej. Queso manchego"
            enabled: !dialogo.guardando
            onAccepted: dialogo.guardar()
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: campoNombre.length + " / 60"
            color: Tema.textoSuave
            font.pixelSize: 11
        }

        Label {
            text: "Existencia"
            color: Tema.texto
            font.bold: true
        }
        SpinBox {
            id: campoStock
            Layout.fillWidth: true
            from: 0
            to: 99999
            editable: true
            enabled: !dialogo.guardando
        }

        Label {
            Layout.fillWidth: true
            visible: dialogo.error !== ""
            text: dialogo.error
            color: Tema.peligro
            wrapMode: Text.WordWrap
        }
    }

    footer: DialogButtonBox {
        Button {
            text: "Cancelar"
            flat: true
            enabled: !dialogo.guardando
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
        Button {
            text: dialogo.guardando ? "Guardando…" : "Guardar"
            highlighted: true
            enabled: !dialogo.guardando
            DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
            onClicked: dialogo.guardar()
        }
    }
}
