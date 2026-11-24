pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Logs: lo que ha pasado en el servidor (inicios de sesión, quién creó,
// cobró o canceló qué y las operaciones rechazadas), de lo más reciente a lo
// más viejo. El servidor guarda las últimas 500 líneas en memoria; al
// reiniciarse empieza de nuevo.
Item {
    id: pagina

    signal volver()

    property var lineas: []
    property var visibles: []
    property bool cargando: false
    readonly property bool compacta: width < 760

    function cargar() {
        cargando = true
        Api.listarBitacora(function (r) {
            if (!pagina)
                return
            pagina.cargando = false
            if (r.codigo !== "OK") {
                Avisos.error(r.mensaje)
                return
            }
            pagina.lineas = r.datos
            pagina.filtrar()
        })
    }

    function filtrar() {
        const texto = busqueda.text.trim().toLowerCase()
        const soloRechazos = filtroRechazos.checked
        visibles = lineas.filter(l => (texto === "" || l.texto.toLowerCase().indexOf(texto) >= 0)
                                      && (!soloRechazos || pagina.esRechazo(l)))
    }

    function esRechazo(linea) {
        return /rechazad|no se pudo|error/i.test(linea.texto)
    }

    Component.onCompleted: cargar()

    // Cualquier cambio en el salón deja una línea nueva
    Connections {
        target: Api
        function onComandasCambiaron() { pagina.cargar() }
        function onCuentasCambiaron() { pagina.cargar() }
        function onInventarioCambio() { pagina.cargar() }
        function onUsuariosCambiaron() { pagina.cargar() }
        function onCatalogoCambio() { pagina.cargar() }
        function onEventosCambiaron() { pagina.cargar() }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tema.margen
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Button {
                text: "← Volver"
                flat: true
                onClicked: pagina.volver()
            }
            Label {
                text: "Logs"
                color: Tema.texto
                font.pixelSize: Tema.letraTitulo
                font.bold: true
            }
            Label {
                visible: !pagina.compacta
                text: pagina.lineas.length + " registros"
                color: Tema.textoSuave
            }
            Item { Layout.fillWidth: true }
            Button {
                text: pagina.cargando ? "Cargando…" : "Actualizar"
                flat: true
                enabled: !pagina.cargando
                onClicked: pagina.cargar()
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            TextField {
                id: busqueda
                Layout.fillWidth: true
                Layout.maximumWidth: 360
                placeholderText: "Buscar por usuario, mesa, operación…"
                onTextChanged: pagina.filtrar()
            }
            CheckBox {
                id: filtroRechazos
                text: "Solo rechazos y errores"
                onToggled: pagina.filtrar()
            }
            Item { Layout.fillWidth: true }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Tema.radio
            color: Tema.superficie
            border.color: Tema.borde
            clip: true

            ListView {
                id: lista
                anchors.fill: parent
                anchors.margins: 1
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                model: pagina.visibles
                ScrollBar.vertical: ScrollBar {}

                delegate: Rectangle {
                    id: fila
                    required property var modelData
                    required property int index
                    width: ListView.view.width
                    implicitHeight: contenido.implicitHeight + 16
                    color: fila.index % 2 === 0 ? Tema.superficie : Tema.superficieAlta

                    GridLayout {
                        id: contenido
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        columns: pagina.compacta ? 1 : 2
                        columnSpacing: 16
                        rowSpacing: 2

                        Label {
                            Layout.preferredWidth: pagina.compacta ? -1 : 150
                            text: fila.modelData.fecha
                            color: Tema.textoSuave
                            font.family: "monospace"
                            font.pixelSize: Tema.letraChica
                        }
                        Label {
                            Layout.fillWidth: true
                            text: fila.modelData.texto
                            color: pagina.esRechazo(fila.modelData) ? Tema.peligro : Tema.texto
                            wrapMode: Text.Wrap
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: lista.count === 0 && !pagina.cargando
                    text: pagina.lineas.length === 0 ? "Todavía no hay actividad" : "Nada coincide con la búsqueda"
                    color: Tema.textoSuave
                }
            }
        }
    }
}
