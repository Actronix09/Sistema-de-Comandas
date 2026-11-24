pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// El detalle de una cuenta de cualquier evento, solo para consultar: lo que se
// pidió (agrupado como en la caja), cómo se cobró y los pagos. Lo abre la ficha
// de un evento para revisar sus cuentas, también las de eventos pasados.
Dialog {
    id: dialogo

    property var cuenta: null       // la fila de listarCuentasCaja
    property int numeroEvento: 0
    property var detalle: null
    property bool cargando: false

    function abrir(datosCuenta, evento) {
        cuenta = datosCuenta
        numeroEvento = evento
        detalle = null
        cargando = true
        open()
        Api.detalleCuenta(datosCuenta.secuencial, function (r) {
            if (!dialogo)
                return
            dialogo.cargando = false
            if (r.codigo === "OK")
                dialogo.detalle = r.datos
            else
                Avisos.error(r.mensaje)
        }, evento)
    }

    title: cuenta ? cuenta.cuenta : ""
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(560, parent ? parent.width - 32 : 560)
    height: Math.min(640, parent ? parent.height - 32 : 640)
    // Botón propio y no standardButtons: Qt pone el texto de los estándar en
    // inglés ("Close") porque la app no carga sus traducciones
    footer: DialogButtonBox {
        Button {
            text: "Cerrar"
            flat: true
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
    }

    contentItem: ColumnLayout {
        spacing: 8

        Label {
            Layout.fillWidth: true
            text: dialogo.cuenta ? "Mesa " + dialogo.cuenta.numero_mesa + " · la atendió " + dialogo.cuenta.mesero : ""
            color: Tema.textoSuave
            wrapMode: Text.WordWrap
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Tema.espacio
            Repeater {
                model: dialogo.cuenta ? [
                    { texto: "Consumo", valor: dialogo.cuenta.total },
                    { texto: "Pagado", valor: dialogo.cuenta.pagado },
                    { texto: "Saldo", valor: dialogo.cuenta.saldo }
                ] : []
                delegate: ColumnLayout {
                    id: cifra
                    required property var modelData
                    spacing: 0
                    Label { text: cifra.modelData.texto; color: Tema.textoSuave; font.pixelSize: Tema.letraChica }
                    Label { text: Tema.dinero(cifra.modelData.valor); color: Tema.texto; font.bold: true; font.pixelSize: Tema.letraMediana }
                }
            }
        }

        Label {
            visible: dialogo.cargando
            text: "Cargando…"
            color: Tema.textoSuave
        }

        ScrollView {
            id: desplazable
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: desplazable.availableWidth
                spacing: 4

                Label { text: "Consumo"; color: Tema.texto; font.bold: true; visible: dialogo.detalle !== null }
                Repeater {
                    model: dialogo.detalle ? dialogo.detalle.lineas : []
                    delegate: RowLayout {
                        id: linea
                        required property var modelData
                        Layout.fillWidth: true
                        spacing: 8
                        Label {
                            Layout.preferredWidth: 34
                            text: linea.modelData.cantidad + "×"
                            color: Tema.texto
                            font.bold: true
                        }
                        Label {
                            Layout.fillWidth: true
                            text: linea.modelData.producto
                                  + (linea.modelData.tipo_cobro === "paquete" ? "  · incluido en paquete" : "")
                                  + (linea.modelData.tipo_cobro === "anfitrion"
                                     ? (linea.modelData.de_otra_cuenta ? "  · cargo de " + linea.modelData.de_otra_cuenta
                                                                       : "  · lo paga el anfitrión") : "")
                            color: linea.modelData.estado === "cancelada" ? Tema.textoSuave : Tema.texto
                            font.strikeout: linea.modelData.estado === "cancelada"
                            wrapMode: Text.WordWrap
                        }
                        ChipEstado { estado: linea.modelData.estado }
                        Label {
                            Layout.preferredWidth: 84
                            horizontalAlignment: Text.AlignRight
                            text: linea.modelData.estado === "cancelada" ? "—"
                                  : !linea.modelData.cuenta_aqui ? "(anfitrión)"
                                  : linea.modelData.tipo_cobro === "paquete" ? "Incluido"
                                  : Tema.dinero(linea.modelData.precio * linea.modelData.cantidad)
                            color: Tema.texto
                        }
                    }
                }

                Label {
                    Layout.topMargin: 10
                    visible: dialogo.detalle !== null
                    text: "Pagos"
                    color: Tema.texto
                    font.bold: true
                }
                Label {
                    visible: dialogo.detalle !== null && dialogo.detalle.pagos.length === 0
                    text: "Sin pagos"
                    color: Tema.textoSuave
                }
                Repeater {
                    model: dialogo.detalle ? dialogo.detalle.pagos : []
                    delegate: RowLayout {
                        id: pago
                        required property var modelData
                        Layout.fillWidth: true
                        Label {
                            Layout.fillWidth: true
                            text: Tema.hora(pago.modelData.fecha_hora) + " · " + pago.modelData.forma_pago
                                  + (pago.modelData.monto_propina ? " · propina " + Tema.dinero(pago.modelData.monto_propina)
                                                                     + " para " + pago.modelData.mesero_propina : "")
                            color: Tema.textoSuave
                            wrapMode: Text.WordWrap
                        }
                        Label {
                            text: Tema.dinero(pago.modelData.monto)
                            color: Tema.texto
                            font.bold: true
                        }
                    }
                }
            }
        }
    }
}
