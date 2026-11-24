pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Alta y edición de un paquete: título, descripción, monto y qué productos
// incluye. Cuánto de cada producto se fija al asignar el paquete a un evento
// (en el acuerdo de ese evento), porque depende de los asistentes.
Dialog {
    id: dialogo

    property var paquete: null         // null = alta
    property var productos: []
    property var elegidos: ({})        // id_producto → true
    property bool guardando: false
    property string error: ""

    readonly property int largo: Api.juntarDetalles(campoTitulo.text, campoDescripcion.text).length
    readonly property int cuantos: Object.keys(elegidos).length
    readonly property var visibles: {
        const texto = busqueda.text.trim().toLowerCase()
        return productos.filter(p => texto === "" || p.nombre.toLowerCase().indexOf(texto) >= 0)
    }

    function abrirNuevo() {
        paquete = null
        campoTitulo.text = ""
        campoDescripcion.text = ""
        campoMonto.text = ""
        elegidos = ({})
        abrir()
    }

    function abrirEdicion(datos) {
        paquete = datos
        campoTitulo.text = datos.titulo
        campoDescripcion.text = datos.descripcion
        campoMonto.text = datos.monto.toFixed(2)
        const mapa = {}
        for (const p of datos.productos)
            mapa[p.id_producto] = true
        elegidos = mapa
        abrir()
    }

    function abrir() {
        busqueda.text = ""
        error = ""
        open()
    }

    function alternar(idProducto) {
        const mapa = Object.assign({}, elegidos)
        if (mapa[idProducto])
            delete mapa[idProducto]
        else
            mapa[idProducto] = true
        elegidos = mapa
    }

    function guardar() {
        if (campoTitulo.text.trim() === "") {
            error = "Escribe el título del paquete"
            return
        }
        if (campoTitulo.text.indexOf(": ") >= 0) {
            error = "El título no puede llevar «: » (así se separa de la descripción)"
            return
        }
        if (largo > 255) {
            error = "El título y la descripción juntos admiten hasta 255 caracteres"
            return
        }
        guardando = true
        error = ""
        const datos = { titulo: campoTitulo.text, descripcion: campoDescripcion.text, monto: campoMonto.text || 0,
                        productos: Object.keys(elegidos).map(Number) }
        if (paquete)
            datos.numero_paquete = paquete.numero_paquete
        Api.guardarPaquete(datos, function (r) {
            if (!dialogo)
                return
            dialogo.guardando = false
            if (r.codigo === "OK") {
                dialogo.close()
                Avisos.exito(r.mensaje)
            } else {
                dialogo.error = r.mensaje
            }
        })
    }

    title: paquete ? "Editar paquete" : "Nuevo paquete"
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(560, parent ? parent.width - 32 : 560)
    height: Math.min(680, parent ? parent.height - 32 : 680)

    contentItem: ColumnLayout {
        spacing: 6
        enabled: !dialogo.guardando

        Label { text: "Título"; color: Tema.texto; font.bold: true }
        TextField {
            id: campoTitulo
            Layout.fillWidth: true
            placeholderText: "p. ej. Paquete Oro"
        }

        Label { text: "Descripción"; color: Tema.texto; font.bold: true }
        TextArea {
            id: campoDescripcion
            Layout.fillWidth: true
            Layout.preferredHeight: Math.max(64, implicitHeight)
            wrapMode: TextEdit.Wrap
            placeholderText: "Qué incluye, para el cliente: cena de tres tiempos, barra libre…"
        }
        Label {
            Layout.alignment: Qt.AlignRight
            text: dialogo.largo + " / 255"
            color: dialogo.largo > 255 ? Tema.peligro : Tema.textoSuave
            font.pixelSize: 11
        }

        Label { text: "Monto del paquete"; color: Tema.texto; font.bold: true }
        TextField {
            id: campoMonto
            Layout.fillWidth: true
            placeholderText: "0.00"
            inputMethodHints: Qt.ImhFormattedNumbersOnly
            validator: RegularExpressionValidator { regularExpression: /^\d{0,8}([.,]\d{0,2})?$/ }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 6
            Label {
                text: "Productos incluidos (" + dialogo.cuantos + ")"
                color: Tema.texto
                font.bold: true
            }
            Item { Layout.fillWidth: true }
            TextField {
                id: busqueda
                Layout.preferredWidth: 180
                placeholderText: "Buscar…"
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 140
            radius: Tema.radio
            color: Tema.superficie
            border.color: Tema.borde
            clip: true

            ListView {
                anchors.fill: parent
                anchors.margins: 1
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: dialogo.visibles
                ScrollBar.vertical: ScrollBar {}
                delegate: CheckDelegate {
                    id: opcion
                    required property var modelData
                    width: ListView.view.width
                    text: opcion.modelData.nombre + "  ·  " + Tema.nombreCategoria(opcion.modelData.categoria)
                    checked: dialogo.elegidos[opcion.modelData.id_producto] === true
                    onClicked: dialogo.alternar(opcion.modelData.id_producto)
                }
            }
        }
        Label {
            Layout.fillWidth: true
            text: "La cantidad de cada producto se pone al asignar el paquete a un evento."
            color: Tema.textoSuave
            font.pixelSize: Tema.letraChica
            wrapMode: Text.WordWrap
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
