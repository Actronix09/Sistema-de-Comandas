pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Primer paso para tomar una orden: elegir la mesa y después la cuenta, que
// puede ser una abierta o una nueva con nombre. Los productos vienen al final.
Item {
    id: paso

    property var mesas: []
    property var cuentas: []
    property var mesa: null          // la mesa elegida; null mientras se elige
    signal elegida(var mesa, var cuenta)

    readonly property bool compacta: width < 700

    function reiniciar() {
        mesa = null
        cuentas = []
        cargarMesas()
    }

    function cargarMesas() {
        Api.listarMesas(function (r) {
            if (!paso)
                return
            if (r.codigo !== "OK") {
                Avisos.error(r.mensaje)
                return
            }
            paso.mesas = r.datos
        })
    }

    function elegirMesa(m) {
        mesa = m
        cuentas = []
        campoNombre.text = ""
        cargarCuentas()
    }

    function cargarCuentas() {
        if (!mesa)
            return
        Api.listarCuentas(mesa.numero_mesa, function (r) {
            if (!paso)
                return
            if (r.codigo !== "OK") {
                Avisos.error(r.mensaje)
                return
            }
            paso.cuentas = r.datos
        })
    }

    function abrirCuenta() {
        Api.crearCuenta(mesa.numero_mesa, campoNombre.text, function (r) {
            if (!paso)
                return
            if (r.codigo !== "OK") {
                Avisos.error(r.mensaje)
                return
            }
            campoNombre.text = ""
            Avisos.exito(r.mensaje)
            paso.elegida(paso.mesa, r.datos)
        })
    }

    Component.onCompleted: cargarMesas()

    Connections {
        target: Api
        function onCuentasCambiaron() { paso.cargarCuentas() }
        function onComandasCambiaron() { paso.cargarCuentas() }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: paso.compacta ? Tema.espacio : Tema.margen
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Button {
                visible: paso.mesa !== null
                text: "← Mesas"
                flat: true
                onClicked: paso.mesa = null
            }
            Label {
                Layout.fillWidth: true
                text: paso.mesa ? "Mesa " + paso.mesa.numero_mesa + ": elige la cuenta" : "Elige la mesa"
                color: Tema.texto
                font.pixelSize: Tema.letraTitulo
                font.bold: true
                wrapMode: Text.WordWrap
            }
        }

        Label {
            Layout.fillWidth: true
            text: paso.mesa ? "Toca una cuenta abierta o abre una nueva con un nombre que te ayude a reconocerla."
                            : "Primero la mesa, después la cuenta y al final los productos. Tus mesas van primero."
            color: Tema.textoSuave
            font.pixelSize: Tema.letraChica
            wrapMode: Text.WordWrap
        }

        // ============================ Mesas ============================
        GridView {
            id: rejillaMesas
            visible: paso.mesa === null
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            readonly property int columnas: Math.max(2, Math.floor(width / 180))
            cellWidth: Math.floor(width / columnas)
            cellHeight: 120
            model: paso.mesas
            ScrollBar.vertical: ScrollBar {}

            delegate: Item {
                id: celdaMesa
                required property var modelData
                width: rejillaMesas.cellWidth
                height: rejillaMesas.cellHeight

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 6
                    radius: Tema.radio
                    color: ratonMesa.containsMouse ? Tema.superficieAlta : Tema.superficie
                    border.width: celdaMesa.modelData.mia ? 2 : 1
                    border.color: celdaMesa.modelData.mia ? Tema.acento : Tema.borde

                    MouseArea {
                        id: ratonMesa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: paso.elegirMesa(celdaMesa.modelData)
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 4
                        Label {
                            text: "Mesa " + celdaMesa.modelData.numero_mesa
                            color: Tema.texto
                            font.pixelSize: 22
                            font.bold: true
                        }
                        Item { Layout.fillHeight: true }
                        Etiqueta {
                            visible: celdaMesa.modelData.mia
                            texto: "Tuya"
                            tono: Tema.acento
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: !celdaMesa.modelData.mia
                            text: "La atiende " + celdaMesa.modelData.mesero
                            color: Tema.textoSuave
                            font.pixelSize: Tema.letraChica
                            elide: Text.ElideRight
                        }
                    }
                }
            }
        }

        // ============================ Cuentas ============================
        Rectangle {
            visible: paso.mesa !== null
            Layout.fillWidth: true
            implicitHeight: filaNueva.implicitHeight + 24
            radius: Tema.radio
            color: Tema.superficie
            border.color: Tema.borde

            GridLayout {
                id: filaNueva
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.margins: 12
                columns: paso.compacta ? 1 : 3
                columnSpacing: 8
                Label {
                    text: "Cuenta nueva"
                    color: Tema.texto
                    font.bold: true
                }
                TextField {
                    id: campoNombre
                    Layout.fillWidth: true
                    placeholderText: "Nombre, p. ej. Novios o Mesa del fondo"
                    maximumLength: 60
                    onAccepted: paso.abrirCuenta()
                }
                Button {
                    Layout.fillWidth: paso.compacta
                    text: "Abrir cuenta"
                    highlighted: true
                    enabled: campoNombre.text.trim().length > 0
                    onClicked: paso.abrirCuenta()
                }
            }
        }

        GridView {
            id: rejillaCuentas
            visible: paso.mesa !== null
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            readonly property int columnas: Math.max(1, Math.floor(width / 260))
            cellWidth: Math.floor(width / columnas)
            cellHeight: 116
            model: paso.cuentas
            ScrollBar.vertical: ScrollBar {}

            delegate: Item {
                id: celdaCuenta
                required property var modelData
                width: rejillaCuentas.cellWidth
                height: rejillaCuentas.cellHeight

                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 6
                    radius: Tema.radio
                    color: ratonCuenta.containsMouse ? Tema.superficieAlta : Tema.superficie
                    border.color: Tema.borde

                    MouseArea {
                        id: ratonCuenta
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: paso.elegida(paso.mesa, celdaCuenta.modelData)
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 2
                        Label {
                            Layout.fillWidth: true
                            text: celdaCuenta.modelData.nombre
                            color: Tema.texto
                            font.pixelSize: 18
                            font.bold: true
                            elide: Text.ElideRight
                        }
                        Label {
                            text: celdaCuenta.modelData.folio
                            color: Tema.textoSuave
                            font.pixelSize: Tema.letraChica
                        }
                        Item { Layout.fillHeight: true }
                        Label {
                            text: "Lleva " + Tema.dinero(celdaCuenta.modelData.total)
                            color: Tema.acento
                            font.bold: true
                        }
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                width: parent.width - 32
                visible: paso.cuentas.length === 0
                text: "Esta mesa no tiene cuentas abiertas. Abre una nueva arriba."
                color: Tema.textoSuave
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
