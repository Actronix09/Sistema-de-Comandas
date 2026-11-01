pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// "Ver Mis Ordenes": lo que levantó este mesero, en tarjetas (una por ronda),
// con el estado de cada comanda. A diferencia de la terminal, filtra en el
// servidor por el id del mesero (no por el nombre) y se actualiza sola cuando
// cocina cambia algo.
Item {
    id: vista

    property bool activa: false
    property bool soloActivas: true
    property var rondas: []
    property date ahora: new Date()
    readonly property bool compacta: width < 700

    function cargar() {
        if (!Api.usuarioActual)
            return
        const filtro = { estacion: "todas", id_mesero: Api.usuarioActual.id_usuario,
                         estados: soloActivas ? ["pendiente", "en_preparacion", "lista"] : null }
        Api.listarRondas(filtro, function (r) {
            if (!vista)
                return
            if (r.codigo !== "OK") {
                Avisos.error(r.mensaje)
                return
            }
            vista.rondas = r.datos.slice().reverse()   // la más reciente arriba
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
                text: "Mis órdenes"
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
            contentHeight: flujo.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            Flow {
                id: flujo
                width: desplazable.width
                spacing: 12
                readonly property int columnas: Math.max(1, Math.floor((width + spacing) / (320 + spacing)))
                readonly property real anchoTarjeta: (width - (columnas - 1) * spacing) / columnas

                Repeater {
                    model: vista.rondas
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

            Label {
                anchors.centerIn: parent
                visible: vista.rondas.length === 0
                text: vista.soloActivas ? "No tienes órdenes activas" : "Todavía no has levantado órdenes"
                color: Tema.textoSuave
            }
        }
    }
}
