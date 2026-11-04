pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// "Ver Mis Ordenes": lo que levantó este mesero, en tarjetas (una por ronda),
// con el estado de cada comanda. A diferencia de la terminal, filtra en el
// servidor por el id del mesero (no por el nombre) y se actualiza sola cuando
// cocina cambia algo. Se organizan por tiempo, por mesa y cuenta, o por
// mesero; esta última muestra también las órdenes de los demás meseros, para
// ayudar a entregar lo que ya está listo.
Item {
    id: vista

    property bool activa: false
    property bool soloActivas: true
    property var rondas: []
    property date ahora: new Date()
    readonly property bool compacta: width < 700
    readonly property var grupos: selector.agrupar(rondas)

    function cargar() {
        if (!Api.usuarioActual)
            return
        const filtro = { estacion: "todas",
                         id_mesero: selector.modo === "mesero" ? null : Api.usuarioActual.id_usuario,
                         estados: soloActivas ? ["pendiente", "en_preparacion", "lista"] : null }
        Api.listarRondas(filtro, function (r) {
            if (!vista)
                return
            if (r.codigo !== "OK") {
                Avisos.error(r.mensaje)
                return
            }
            vista.rondas = vista.ordenar(r.datos)
        })
    }

    // Lo que ya se puede entregar va primero (de lo que lleva más tiempo listo
    // a lo más reciente); después, lo demás con lo más reciente arriba.
    function ordenar(rondas) {
        const tieneLista = r => r.lineas.some(l => l.estado === "lista")
        return rondas.slice().sort(function (a, b) {
            const listaA = tieneLista(a), listaB = tieneLista(b)
            if (listaA !== listaB)
                return listaA ? -1 : 1
            return listaA ? a.fecha_hora - b.fecha_hora : b.fecha_hora - a.fecha_hora
        })
    }

    function cambiar(ids, estado) {
        Api.cambiarEstado(ids, estado, function (r) {
            if (r.codigo === "OK")
                Avisos.exito(r.mensaje)
            else
                Avisos.error(r.mensaje)
        })
    }

    onActivaChanged: if (activa) cargar()
    onSoloActivasChanged: cargar()
    Component.onCompleted: cargar()

    Connections {
        target: Api
        function onComandasCambiaron() { vista.cargar() }
    }

    Timer {
        interval: 15000
        running: vista.activa
        repeat: true
        onTriggered: vista.ahora = new Date()
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tema.margen
        spacing: 10

        GridLayout {
            Layout.fillWidth: true
            columns: vista.compacta ? 1 : 2
            columnSpacing: 6
            rowSpacing: 0
            Label {
                Layout.fillWidth: true
                text: selector.modo === "mesero" ? "Órdenes por mesero" : "Mis órdenes"
                color: Tema.texto
                font.pixelSize: Tema.letraTitulo
                font.bold: true
            }
            RowLayout {
                spacing: 6
                Button {
                    text: "Activas"
                    highlighted: vista.soloActivas
                    flat: !highlighted
                    onClicked: vista.soloActivas = true
                }
                Button {
                    text: "Todas"
                    highlighted: !vista.soloActivas
                    flat: !highlighted
                    onClicked: vista.soloActivas = false
                }
                Button {
                    text: "Actualizar"
                    flat: true
                    onClicked: vista.cargar()
                }
            }
        }

        SelectorOrden {
            id: selector
            onModoChanged: vista.cargar()
        }

        Label {
            Layout.fillWidth: true
            text: "Se actualiza sola cuando cocina o bar cambian algo. Lo que está listo se entrega desde aquí."
            color: Tema.textoSuave
            font.pixelSize: Tema.letraChica
            wrapMode: Text.WordWrap
        }

        // Tarjetas en columnas de unos 320 px; en teléfono queda una sola columna
        Flickable {
            id: desplazable
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: columnaGrupos.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            Column {
                id: columnaGrupos
                width: desplazable.width
                spacing: 18

                Repeater {
                    model: vista.grupos
                    delegate: Column {
                        id: grupo
                        required property var modelData
                        width: columnaGrupos.width
                        spacing: 8

                        Label {
                            visible: grupo.modelData.titulo !== ""
                            width: parent.width
                            text: grupo.modelData.titulo + " (" + grupo.modelData.rondas.length + ")"
                            color: Tema.texto
                            font.pixelSize: Tema.letraMediana
                            font.bold: true
                            elide: Text.ElideRight
                        }

                        Flow {
                            id: flujo
                            width: parent.width
                            spacing: 12
                            readonly property int columnas: Math.max(1, Math.floor((width + spacing) / (320 + spacing)))
                            readonly property real anchoTarjeta: (width - (columnas - 1) * spacing) / columnas

                            Repeater {
                                model: grupo.modelData.rondas
                                delegate: TarjetaRonda {
                                    id: tarjeta
                                    required property var modelData
                                    width: flujo.anchoTarjeta
                                    ronda: tarjeta.modelData
                                    modo: "mesero"
                                    ahora: vista.ahora
                                    onCambiar: (ids, estado) => vista.cambiar(ids, estado)
                                }
                            }
                        }
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: vista.rondas.length === 0
                text: vista.soloActivas ? "No tienes órdenes activas" : "Todavía no has levantado órdenes"
                color: Tema.textoSuave
            }
        }
    }
}
