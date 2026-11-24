pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Admin › Productos: el menú con su precio, categoría, variantes y receta.
//   lista                   → Api.listarProductos (ya trae la receta)
//   alta y edición          → DialogoProducto
//   baja con confirmación   → solo si nunca se pidió, no tiene variantes y no
//                             está en un paquete (motivo_sin_baja lo explica)
Item {
    id: pagina

    signal volver()

    property var productos: []
    property var ingredientes: []
    property var visibles: []
    property bool cargando: false
    property var porEliminar: null
    readonly property bool compacta: width < 760

    function cargar() {
        cargando = true
        Api.listarProductos(function (r) {
            if (!pagina)
                return
            pagina.cargando = false
            if (r.codigo !== "OK") {
                Avisos.error(r.mensaje)
                return
            }
            pagina.productos = r.datos
            pagina.filtrar()
        })
        Api.listarIngredientes(function (r) {
            if (pagina && r.codigo === "OK")
                pagina.ingredientes = r.datos
        })
    }

    function filtrar() {
        const texto = busqueda.text.trim().toLowerCase()
        const categoria = comboFiltro.currentValue
        visibles = productos.filter(p => (texto === "" || p.nombre.toLowerCase().indexOf(texto) >= 0)
                                         && (categoria === "todas" || p.categoria === categoria))
    }

    function pedirEliminar(producto) {
        if (producto.motivo_sin_baja) {
            Avisos.error(producto.motivo_sin_baja)
            return
        }
        porEliminar = producto
        confirmarBaja.mensaje = "Se eliminará «" + producto.nombre + "» con su receta."
        confirmarBaja.open()
    }

    Component.onCompleted: cargar()

    Connections {
        target: Api
        function onCatalogoCambio() { pagina.cargar() }
        function onInventarioCambio() { pagina.cargar() }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tema.margen
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Button {
                text: "← Volver"
                flat: true
                onClicked: pagina.volver()
            }
            Label {
                text: "Productos"
                color: Tema.texto
                font.pixelSize: Tema.letraTitulo
                font.bold: true
            }
            Label {
                visible: !pagina.compacta
                text: pagina.productos.length + " en el menú"
                color: Tema.textoSuave
            }
            Item { Layout.fillWidth: true }
            Button {
                text: pagina.compacta ? "Nuevo" : "Nuevo producto"
                highlighted: true
                onClicked: dialogo.abrirNuevo()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            TextField {
                id: busqueda
                Layout.fillWidth: true
                Layout.maximumWidth: 360
                placeholderText: "Buscar…"
                onTextChanged: pagina.filtrar()
            }
            ComboBox {
                id: comboFiltro
                textRole: "texto"
                valueRole: "valor"
                model: [
                    { texto: "Todas las categorías", valor: "todas" },
                    { texto: "Alimentos", valor: "alimentos" },
                    { texto: "Bebidas", valor: "bebidas" },
                    { texto: "Dulcería", valor: "dulceria" }
                ]
                onActivated: pagina.filtrar()
            }
            Item { Layout.fillWidth: true }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Tema.radio
            color: Tema.superficie
            border.color: Tema.borde
            clip: true

            ListView {
                id: lista
                anchors.fill: parent
                anchors.margins: 1
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: pagina.visibles
                ScrollBar.vertical: ScrollBar {}

                delegate: Rectangle {
                    id: fila
                    required property var modelData
                    required property int index
                    width: ListView.view.width
                    implicitHeight: contenido.implicitHeight + 20
                    color: fila.index % 2 === 0 ? Tema.superficie : Tema.superficieAlta

                    TapHandler {
                        onDoubleTapped: dialogo.abrirEdicion(fila.modelData)
                    }

                    RowLayout {
                        id: contenido
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 16
                        anchors.rightMargin: 8
                        spacing: 12

                        // El ancho preferido pequeño hace que esta columna tome solo lo
                        // que sobra: así un nombre largo se recorta y no empuja el precio
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.preferredWidth: 100
                            spacing: 2
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                Label {
                                    Layout.fillWidth: true
                                    Layout.maximumWidth: implicitWidth
                                    text: fila.modelData.nombre
                                    color: Tema.texto
                                    font.bold: true
                                    font.pixelSize: 15
                                    elide: Text.ElideRight
                                }
                                Etiqueta {
                                    // En teléfono el nombre necesita el espacio; la categoría está en el filtro
                                    visible: !pagina.compacta
                                    texto: Tema.nombreCategoria(fila.modelData.categoria)
                                    tono: Tema.colorCategoria(fila.modelData.categoria)
                                }
                                Etiqueta {
                                    visible: !fila.modelData.disponible
                                    texto: "Sin stock"
                                    tono: Tema.peligro
                                }
                                Item { Layout.fillWidth: true }
                            }
                            Label {
                                Layout.fillWidth: true
                                visible: text !== ""
                                text: (fila.modelData.variante_de ? "Variante de " + fila.modelData.variante_de + " · " : "")
                                      + (fila.modelData.receta.length === 0 ? "Sin receta"
                                         : fila.modelData.receta.map(i => i.cantidad + " " + i.unidad_medida + " " + i.nombre).join(", "))
                                color: Tema.textoSuave
                                font.pixelSize: Tema.letraChica
                                elide: Text.ElideRight
                            }
                        }
                        Label {
                            text: Tema.dinero(fila.modelData.precio)
                            color: Tema.texto
                            font.bold: true
                        }
                        Button {
                            visible: !pagina.compacta
                            text: "Editar"
                            flat: true
                            onClicked: dialogo.abrirEdicion(fila.modelData)
                        }
                        Button {
                            visible: !pagina.compacta
                            text: "Eliminar"
                            flat: true
                            // Sigue activo para explicar por qué no se puede
                            opacity: fila.modelData.motivo_sin_baja ? 0.5 : 1
                            onClicked: pagina.pedirEliminar(fila.modelData)
                            ToolTip.visible: hovered && fila.modelData.motivo_sin_baja !== ""
                            ToolTip.text: fila.modelData.motivo_sin_baja
                        }
                        ToolButton {
                            visible: pagina.compacta
                            text: "⋮"
                            onClicked: menuFila.open()
                            Menu {
                                id: menuFila
                                MenuItem { text: "Editar"; onTriggered: dialogo.abrirEdicion(fila.modelData) }
                                MenuItem { text: "Eliminar"; onTriggered: pagina.pedirEliminar(fila.modelData) }
                            }
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: lista.count === 0 && !pagina.cargando
                    text: pagina.productos.length === 0 ? "Todavía no hay productos" : "Nada coincide con la búsqueda"
                    color: Tema.textoSuave
                }
            }
        }
    }

    DialogoProducto {
        id: dialogo
        productos: pagina.productos
        ingredientes: pagina.ingredientes
    }

    DialogoConfirmar {
        id: confirmarBaja
        titulo: "¿Eliminar producto?"
        textoAceptar: "Eliminar"
        peligroso: true
        onConfirmado: Api.eliminarProducto(pagina.porEliminar.id_producto, function (r) {
            if (r.codigo === "OK")
                Avisos.exito(r.mensaje)
            else
                Avisos.error(r.mensaje)
        })
    }
}
