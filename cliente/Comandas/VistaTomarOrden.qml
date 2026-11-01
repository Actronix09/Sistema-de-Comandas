pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// "Tomar Nueva Orden" + "Confirmar Orden". La orden se arma aquí, en el
// cliente; al confirmarla, el servidor crea una comanda por cada unidad.
// Todo se hace con toques o clics: la interfaz no depende del teclado.
Item {
    id: vista

    property var productos: []        // lo que regresó Api.listarProductos
    property var visibles: []         // después de filtrar por categoría y búsqueda
    property string categoria: "todas"
    property bool cargando: false
    // Los enlaces que leen el carrito dependen de esta versión, que sube en
    // cada cambio; así las tarjetas y el total se recalculan.
    property int versionCarrito: 0
    // En pantallas angostas (teléfono) la orden va debajo del catálogo
    readonly property bool compacta: width < 700

    ListModel { id: carrito }

    function cargar() {
        cargando = true
        Api.listarProductos(function (r) {
            // La respuesta llega después; si la pantalla ya se cerró (cerraron
            // sesión mientras tanto), su id vale null y no hay nada que actualizar.
            if (!vista)
                return
            vista.cargando = false
            if (r.codigo !== "OK") {
                Avisos.error(r.mensaje)
                return
            }
            vista.productos = r.datos
            vista.filtrar()
            vista.ajustarCarritoAlStock()
        })
    }

    function filtrar() {
        const texto = busqueda.text.trim().toLowerCase()
        visibles = productos.filter(p =>
            (categoria === "todas" || p.categoria === categoria)
            && (texto === "" || p.nombre.toLowerCase().indexOf(texto) >= 0
                || p.ingredientes.join(" ").toLowerCase().indexOf(texto) >= 0))
    }

    function indiceEnCarrito(idProducto) {
        for (let i = 0; i < carrito.count; i++)
            if (carrito.get(i).id_producto === idProducto)
                return i
        return -1
    }

    function cantidadDe(idProducto) {
        const i = indiceEnCarrito(idProducto)
        return i < 0 ? 0 : carrito.get(i).cantidad
    }

    function agregar(producto) {
        if (!producto.disponible) {
            Avisos.error(producto.nombre + ": sin stock")
            return
        }
        const i = indiceEnCarrito(producto.id_producto)
        if (i < 0) {
            carrito.append({ id_producto: producto.id_producto, nombre: producto.nombre, precio: producto.precio,
                             cantidad: 1, maximo: producto.maximo, detalles: "" })
        } else if (carrito.get(i).cantidad < producto.maximo) {
            carrito.setProperty(i, "cantidad", carrito.get(i).cantidad + 1)
        } else {
            Avisos.info("Solo alcanza para " + producto.maximo + " de " + producto.nombre)
            return
        }
        versionCarrito++
    }

    function agregarPorId(idProducto) {
        const p = productos.find(x => x.id_producto === idProducto)
        if (p)
            agregar(p)
    }

    function quitar(idProducto) {
        const i = indiceEnCarrito(idProducto)
        if (i < 0)
            return
        const cantidad = carrito.get(i).cantidad
        if (cantidad <= 1)
            carrito.remove(i)
        else
            carrito.setProperty(i, "cantidad", cantidad - 1)
        versionCarrito++
    }

    function vaciar() {
        carrito.clear()
        versionCarrito++
    }

    function total() {
        let suma = 0
        for (let i = 0; i < carrito.count; i++)
            suma += carrito.get(i).precio * carrito.get(i).cantidad
        return suma
    }

    function unidades() {
        let suma = 0
        for (let i = 0; i < carrito.count; i++)
            suma += carrito.get(i).cantidad
        return suma
    }

    // Si el stock bajó (otro mesero ordenó, o el admin lo ajustó), recorta la orden.
    function ajustarCarritoAlStock() {
        for (let i = carrito.count - 1; i >= 0; i--) {
            const p = productos.find(x => x.id_producto === carrito.get(i).id_producto)
            if (!p || p.maximo === 0) {
                carrito.remove(i)
                continue
            }
            carrito.setProperty(i, "maximo", p.maximo)
            if (carrito.get(i).cantidad > p.maximo)
                carrito.setProperty(i, "cantidad", p.maximo)
        }
        versionCarrito++
    }

    function confirmar() {
        if (carrito.count === 0) {
            Avisos.info("Agrega al menos un producto")
            return
        }
        const lineas = []
        for (let i = 0; i < carrito.count; i++) {
            const l = carrito.get(i)
            lineas.push({ id_producto: l.id_producto, nombre: l.nombre, precio: l.precio,
                          cantidad: l.cantidad, detalles: l.detalles })
        }
        dialogo.lineas = lineas
        dialogo.open()
    }

    function enviar(numeroMesa, secuencial) {
        const lineas = dialogo.lineas.map(l => ({ id_producto: l.id_producto, cantidad: l.cantidad, detalles: l.detalles }))
        dialogo.enviando = true
        Api.crearComandas(numeroMesa, secuencial, lineas, function (r) {
            if (!vista)
                return
            dialogo.enviando = false
            if (r.codigo === "OK") {
                dialogo.close()
                vista.vaciar()
                Avisos.exito(r.mensaje)
            } else {
                dialogo.error = r.mensaje   // a diferencia de la terminal, se muestra el motivo real
            }
        })
    }

    Component.onCompleted: cargar()

    Connections {
        target: Api
        function onInventarioCambio() { vista.cargar() }
    }

    GridLayout {
        anchors.fill: parent
        anchors.margins: vista.compacta ? Tema.espacio : Tema.margen
        columns: vista.compacta ? 1 : 2
        columnSpacing: Tema.margen
        rowSpacing: Tema.espacio

        // ======================= Catálogo =======================
        ColumnLayout {
            id: catalogo
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            GridLayout {
                Layout.fillWidth: true
                // El buscador baja a otra línea si no caben las categorías a su lado
                readonly property bool angosta: catalogo.width < 640
                columns: angosta ? 1 : 2
                columnSpacing: 6
                rowSpacing: 4

                // Flow acomoda las categorías en otra línea si no caben
                Flow {
                    Layout.fillWidth: true
                    spacing: 6
                    Repeater {
                        model: [
                            { clave: "todas", texto: "Todos" },
                            { clave: "alimentos", texto: "Alimentos" },
                            { clave: "bebidas", texto: "Bebidas" },
                            { clave: "dulceria", texto: "Dulcería" }
                        ]
                        delegate: Button {
                            id: botonCategoria
                            required property var modelData
                            text: botonCategoria.modelData.texto
                            highlighted: vista.categoria === botonCategoria.modelData.clave
                            flat: !highlighted
                            onClicked: {
                                vista.categoria = botonCategoria.modelData.clave
                                vista.filtrar()
                            }
                        }
                    }
                }
                TextField {
                    id: busqueda
                    Layout.fillWidth: parent.angosta
                    Layout.preferredWidth: parent.angosta ? -1 : 280
                    placeholderText: "Buscar producto o ingrediente…"
                    onTextChanged: vista.filtrar()
                }
            }

            GridView {
                id: cuadricula
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                readonly property int columnas: Math.max(1, Math.floor(width / 230))
                cellWidth: Math.floor(width / columnas)
                cellHeight: 244
                model: vista.visibles
                ScrollBar.vertical: ScrollBar {}

                delegate: Item {
                    id: celda
                    required property var modelData
                    required property int index
                    width: cuadricula.cellWidth
                    height: cuadricula.cellHeight

                    TarjetaProducto {
                        anchors.fill: parent
                        anchors.margins: 6
                        producto: celda.modelData
                        cantidad: {
                            vista.versionCarrito   // dependencia: recalcular al cambiar el carrito
                            return vista.cantidadDe(celda.modelData.id_producto)
                        }
                        onAgregar: vista.agregar(celda.modelData)
                        onQuitar: vista.quitar(celda.modelData.id_producto)
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: !vista.cargando && vista.visibles.length === 0
                    text: "No hay productos que coincidan"
                    color: Tema.textoSuave
                }
                BusyIndicator {
                    anchors.centerIn: parent
                    running: vista.cargando && vista.productos.length === 0
                }
            }
        }

        // ======================= Orden actual =======================
        Rectangle {
            Layout.fillWidth: vista.compacta
            Layout.preferredWidth: vista.compacta ? -1 : Math.min(370, Math.round(vista.width * 0.38))
            Layout.fillHeight: !vista.compacta
            Layout.preferredHeight: vista.compacta ? Math.round(vista.height * 0.42) : -1
            radius: Tema.radio
            color: Tema.superficie
            border.color: Tema.borde

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Tema.margen
                spacing: Tema.espacio

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: "Orden actual"
                        color: Tema.texto
                        font.pixelSize: 20
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    Label {
                        text: {
                            vista.versionCarrito
                            const n = vista.unidades()
                            return n + (n === 1 ? " producto" : " productos")
                        }
                        color: Tema.textoSuave
                    }
                }

                ListView {
                    id: listaOrden
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 8
                    model: carrito
                    ScrollBar.vertical: ScrollBar {}

                    delegate: LineaOrden {
                        id: lineaDelegado
                        width: listaOrden.width
                        onIncrementar: vista.agregarPorId(lineaDelegado.id_producto)
                        onDecrementar: vista.quitar(lineaDelegado.id_producto)
                        onNotaEditada: (texto) => carrito.setProperty(lineaDelegado.index, "detalles", texto)
                    }

                    Label {
                        anchors.centerIn: parent
                        width: parent.width - 32
                        visible: carrito.count === 0
                        text: "Toca un producto para agregarlo a la orden."
                        color: Tema.textoSuave
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 1
                    color: Tema.borde
                }

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: "Total"
                        color: Tema.texto
                        font.pixelSize: 18
                        font.bold: true
                    }
                    Item { Layout.fillWidth: true }
                    Label {
                        text: {
                            vista.versionCarrito
                            return Tema.dinero(vista.total())
                        }
                        color: Tema.acento
                        font.pixelSize: 24
                        font.bold: true
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Button {
                        text: "Vaciar"
                        flat: true
                        enabled: carrito.count > 0
                        onClicked: confirmarVaciar.open()
                    }
                    Button {
                        Layout.fillWidth: true
                        text: "Confirmar orden"
                        highlighted: true
                        enabled: carrito.count > 0
                        onClicked: vista.confirmar()
                    }
                }
            }
        }
    }

    DialogoConfirmarOrden {
        id: dialogo
        onConfirmar: (numeroMesa, secuencial) => vista.enviar(numeroMesa, secuencial)
    }

    DialogoConfirmar {
        id: confirmarVaciar
        titulo: "¿Vaciar la orden?"
        mensaje: "Se quitarán todos los productos de la orden actual."
        textoAceptar: "Vaciar"
        onConfirmado: vista.vaciar()
    }
}
