pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// "=== SISTEMA DE COCINA ===" convertido en tablero de tres columnas.
// Cada estado se avanza tocando su chip o los botones Iniciar y Lista.
Item {
    id: pantalla

    signal salir()

    property var rondas: []
    property string estacion: Api.usuarioActual && Api.usuarioActual.estacion_asignada
                              ? Api.usuarioActual.estacion_asignada : "todas"
    property date ahora: new Date()

    readonly property var columnas: [
        { estado: "pendiente", titulo: "Pendientes" },
        { estado: "en_preparacion", titulo: "En preparación" },
        { estado: "lista", titulo: "Listas" }
    ]

    // Columna de una ronda según el estado de sus comandas
    function columnaDe(ronda) {
        const estados = ronda.lineas.map(l => l.estado)
        if (estados.every(e => e === "lista"))
            return "lista"
        if (estados.some(e => e === "en_preparacion"))
            return "en_preparacion"
        return "pendiente"
    }

    function rondasEn(estado) {
        return rondas.filter(r => columnaDe(r) === estado)
    }

    function cargar() {
        Api.listarRondas({ estacion: estacion, estados: ["pendiente", "en_preparacion", "lista"] }, function (r) {
            if (!pantalla)
                return
            if (r.codigo !== "OK") {
                Avisos.error(r.mensaje)
                return
            }
            pantalla.rondas = r.datos
        })
    }

    function cambiar(ids, estado) {
        if (ids.length === 0)
            return
        Api.cambiarEstado(ids, estado, function (r) {
            if (r.codigo !== "OK")
                Avisos.error(r.mensaje)
        })
    }

    Component.onCompleted: cargar()
    onEstacionChanged: cargar()

    Connections {
        target: Api
        function onComandasCambiaron() { pantalla.cargar() }
    }

    Timer {
        interval: 15000
        running: true
        repeat: true
        onTriggered: pantalla.ahora = new Date()
    }

    // Imita a los meseros mandando órdenes desde otras computadoras
    Timer {
        interval: 9000
        repeat: true
        running: interruptor.checked
        onTriggered: {
            const datos = Api.simularRondaEntrante()
            if (datos)
                Avisos.info("Nueva orden · Mesa " + datos.numero_mesa + " · Cuenta " + datos.folio)
        }
    }

    Rectangle {
        anchors.fill: parent
        color: Tema.fondo
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Encabezado {
            Layout.fillWidth: true
            titulo: pantalla.estacion === "bar" ? "Bar" : (pantalla.estacion === "cocina" ? "Cocina" : "Cocina y bar")
            onSalir: pantalla.salir()

            ComboBox {
                id: comboEstacion
                implicitWidth: 190
                model: [
                    { texto: "Cocina", valor: "cocina" },
                    { texto: "Bar", valor: "bar" },
                    { texto: "Todas las estaciones", valor: "todas" }
                ]
                textRole: "texto"
                valueRole: "valor"
                Component.onCompleted: currentIndex = indexOfValue(pantalla.estacion)
                onActivated: pantalla.estacion = currentValue
            }
            Switch {
                id: interruptor
                text: "Simular pedidos"
            }
            Button {
                text: "Actualizar"
                flat: true
                onClicked: pantalla.cargar()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: Tema.margen
            spacing: Tema.espacio

            Repeater {
                model: pantalla.columnas
                delegate: Rectangle {
                    id: contador
                    required property var modelData
                    readonly property int cuantas: pantalla.rondasEn(modelData.estado).length
                    implicitWidth: filaContador.implicitWidth + 32
                    implicitHeight: 56
                    radius: Tema.radio
                    color: Tema.superficie
                    border.color: Tema.borde

                    RowLayout {
                        id: filaContador
                        anchors.centerIn: parent
                        spacing: 10
                        Label {
                            text: contador.cuantas
                            color: Tema.colorEstado(contador.modelData.estado)
                            font.pixelSize: 28
                            font.bold: true
                        }
                        Label {
                            text: contador.modelData.titulo
                            color: Tema.textoSuave
                            font.pixelSize: Tema.letraMediana
                        }
                    }
                }
            }

            Item { Layout.fillWidth: true }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: Tema.margen
            Layout.rightMargin: Tema.margen
            Layout.bottomMargin: Tema.margen
            spacing: Tema.margen

            Repeater {
                model: pantalla.columnas
                delegate: Rectangle {
                    id: columna
                    required property var modelData
                    readonly property var tarjetas: pantalla.rondasEn(modelData.estado)
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredWidth: 1   // las tres columnas del mismo ancho
                    radius: Tema.radio
                    color: Tema.superficieAlta
                    border.color: Tema.borde

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 10

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: 4
                            Layout.rightMargin: 4
                            Rectangle {
                                implicitWidth: 10
                                implicitHeight: 10
                                radius: 5
                                color: Tema.colorEstado(columna.modelData.estado)
                            }
                            Label {
                                text: columna.modelData.titulo
                                color: Tema.texto
                                font.pixelSize: Tema.letraMediana
                                font.bold: true
                            }
                            Item { Layout.fillWidth: true }
                            Etiqueta {
                                texto: String(columna.tarjetas.length)
                                tono: Tema.colorEstado(columna.modelData.estado)
                            }
                        }

                        ListView {
                            id: listaColumna
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 10
                            model: columna.tarjetas
                            ScrollBar.vertical: ScrollBar {}

                            delegate: TarjetaRonda {
                                id: tarjetaRonda
                                required property var modelData
                                width: listaColumna.width
                                ronda: tarjetaRonda.modelData
                                modo: "cocina"
                                ahora: pantalla.ahora
                                onCambiar: (ids, estado) => pantalla.cambiar(ids, estado)
                            }

                            Label {
                                anchors.centerIn: parent
                                visible: columna.tarjetas.length === 0
                                text: "Sin órdenes"
                                color: Tema.textoSuave
                            }
                        }
                    }
                }
            }
        }
    }
}
