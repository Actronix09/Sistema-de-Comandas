pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Caja: el cobro lo maneja el cajero/administrador. A la izquierda las cuentas
// con su saldo; a la derecha el detalle de la elegida, sus pagos, el formulario
// para cobrar y el cierre. En pantallas angostas se ve una cosa a la vez.
// Las reglas (no cobrar de más, propina ≤ monto, cerrar sin adeudo ni comandas
// abiertas) las revisa la base; aquí solo se muestran sus mensajes.
// Solo el nivel administrador ve los botones para cancelar comandas y cuentas.
Item {
    id: caja

    signal volver()

    property bool puedeVolver: true     // el cajero entra directo aquí: no hay a dónde volver
    property var cuentas: []
    property bool soloAbiertas: true
    property var elegida: null          // la cuenta del panel derecho
    property var detalle: null          // { lineas, pagos }
    property bool ocupada: false
    property string error: ""

    readonly property bool esAdmin: Api.esAdministrador
    readonly property bool abierta: elegida !== null && elegida.estado === "abierta"
    readonly property bool compacta: width < 760
    // El panel de detalle puede ser angosto aunque la ventana no lo sea
    readonly property bool lineaCompacta: desplazableDetalle.width < 520
    readonly property var visibles: cuentas.filter(c => !soloAbiertas || c.estado === "abierta")
    readonly property var grupos: selector.agrupar(visibles)

    function cargar() {
        Api.listarCuentasCaja(function (r) {
            if (!caja)
                return
            if (r.codigo !== "OK") {
                Avisos.error(r.mensaje)
                return
            }
            // Abiertas primero; dentro, las de mayor saldo arriba
            caja.cuentas = r.datos.sort((a, b) => (Number(b.estado === "abierta") - Number(a.estado === "abierta"))
                                                  || (b.saldo - a.saldo))
            if (caja.elegida) {
                caja.elegida = caja.cuentas.find(c => c.secuencial === caja.elegida.secuencial) || null
                caja.cargarDetalle()
            }
        })
    }

    function elegir(cuenta) {
        elegida = cuenta
        detalle = null
        error = ""
        campoMonto.text = cuenta.saldo > 0 ? cuenta.saldo.toFixed(2) : ""
        campoPropina.text = ""
        cargarDetalle()
    }

    function cargarDetalle() {
        if (!elegida)
            return
        Api.detalleCuenta(elegida.secuencial, function (r) {
            if (!caja)
                return
            if (r.codigo === "OK" && caja.elegida && r.datos.secuencial === caja.elegida.secuencial)
                caja.detalle = r.datos
        })
    }

    function cobrar() {
        error = ""
        ocupada = true
        Api.registrarPago(elegida.secuencial, {
            monto: campoMonto.text, forma_pago: comboForma.currentValue, monto_propina: campoPropina.text || 0
        }, function (r) {
            if (!caja)
                return
            caja.ocupada = false
            if (r.codigo !== "OK") {
                caja.error = r.mensaje
                return
            }
            Avisos.exito(r.mensaje)
            campoMonto.text = r.datos.saldo > 0 ? r.datos.saldo.toFixed(2) : ""
            campoPropina.text = ""
        })
    }

    function cerrar() {
        error = ""
        Api.cerrarCuenta(elegida.secuencial, function (r) {
            if (!caja)
                return
            if (r.codigo !== "OK") {
                caja.error = r.mensaje
                return
            }
            Avisos.exito(r.mensaje)
        })
    }

    function cancelarComandas(ids) {
        error = ""
        Api.cancelarComandas(ids, function (r) {
            if (!caja)
                return
            if (r.codigo !== "OK") {
                caja.error = r.mensaje
                return
            }
            Avisos.exito(r.mensaje)
            campoMonto.text = r.datos.saldo > 0 ? r.datos.saldo.toFixed(2) : ""
        })
    }

    function pedirCancelarCuenta() {
        const vivas = detalle.lineas.filter(l => l.estado !== "cancelada").reduce((n, l) => n + l.cantidad, 0)
        confirmarCancelarCuenta.mensaje = "La cuenta " + elegida.cuenta + " se cerrará"
                + (vivas === 0 ? "." : (vivas === 1 ? " y se cancelará su comanda." : " y se cancelarán sus " + vivas + " comandas."))
        confirmarCancelarCuenta.open()
    }

    function cancelarCuenta() {
        error = ""
        Api.cancelarCuenta(elegida.secuencial, function (r) {
            if (!caja)
                return
            if (r.codigo !== "OK") {
                caja.error = r.mensaje
                return
            }
            Avisos.exito(r.mensaje)
        })
    }

    Component.onCompleted: cargar()

    Connections {
        target: Api
        function onCuentasCambiaron() { caja.cargar() }
        function onComandasCambiaron() { caja.cargar() }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: caja.compacta ? Tema.espacio : Tema.margen
        spacing: Tema.margen

        // ============================ Cuentas ============================
        ColumnLayout {
            visible: !caja.compacta || caja.elegida === null
            Layout.fillWidth: caja.compacta
            Layout.preferredWidth: caja.compacta ? -1 : Math.min(420, caja.width * 0.42)
            Layout.fillHeight: true
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Button {
                    visible: caja.puedeVolver
                    text: "← Volver"
                    flat: true
                    onClicked: caja.volver()
                }
                Label {
                    Layout.fillWidth: true
                    text: "Caja"
                    color: Tema.texto
                    font.pixelSize: Tema.letraTitulo
                    font.bold: true
                }
                Button {
                    text: "Abiertas"
                    highlighted: caja.soloAbiertas
                    flat: !highlighted
                    onClicked: caja.soloAbiertas = true
                }
                Button {
                    text: "Todas"
                    highlighted: !caja.soloAbiertas
                    flat: !highlighted
                    onClicked: caja.soloAbiertas = false
                }
            }

            SelectorOrden {
                id: selector
                soloMesa: true
            }

            Flickable {
                id: desplazable
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentHeight: columnaCuentas.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}

                Column {
                    id: columnaCuentas
                    width: desplazable.width
                    spacing: 8

                    Repeater {
                        model: caja.grupos
                        delegate: Column {
                            id: grupo
                            required property var modelData
                            width: columnaCuentas.width
                            spacing: 8

                            Label {
                                visible: grupo.modelData.titulo !== ""
                                width: parent.width
                                topPadding: 6
                                text: grupo.modelData.titulo
                                color: Tema.texto
                                font.bold: true
                                elide: Text.ElideRight
                            }

                            Repeater {
                                model: grupo.modelData.rondas
                                delegate: Rectangle {
                                    id: tarjeta
                                    required property var modelData
                                    readonly property bool esElegida: caja.elegida !== null
                                                                      && caja.elegida.secuencial === modelData.secuencial
                                    width: grupo.width
                                    implicitHeight: datosCuenta.implicitHeight + 24
                                    radius: Tema.radio
                                    color: raton.containsMouse ? Tema.superficieAlta : Tema.superficie
                                    border.width: esElegida ? 2 : 1
                                    border.color: esElegida ? Tema.acento : Tema.borde
                                    opacity: modelData.estado === "abierta" ? 1 : 0.6

                                    MouseArea {
                                        id: raton
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: caja.elegir(tarjeta.modelData)
                                    }

                                    GridLayout {
                                        id: datosCuenta
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.verticalCenter: parent.verticalCenter
                                        anchors.margins: 12
                                        columns: 2
                                        rowSpacing: 2
                                        Label {
                                            Layout.fillWidth: true
                                            text: tarjeta.modelData.cuenta
                                            color: Tema.texto
                                            font.pixelSize: Tema.letraMediana
                                            font.bold: true
                                            elide: Text.ElideRight
                                        }
                                        Label {
                                            Layout.alignment: Qt.AlignRight
                                            text: tarjeta.modelData.estado === "abierta" ? Tema.dinero(tarjeta.modelData.saldo)
                                                                                         : (tarjeta.modelData.cancelada ? "Cancelada" : "Cerrada")
                                            color: tarjeta.modelData.saldo > 0 ? Tema.acento : Tema.textoSuave
                                            font.pixelSize: Tema.letraMediana
                                            font.bold: true
                                        }
                                        Label {
                                            Layout.fillWidth: true
                                            text: "Mesa " + tarjeta.modelData.numero_mesa + " · " + tarjeta.modelData.mesero
                                            color: Tema.textoSuave
                                            font.pixelSize: Tema.letraChica
                                            elide: Text.ElideRight
                                        }
                                        Label {
                                            Layout.alignment: Qt.AlignRight
                                            text: tarjeta.modelData.estado === "abierta" ? "saldo" : ""
                                            color: Tema.textoSuave
                                            font.pixelSize: Tema.letraChica
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: caja.visibles.length === 0
                    text: caja.soloAbiertas ? "No hay cuentas abiertas" : "No hay cuentas"
                    color: Tema.textoSuave
                }
            }
        }

        // ============================ Detalle ============================
        Rectangle {
            visible: !caja.compacta || caja.elegida !== null
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Tema.radio
            color: Tema.superficie
            border.color: Tema.borde

            Label {
                anchors.centerIn: parent
                visible: caja.elegida === null
                text: "Elige una cuenta para cobrarla"
                color: Tema.textoSuave
            }

            Flickable {
                id: desplazableDetalle
                visible: caja.elegida !== null
                anchors.fill: parent
                anchors.margins: Tema.margen
                clip: true
                contentHeight: columnaDetalle.implicitHeight
                boundsBehavior: Flickable.StopAtBounds
                ScrollBar.vertical: ScrollBar {}

                ColumnLayout {
                    id: columnaDetalle
                    width: desplazableDetalle.width
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        Button {
                            visible: caja.compacta
                            text: "← Cuentas"
                            flat: true
                            onClicked: caja.elegida = null
                        }
                        Label {
                            Layout.fillWidth: true
                            text: caja.elegida ? caja.elegida.cuenta : ""
                            color: Tema.texto
                            font.pixelSize: 22
                            font.bold: true
                            wrapMode: Text.WordWrap
                        }
                        Etiqueta {
                            visible: caja.elegida !== null && !caja.abierta
                            texto: caja.elegida && caja.elegida.cancelada ? "Cancelada" : "Cerrada"
                            tono: caja.elegida && caja.elegida.cancelada ? Tema.peligro : Tema.textoSuave
                        }
                    }

                    Label {
                        text: caja.elegida ? "Mesa " + caja.elegida.numero_mesa + " · la atiende " + caja.elegida.mesero : ""
                        color: Tema.textoSuave
                        font.pixelSize: Tema.letraChica
                    }

                    // Consumo
                    Repeater {
                        model: caja.detalle ? caja.detalle.lineas : []
                        // En angosto el estado y "Cancelar" bajan a una segunda fila
                        delegate: GridLayout {
                            id: linea
                            required property var modelData
                            Layout.fillWidth: true
                            columnSpacing: 8
                            rowSpacing: 0
                            Label {
                                Layout.row: 0
                                Layout.column: 0
                                Layout.preferredWidth: 30
                                text: linea.modelData.cantidad + "×"
                                color: Tema.texto
                                font.bold: true
                            }
                            Label {
                                Layout.row: 0
                                Layout.column: 1
                                Layout.fillWidth: true
                                text: linea.modelData.producto
                                color: linea.modelData.estado === "cancelada" ? Tema.textoSuave : Tema.texto
                                font.strikeout: linea.modelData.estado === "cancelada"
                                wrapMode: Text.WordWrap
                            }
                            ChipEstado {
                                Layout.row: caja.lineaCompacta ? 1 : 0
                                Layout.column: caja.lineaCompacta ? 1 : 2
                                estado: linea.modelData.estado
                            }
                            Label {
                                Layout.row: 0
                                Layout.column: caja.lineaCompacta ? 2 : 3
                                Layout.preferredWidth: 84
                                horizontalAlignment: Text.AlignRight
                                text: linea.modelData.estado === "cancelada" ? "—"
                                      : Tema.dinero(linea.modelData.precio * linea.modelData.cantidad)
                                color: Tema.texto
                            }
                            Button {
                                Layout.row: caja.lineaCompacta ? 1 : 0
                                Layout.column: caja.lineaCompacta ? 2 : 4
                                Layout.alignment: Qt.AlignRight
                                visible: caja.esAdmin && caja.abierta && linea.modelData.estado !== "cancelada"
                                text: "Cancelar"
                                flat: true
                                onClicked: dialogoCancelar.abrir(linea.modelData)
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 1
                        color: Tema.borde
                    }

                    // Totales, como v_cuenta
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        rowSpacing: 2
                        Label { Layout.fillWidth: true; text: "Consumo"; color: Tema.textoSuave }
                        Label {
                            Layout.alignment: Qt.AlignRight
                            text: caja.elegida ? Tema.dinero(caja.elegida.total) : ""
                            color: Tema.texto
                        }
                        Label { Layout.fillWidth: true; text: "Pagado (sin propinas)"; color: Tema.textoSuave }
                        Label {
                            Layout.alignment: Qt.AlignRight
                            text: caja.elegida ? Tema.dinero(caja.elegida.pagado) : ""
                            color: Tema.texto
                        }
                        Label {
                            Layout.fillWidth: true
                            text: "Saldo"
                            color: Tema.texto
                            font.pixelSize: 18
                            font.bold: true
                        }
                        Label {
                            Layout.alignment: Qt.AlignRight
                            text: caja.elegida ? Tema.dinero(caja.elegida.saldo) : ""
                            color: Tema.acento
                            font.pixelSize: 24
                            font.bold: true
                        }
                    }

                    // Pagos registrados
                    Label {
                        visible: caja.detalle !== null && caja.detalle.pagos.length > 0
                        Layout.topMargin: 6
                        text: "Pagos"
                        color: Tema.texto
                        font.bold: true
                    }
                    Repeater {
                        model: caja.detalle ? caja.detalle.pagos : []
                        delegate: Label {
                            id: pago
                            required property var modelData
                            Layout.fillWidth: true
                            text: Tema.hora(pago.modelData.fecha_hora) + " · " + pago.modelData.forma_pago + " · "
                                  + Tema.dinero(pago.modelData.monto)
                                  + (pago.modelData.monto_propina ? " (propina " + Tema.dinero(pago.modelData.monto_propina)
                                                                     + " para " + pago.modelData.mesero_propina + ")" : "")
                            color: Tema.textoSuave
                            wrapMode: Text.WordWrap
                        }
                    }

                    // Cobrar
                    Rectangle {
                        visible: caja.elegida !== null && caja.elegida.estado === "abierta"
                        Layout.fillWidth: true
                        Layout.topMargin: 6
                        implicitHeight: formulario.implicitHeight + 24
                        radius: Tema.radio
                        color: Tema.superficieAlta

                        ColumnLayout {
                            id: formulario
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 12
                            spacing: 8

                            Label {
                                text: "Registrar pago"
                                color: Tema.texto
                                font.bold: true
                            }
                            GridLayout {
                                Layout.fillWidth: true
                                columns: caja.compacta ? 1 : 3
                                columnSpacing: 8
                                TextField {
                                    id: campoMonto
                                    Layout.fillWidth: true
                                    placeholderText: "Monto (incluye propina)"
                                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                                }
                                TextField {
                                    id: campoPropina
                                    Layout.fillWidth: true
                                    placeholderText: "Propina (opcional)"
                                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                                }
                                ComboBox {
                                    id: comboForma
                                    Layout.fillWidth: true
                                    model: [
                                        { texto: "Efectivo", valor: "efectivo" },
                                        { texto: "Tarjeta", valor: "tarjeta" },
                                        { texto: "Transferencia", valor: "transferencia" }
                                    ]
                                    textRole: "texto"
                                    valueRole: "valor"
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                text: "El monto incluye la propina: al saldo solo se abona el monto menos la propina."
                                color: Tema.textoSuave
                                font.pixelSize: Tema.letraChica
                                wrapMode: Text.WordWrap
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                Button {
                                    text: "Cerrar cuenta"
                                    flat: true
                                    onClicked: caja.cerrar()
                                }
                                Item { Layout.fillWidth: true }
                                Button {
                                    text: caja.ocupada ? "Registrando…" : "Cobrar"
                                    highlighted: true
                                    enabled: !caja.ocupada && campoMonto.text.trim().length > 0
                                    onClicked: caja.cobrar()
                                }
                            }
                        }
                    }

                    // Cancelar la cuenta completa
                    RowLayout {
                        visible: caja.esAdmin && caja.abierta && caja.detalle !== null
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            visible: caja.detalle !== null && caja.detalle.pagos.length > 0
                            text: "Con pagos registrados no se puede cancelar la cuenta."
                            color: Tema.textoSuave
                            font.pixelSize: Tema.letraChica
                            wrapMode: Text.WordWrap
                        }
                        Item {
                            Layout.fillWidth: true
                            visible: caja.detalle !== null && caja.detalle.pagos.length === 0
                        }
                        Button {
                            text: "Cancelar cuenta"
                            flat: true
                            enabled: caja.detalle !== null && caja.detalle.pagos.length === 0
                            onClicked: caja.pedirCancelarCuenta()
                        }
                    }

                    Label {
                        Layout.fillWidth: true
                        visible: caja.error !== ""
                        text: caja.error
                        color: Tema.peligro
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }

    DialogoCancelarComandas {
        id: dialogoCancelar
        onConfirmado: (ids) => caja.cancelarComandas(ids)
    }

    DialogoConfirmar {
        id: confirmarCancelarCuenta
        titulo: "¿Cancelar la cuenta?"
        textoAceptar: "Cancelar cuenta"
        textoRechazar: "Regresar"
        peligroso: true
        onConfirmado: caja.cancelarCuenta()
    }
}
