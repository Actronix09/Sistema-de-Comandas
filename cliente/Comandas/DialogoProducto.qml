pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Alta y edición de un producto: datos del menú, si es variante de otro y su
// receta (qué ingredientes lleva una porción y cuánto, en la unidad de cada
// ingrediente). Las reglas las revisa el servidor; aquí solo se arma el formulario.
Dialog {
    id: dialogo

    property var producto: null        // null = alta
    property var productos: []         // para elegir de cuál es variante
    property var ingredientes: []      // para la receta
    property bool guardando: false
    property string error: ""

    // Solo un producto que no es variante puede tener variantes, y uno no es
    // variante de sí mismo (tg_producto_variante)
    readonly property var principales: [{ texto: "Ninguno (producto principal)", valor: 0 }].concat(
        productos.filter(p => !p.id_producto_principal && (!producto || p.id_producto !== producto.id_producto))
                 .map(p => ({ texto: p.nombre, valor: p.id_producto })))

    ListModel { id: modeloReceta }

    function unidadDe(idIngrediente) {
        const i = ingredientes.find(x => x.id_ingrediente === idIngrediente)
        return i ? i.unidad_medida : ""
    }

    function abrirNuevo() {
        producto = null
        campoNombre.text = ""
        campoDescripcion.text = ""
        comboCategoria.currentIndex = 0
        campoPrecio.text = ""
        comboPrincipal.currentIndex = 0
        modeloReceta.clear()
        abrir()
    }

    function abrirEdicion(datos) {
        producto = datos
        campoNombre.text = datos.nombre
        campoDescripcion.text = datos.descripcion || ""
        comboCategoria.currentIndex = comboCategoria.indexOfValue(datos.categoria)
        campoPrecio.text = Number(datos.precio).toFixed(2)
        Qt.callLater(function () {
            comboPrincipal.currentIndex = Math.max(0, comboPrincipal.indexOfValue(datos.id_producto_principal || 0))
        })
        modeloReceta.clear()
        for (const r of datos.receta || [])
            modeloReceta.append({ id_ingrediente: r.id_ingrediente, cantidad: String(r.cantidad) })
        abrir()
    }

    function abrir() {
        error = ""
        desplazable.contentY = 0
        open()
    }

    function guardar() {
        if (campoNombre.text.trim() === "") {
            error = "Escribe el nombre del producto"
            return
        }
        const receta = []
        for (let i = 0; i < modeloReceta.count; i++) {
            const r = modeloReceta.get(i)
            if (!r.id_ingrediente) {
                error = "Elige el ingrediente de cada renglón de la receta, o quita el renglón"
                return
            }
            receta.push({ id_ingrediente: r.id_ingrediente, cantidad: String(r.cantidad).replace(",", ".") })
        }
        guardando = true
        error = ""
        const datos = {
            nombre: campoNombre.text, descripcion: campoDescripcion.text, categoria: comboCategoria.currentValue,
            precio: campoPrecio.text || 0, id_producto_principal: comboPrincipal.currentValue || null, receta: receta
        }
        if (producto)
            datos.id_producto = producto.id_producto
        Api.guardarProducto(datos, function (r) {
            if (!dialogo)
                return
            dialogo.guardando = false
            if (r.codigo === "OK") {
                dialogo.close()
                Avisos.exito(r.mensaje)
            } else {
                dialogo.error = r.mensaje
                Qt.callLater(function () { desplazable.contentY = Math.max(0, formulario.implicitHeight - desplazable.height) })
            }
        })
    }

    title: producto ? "Editar producto" : "Nuevo producto"
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(560, parent ? parent.width - 32 : 560)
    height: Math.min(implicitHeight, parent ? parent.height - 32 : implicitHeight)

    contentItem: Flickable {
        id: desplazable
        implicitHeight: formulario.implicitHeight
        contentHeight: formulario.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: formulario
            width: desplazable.width
            spacing: 6
            enabled: !dialogo.guardando

            Label { text: "Nombre"; color: Tema.texto; font.bold: true }
            TextField {
                id: campoNombre
                Layout.fillWidth: true
                maximumLength: 80
                placeholderText: "p. ej. Limonada"
            }

            Label { text: "Descripción"; color: Tema.texto; font.bold: true }
            TextArea {
                id: campoDescripcion
                Layout.fillWidth: true
                Layout.preferredHeight: Math.max(64, implicitHeight)
                wrapMode: TextEdit.Wrap
                placeholderText: "Lo que ve el mesero al tomar la orden (opcional)"
                onTextChanged: if (length > 255) remove(255, length)
            }

            GridLayout {
                Layout.fillWidth: true
                columns: dialogo.width < 460 ? 1 : 2
                columnSpacing: Tema.espacio
                rowSpacing: 6

                Label { text: "Categoría"; color: Tema.texto; font.bold: true }
                Label { text: "Precio"; color: Tema.texto; font.bold: true; visible: dialogo.width >= 460 }
                ComboBox {
                    id: comboCategoria
                    Layout.fillWidth: true
                    textRole: "texto"
                    valueRole: "valor"
                    model: [
                        { texto: "Alimentos (cocina)", valor: "alimentos" },
                        { texto: "Bebidas (bar)", valor: "bebidas" },
                        { texto: "Dulcería (cocina)", valor: "dulceria" }
                    ]
                }
                Label { text: "Precio"; color: Tema.texto; font.bold: true; visible: dialogo.width < 460 }
                TextField {
                    id: campoPrecio
                    Layout.fillWidth: true
                    placeholderText: "0.00"
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    validator: RegularExpressionValidator { regularExpression: /^\d{0,8}([.,]\d{0,2})?$/ }
                }
            }

            Label { text: "Es variante de"; color: Tema.texto; font.bold: true }
            ComboBox {
                id: comboPrincipal
                Layout.fillWidth: true
                textRole: "texto"
                valueRole: "valor"
                model: dialogo.principales
            }
            Label {
                Layout.fillWidth: true
                text: "Una variante es otra versión del mismo platillo (Café con leche de Café americano). Tiene su propia receta y precio."
                color: Tema.textoSuave
                font.pixelSize: Tema.letraChica
                wrapMode: Text.WordWrap
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 8
                Label { text: "Receta (por porción)"; color: Tema.texto; font.bold: true }
                Item { Layout.fillWidth: true }
                Button {
                    text: "+ Ingrediente"
                    flat: true
                    onClicked: modeloReceta.append({ id_ingrediente: 0, cantidad: "1" })
                }
            }
            Label {
                visible: modeloReceta.count === 0
                Layout.fillWidth: true
                text: "Sin ingredientes: no descuenta inventario y siempre aparece disponible."
                color: Tema.textoSuave
                font.pixelSize: Tema.letraChica
                wrapMode: Text.WordWrap
            }

            Repeater {
                model: modeloReceta
                delegate: RowLayout {
                    id: renglon
                    required property int index
                    required property int id_ingrediente
                    required property string cantidad
                    Layout.fillWidth: true
                    spacing: 6

                    ComboBox {
                        Layout.fillWidth: true
                        textRole: "nombre"
                        valueRole: "id_ingrediente"
                        model: dialogo.ingredientes
                        displayText: currentIndex < 0 ? "Elige un ingrediente…" : currentText
                        currentIndex: dialogo.ingredientes.findIndex(i => i.id_ingrediente === renglon.id_ingrediente)
                        onActivated: modeloReceta.setProperty(renglon.index, "id_ingrediente", currentValue)
                    }
                    TextField {
                        Layout.preferredWidth: 90
                        text: renglon.cantidad
                        horizontalAlignment: Text.AlignRight
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        validator: RegularExpressionValidator { regularExpression: /^\d{0,7}([.,]\d{0,3})?$/ }
                        onTextEdited: modeloReceta.setProperty(renglon.index, "cantidad", text)
                    }
                    Label {
                        Layout.preferredWidth: 40
                        text: dialogo.unidadDe(renglon.id_ingrediente)
                        color: Tema.textoSuave
                    }
                    ToolButton {
                        text: "✕"
                        onClicked: modeloReceta.remove(renglon.index)
                        ToolTip.visible: hovered
                        ToolTip.text: "Quitar de la receta"
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.topMargin: 6
                visible: dialogo.error !== ""
                text: dialogo.error
                color: Tema.peligro
                wrapMode: Text.WordWrap
            }
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
