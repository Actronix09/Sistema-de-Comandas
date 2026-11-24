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
        comboUnidad.currentIndex = comboUnidad.indexOfValue("pieza")
        campoStock.value = 0
        campoMinimo.value = 0
        error = ""
        open()
    }

    function abrirEdicion(datos) {
        ingrediente = datos
        campoNombre.text = datos.nombre
        comboUnidad.currentIndex = comboUnidad.indexOfValue(datos.unidad_medida)
        campoStock.value = datos.stock
        campoMinimo.value = datos.stock_minimo
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
        const datos = { nombre: campoNombre.text, unidad_medida: comboUnidad.currentValue,
                        stock: campoStock.value, stock_minimo: campoMinimo.value }
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

        // Las cantidades van en la unidad base, como en la receta: 20000 g, no 20 kg
        Label {
            text: "Unidad"
            color: Tema.texto
            font.bold: true
        }
        ComboBox {
            id: comboUnidad
            Layout.fillWidth: true
            model: [
                { texto: "Piezas", valor: "pieza" },
                { texto: "Gramos (g)", valor: "g" },
                { texto: "Mililitros (ml)", valor: "ml" }
            ]
            textRole: "texto"
            valueRole: "valor"
            enabled: !dialogo.guardando
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
            to: 9999999
            stepSize: comboUnidad.currentValue === "pieza" ? 1 : 100
            editable: true
            enabled: !dialogo.guardando
        }

        Label {
            text: "Mínimo (avisa cuando quede menos)"
            color: Tema.texto
            font.bold: true
        }
        SpinBox {
            id: campoMinimo
            Layout.fillWidth: true
            from: 0
            to: 9999999
            stepSize: comboUnidad.currentValue === "pieza" ? 1 : 100
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
