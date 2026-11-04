pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// "Ver Mis Ordenes": lo que levantó este mesero, en rondas, con el estado de
// cada comanda. A diferencia de la terminal, filtra en el servidor por el id
// del mesero (no por el nombre) y se actualiza sola cuando cocina cambia algo.
Item {
    id: vista

    property bool activa: false
    property bool soloActivas: true
    property var rondas: []
    property date ahora: new Date()

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

        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Label {
                text: "Mis órdenes"
                color: Tema.texto
                font.pixelSize: Tema.letraTitulo
                font.bold: true
            }
            Item { Layout.fillWidth: true }
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

        Label {
            text: "Se actualiza sola cuando cocina o bar cambian algo. Lo que está listo se entrega desde aquí."
            color: Tema.textoSuave
            font.pixelSize: Tema.letraChica
        }

        ListView {
            id: lista
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 12
            model: vista.rondas
            ScrollBar.vertical: ScrollBar {}

            delegate: Item {
                id: celda
                required property var modelData
                width: lista.width
                height: tarjeta.implicitHeight

                TarjetaRonda {
                    id: tarjeta
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: Math.min(parent.width, 820)
                    ronda: celda.modelData
                    modo: "mesero"
                    ahora: vista.ahora
                    onCambiar: (ids, estado) => vista.cambiar(ids, estado)
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
