pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// "Confirmar Orden". En el sistema en terminal solo se pedía el número de mesa
// como texto libre; tu esquema exige además la cuenta (evento, secuencial),
// que puede ser una ya abierta en esa mesa o una nueva con su nombre.
Dialog {
    id: dialogo

    property var lineas: []        // [{ id_producto, nombre, precio, cantidad, detalles }]
    property bool enviando: false
    property string error: ""
    property var mesas: []
    property var cuentas: []
    signal confirmar(int numeroMesa, var secuencial)

    readonly property int unidades: lineas.reduce((suma, l) => suma + l.cantidad, 0)
    readonly property real total: lineas.reduce((suma, l) => suma + l.precio * l.cantidad, 0)

    function cargarCuentas() {
        const mesa = mesas[comboMesa.currentIndex]
        if (!mesa)
            return
        cuentas = []
        Api.listarCuentas(mesa.numero_mesa, function (r) {
            if (!dialogo)
                return
            if (r.codigo !== "OK") {
                dialogo.error = r.mensaje
                return
            }
            const abiertas = r.datos.map(c => ({ texto: c.cuenta + " · lleva " + Tema.dinero(c.total), secuencial: c.secuencial }))
            dialogo.cuentas = abiertas.concat([{ texto: "Abrir una cuenta nueva", secuencial: null }])
            comboCuenta.currentIndex = 0
        })
    }

    readonly property bool cuentaNueva: cuentas.length > 0 && cuentas[comboCuenta.currentIndex]
                                         && cuentas[comboCuenta.currentIndex].secuencial === null

    function aceptar() {
        const mesa = mesas[comboMesa.currentIndex]
        const cuenta = cuentas[comboCuenta.currentIndex]
        if (!mesa || !cuenta) {
            error = "Elige la mesa y la cuenta"
            return
        }
        error = ""
        if (cuenta.secuencial !== null) {
            confirmar(mesa.numero_mesa, cuenta.secuencial)
            return
        }
        // Cuenta nueva: primero se abre con su nombre y después se envía la orden
        enviando = true
        Api.crearCuenta(mesa.numero_mesa, campoNombre.text, function (r) {
            if (!dialogo)
                return
            dialogo.enviando = false
            if (r.codigo !== "OK") {
                dialogo.error = r.mensaje
                return
            }
            dialogo.confirmar(mesa.numero_mesa, r.datos.secuencial)
        })
    }

    title: "Confirmar orden"
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(560, parent ? parent.width - 48 : 560)
    closePolicy: enviando ? Popup.NoAutoClose : (Popup.CloseOnEscape | Popup.CloseOnPressOutside)

    onOpened: {
        error = ""
        campoNombre.text = ""
        mesas = []
        cuentas = []
        Api.listarMesas(function (r) {
            if (!dialogo)
                return
            if (r.codigo !== "OK") {
                dialogo.error = r.mensaje
                return
            }
            dialogo.mesas = r.datos
            comboMesa.currentIndex = 0
            dialogo.cargarCuentas()
        })
    }

    contentItem: ColumnLayout {
        spacing: 8

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 12
            rowSpacing: 8

            Label {
                text: "Mesa"
                color: Tema.texto
                font.bold: true
            }
            ComboBox {
                id: comboMesa
                Layout.fillWidth: true
                model: dialogo.mesas.map(m => m.mia ? "Mesa " + m.numero_mesa + " (tuya)"
                                                    : "Mesa " + m.numero_mesa + " · la atiende " + m.mesero)
                enabled: !dialogo.enviando
                onActivated: dialogo.cargarCuentas()
            }

            Label {
                text: "Cuenta"
                color: Tema.texto
                font.bold: true
            }
            ComboBox {
                id: comboCuenta
                Layout.fillWidth: true
                model: dialogo.cuentas
                textRole: "texto"
                enabled: !dialogo.enviando && dialogo.cuentas.length > 0
            }

            Label {
                visible: dialogo.cuentaNueva
                text: "Nombre"
                color: Tema.texto
                font.bold: true
            }
            TextField {
                id: campoNombre
                visible: dialogo.cuentaNueva
                Layout.fillWidth: true
                placeholderText: "Para guiarte, p. ej. Novios o Mesa del fondo"
                maximumLength: 60
                enabled: !dialogo.enviando
            }
        }

        Label {
            Layout.topMargin: 6
            text: "Resumen"
            color: Tema.texto
            font.bold: true
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: resumen.implicitHeight + 16
            radius: 8
            color: Tema.superficieAlta

            ColumnLayout {
                id: resumen
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 8
                spacing: 4

                Repeater {
                    model: dialogo.lineas
                    delegate: RowLayout {
                        id: renglon
                        required property var modelData
                        Layout.fillWidth: true
                        Label {
                            Layout.preferredWidth: 30
                            text: renglon.modelData.cantidad + "×"
                            color: Tema.texto
                            font.bold: true
                        }
                        Label {
                            Layout.fillWidth: true
                            text: renglon.modelData.nombre
                                  + (renglon.modelData.detalles ? "  —  “" + renglon.modelData.detalles + "”" : "")
                            color: Tema.texto
                            elide: Text.ElideRight
                        }
                        Label {
                            text: Tema.dinero(renglon.modelData.precio * renglon.modelData.cantidad)
                            color: Tema.texto
                        }
                    }
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Label {
                text: "Se crearán " + dialogo.unidades + (dialogo.unidades === 1 ? " comanda" : " comandas")
                      + " (una por unidad)"
                color: Tema.textoSuave
                font.pixelSize: Tema.letraChica
            }
            Item { Layout.fillWidth: true }
            Label {
                text: "Total " + Tema.dinero(dialogo.total)
                color: Tema.acento
                font.pixelSize: 18
                font.bold: true
            }
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
            enabled: !dialogo.enviando
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
        // ActionRole: no cierra el diálogo; se cierra cuando el servidor responde OK
        Button {
            text: dialogo.enviando ? "Enviando…" : "Enviar a cocina y bar"
            highlighted: true
            enabled: !dialogo.enviando && dialogo.cuentas.length > 0
            DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
            onClicked: dialogo.aceptar()
        }
    }
}
