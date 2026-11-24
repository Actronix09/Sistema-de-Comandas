pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Admin › Eventos: un calendario con los eventos pasados y futuros y, al lado
// (abajo en pantallas angostas), la lista del día elegido, de los próximos o
// de los pasados. Tocar un evento abre su ficha (EditorEvento).
Item {
    id: pagina

    signal volver()
    signal abrirEvento(var evento, string fecha)

    property var eventos: []
    property var porFecha: ({})                 // "AAAA-MM-DD" → [eventos]
    property bool cargando: false
    property date hoy: new Date()
    property string diaElegido: Tema.iso(hoy)
    property string vista: "dia"                // dia | proximos | pasados
    property int mes: hoy.getMonth()
    property int anio: hoy.getFullYear()
    readonly property bool compacta: width < 820

    // El evento con el que trabaja el salón ahora mismo (puede no haber)
    readonly property var actual: eventos.find(e => e.estado === "en_curso") || null

    readonly property var lista: {
        const hoyIso = Tema.iso(hoy)
        if (vista === "proximos")
            return eventos.filter(e => e.fecha >= hoyIso && e.estado !== "cancelado")
        if (vista === "pasados")
            return eventos.filter(e => e.fecha < hoyIso).slice().reverse()
        return porFecha[diaElegido] || []
    }

    function cargar() {
        cargando = true
        Api.listarEventos(null, null, function (r) {
            if (!pagina)
                return
            pagina.cargando = false
            if (r.codigo !== "OK") {
                Avisos.error(r.mensaje)
                return
            }
            const mapa = {}
            for (const e of r.datos)
                (mapa[e.fecha] = mapa[e.fecha] || []).push(e)
            pagina.eventos = r.datos
            pagina.porFecha = mapa
        })
    }

    function moverMes(delta) {
        const fecha = new Date(anio, mes + delta, 1)
        mes = fecha.getMonth()
        anio = fecha.getFullYear()
    }

    function irAHoy() {
        hoy = new Date()
        mes = hoy.getMonth()
        anio = hoy.getFullYear()
        diaElegido = Tema.iso(hoy)
        vista = "dia"
    }

    Component.onCompleted: cargar()

    Connections {
        target: Api
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
                text: "Eventos"
                color: Tema.texto
                font.pixelSize: Tema.letraTitulo
                font.bold: true
            }
            Item { Layout.fillWidth: true }
            Button {
                text: pagina.compacta ? "Nuevo" : "Nuevo evento"
                highlighted: true
                onClicked: pagina.abrirEvento(null, pagina.diaElegido)
            }
        }

        // En el celular, el evento actual, el calendario y la lista se desplazan
        // juntos y la lista crece lo que necesite; en pantalla ancha cada parte
        // conserva su lugar y solo la lista se desplaza.
        Flickable {
            id: cuerpo
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: width
            contentHeight: pagina.compacta ? contenidoCuerpo.implicitHeight : height
            interactive: pagina.compacta
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { visible: pagina.compacta }

            ColumnLayout {
                id: contenidoCuerpo
                width: cuerpo.width
                height: pagina.compacta ? implicitHeight : cuerpo.height
                spacing: 10

                // ---------------- Evento actual (solo si hay uno en curso) ----------------
                Rectangle {
                    visible: pagina.actual !== null
                    Layout.fillWidth: true
                    implicitHeight: filaActual.implicitHeight + 24
                    radius: Tema.radio
                    color: Tema.superficie
                    border.color: Tema.exito
                    border.width: 2

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: pagina.abrirEvento(pagina.actual, pagina.actual.fecha)
                    }

                    RowLayout {
                        id: filaActual
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        anchors.leftMargin: 16
                        anchors.rightMargin: 12
                        spacing: 12

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Label {
                                text: "EVENTO ACTUAL"
                                color: Tema.exito
                                font.pixelSize: 11
                                font.bold: true
                                font.letterSpacing: 1.2
                            }
                            Label {
                                Layout.fillWidth: true
                                text: pagina.actual ? "#" + pagina.actual.numero_evento + " · " + pagina.actual.tipo_evento : ""
                                color: Tema.texto
                                font.pixelSize: Tema.letraMediana
                                font.bold: true
                                elide: Text.ElideRight
                            }
                            Label {
                                Layout.fillWidth: true
                                text: pagina.actual
                                      ? Tema.fechaLarga(pagina.actual.fecha) + " · " + pagina.actual.numero_asistentes + " asistentes · "
                                        + pagina.actual.mesas.length + " mesas · " + pagina.actual.cuentas_abiertas
                                        + (pagina.actual.cuentas_abiertas === 1 ? " cuenta abierta" : " cuentas abiertas")
                                      : ""
                                color: Tema.textoSuave
                                wrapMode: Text.WordWrap
                            }
                        }
                        Button {
                            text: "Abrir"
                            flat: true
                            onClicked: pagina.abrirEvento(pagina.actual, pagina.actual.fecha)
                        }
                    }
                }

                GridLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    columns: pagina.compacta ? 1 : 2
                    columnSpacing: Tema.margen
                    rowSpacing: Tema.margen

                    // ---------------- Calendario ----------------
                    Rectangle {
                        Layout.preferredWidth: pagina.compacta ? -1 : 400
                        Layout.fillWidth: pagina.compacta
                        Layout.alignment: Qt.AlignTop
                        implicitHeight: calendario.implicitHeight + 24
                        radius: Tema.radio
                        color: Tema.superficie
                        border.color: Tema.borde

                        ColumnLayout {
                            id: calendario
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 12
                            spacing: 6

                            RowLayout {
                                Layout.fillWidth: true
                                ToolButton {
                                    text: "‹"
                                    font.pixelSize: 20
                                    onClicked: pagina.moverMes(-1)
                                }
                                Label {
                                    Layout.fillWidth: true
                                    horizontalAlignment: Text.AlignHCenter
                                    text: {
                                        const nombre = new Date(pagina.anio, pagina.mes, 1).toLocaleDateString(Qt.locale("es_MX"), "MMMM yyyy")
                                        return nombre.charAt(0).toUpperCase() + nombre.slice(1)
                                    }
                                    color: Tema.texto
                                    font.pixelSize: Tema.letraMediana
                                    font.bold: true
                                }
                                ToolButton {
                                    text: "›"
                                    font.pixelSize: 20
                                    onClicked: pagina.moverMes(1)
                                }
                            }

                            DayOfWeekRow {
                                Layout.fillWidth: true
                                locale: Qt.locale("es_MX")
                                delegate: Label {
                                    required property string shortName
                                    text: shortName
                                    horizontalAlignment: Text.AlignHCenter
                                    color: Tema.textoSuave
                                    font.pixelSize: Tema.letraChica
                                    font.bold: true
                                }
                            }

                            // MonthGrid reparte su alto entre las 6 semanas; sin alto propio
                                // dentro del layout queda en 0 y los días se enciman
                            MonthGrid {
                                id: rejilla
                                Layout.fillWidth: true
                                Layout.preferredHeight: 6 * 46 + 5 * spacing
                                month: pagina.mes
                                year: pagina.anio
                                locale: Qt.locale("es_MX")
                                spacing: 2

                                delegate: Rectangle {
                                    id: celda
                                    required property int day
                                    required property int month
                                    required property int year
                                    required property bool today
                                    readonly property string fecha: Tema.iso(new Date(celda.year, celda.month, celda.day))
                                    readonly property var delDia: pagina.porFecha[celda.fecha] || []
                                    readonly property bool elegida: pagina.vista === "dia" && pagina.diaElegido === celda.fecha
                                    implicitWidth: 40
                                    implicitHeight: 46
                                    radius: 8
                                    color: elegida ? Tema.acento : (raton.containsMouse ? Tema.superficieAlta : "transparent")
                                    border.width: celda.today && !elegida ? 2 : 0
                                    border.color: Tema.acento
                                    opacity: celda.month === pagina.mes ? 1 : 0.35

                                    Label {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        anchors.top: parent.top
                                        anchors.topMargin: 6
                                        text: celda.day
                                        color: celda.elegida ? Tema.textoSobreAcento : Tema.texto
                                        font.bold: celda.today || celda.delDia.length > 0
                                    }
                                    // Un punto por evento (hasta 3), del color de su estado
                                    Row {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        anchors.bottom: parent.bottom
                                        anchors.bottomMargin: 6
                                        spacing: 3
                                        Repeater {
                                            model: celda.delDia.slice(0, 3)
                                            delegate: Rectangle {
                                                required property var modelData
                                                width: 6
                                                height: 6
                                                radius: 3
                                                color: celda.elegida ? Tema.textoSobreAcento : Tema.colorEstadoEvento(modelData.estado)
                                            }
                                        }
                                    }
                                    MouseArea {
                                        id: raton
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        cursorShape: Qt.PointingHandCursor
                                        onClicked: {
                                            pagina.diaElegido = celda.fecha
                                            pagina.vista = "dia"
                                            if (celda.month !== pagina.mes)
                                                pagina.moverMes(celda.month < pagina.mes || (pagina.mes === 0 && celda.month === 11) ? -1 : 1)
                                        }
                                        onDoubleClicked: if (celda.delDia.length === 0) pagina.abrirEvento(null, celda.fecha)
                                    }
                                }
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                Button {
                                    text: "Hoy"
                                    flat: true
                                    onClicked: pagina.irAHoy()
                                }
                                // En varias líneas si no cabe: en una sola empujaba el calendario
                                Flow {
                                    Layout.fillWidth: true
                                    spacing: 8
                                Repeater {
                                    model: ["programado", "en_curso", "finalizado", "cancelado"]
                                    delegate: RowLayout {
                                        required property string modelData
                                        spacing: 4
                                        Rectangle {
                                            width: 8
                                            height: 8
                                            radius: 4
                                            color: Tema.colorEstadoEvento(parent.modelData)
                                        }
                                        Label {
                                            text: Tema.nombreEstadoEvento(parent.modelData)
                                            color: Tema.textoSuave
                                            font.pixelSize: 11
                                        }
                                    }
                                }
                                }
                            }
                        }
                    }

                    // ---------------- Lista ----------------
                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: pagina.compacta ? 0 : 240
                        spacing: 8

                        TabBar {
                            id: pestanas
                            Layout.fillWidth: true
                            currentIndex: ["dia", "proximos", "pasados"].indexOf(pagina.vista)
                            TabButton {
                                text: pagina.compacta ? "Día" : Tema.fechaCorta(pagina.diaElegido)
                                onClicked: pagina.vista = "dia"
                            }
                            TabButton { text: "Próximos"; onClicked: pagina.vista = "proximos" }
                            TabButton { text: "Pasados"; onClicked: pagina.vista = "pasados" }
                        }

                        ListView {
                            id: listaEventos
                            Layout.fillWidth: true
                            Layout.fillHeight: !pagina.compacta
                            // En el celular la lista mide lo que sus eventos y se desplaza la página
                            Layout.preferredHeight: pagina.compacta ? Math.max(contentHeight, 120) : -1
                            interactive: !pagina.compacta
                            clip: true
                            spacing: 8
                            boundsBehavior: Flickable.StopAtBounds
                            model: pagina.lista
                            ScrollBar.vertical: ScrollBar {}

                            delegate: Rectangle {
                                id: tarjeta
                                required property var modelData
                                width: ListView.view.width
                                implicitHeight: datos.implicitHeight + 24
                                radius: Tema.radio
                                color: raton2.containsMouse ? Tema.superficieAlta : Tema.superficie
                                border.color: tarjeta.modelData.estado === "en_curso" ? Tema.exito : Tema.borde
                                border.width: tarjeta.modelData.estado === "en_curso" ? 2 : 1

                                MouseArea {
                                    id: raton2
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: pagina.abrirEvento(tarjeta.modelData, tarjeta.modelData.fecha)
                                }

                                ColumnLayout {
                                    id: datos
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.margins: 12
                                    spacing: 4

                                    RowLayout {
                                        Layout.fillWidth: true
                                        Label {
                                            Layout.fillWidth: true
                                            text: "#" + tarjeta.modelData.numero_evento + " · " + tarjeta.modelData.tipo_evento
                                            color: Tema.texto
                                            font.pixelSize: Tema.letraMediana
                                            font.bold: true
                                            elide: Text.ElideRight
                                        }
                                        Etiqueta {
                                            texto: Tema.nombreEstadoEvento(tarjeta.modelData.estado)
                                            tono: Tema.colorEstadoEvento(tarjeta.modelData.estado)
                                        }
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: Tema.fechaLarga(tarjeta.modelData.fecha) + " · " + tarjeta.modelData.numero_asistentes + " asistentes"
                                              + " · " + tarjeta.modelData.mesas.length + (tarjeta.modelData.mesas.length === 1 ? " mesa" : " mesas")
                                        color: Tema.textoSuave
                                        wrapMode: Text.WordWrap
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        visible: tarjeta.modelData.organizadores.length > 0
                                        text: "Organizan " + tarjeta.modelData.organizadores.join(", ")
                                        color: Tema.textoSuave
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        visible: tarjeta.modelData.paquetes.length > 0 || tarjeta.modelData.total_anticipos > 0
                                        text: tarjeta.modelData.paquetes.map(p => p.titulo).join(", ")
                                              + (tarjeta.modelData.total_anticipos > 0
                                                 ? (tarjeta.modelData.paquetes.length > 0 ? " · " : "") + "Anticipos " + Tema.dinero(tarjeta.modelData.total_anticipos)
                                                 : "")
                                        color: Tema.texto
                                        font.pixelSize: Tema.letraChica
                                        elide: Text.ElideRight
                                    }
                                }
                            }

                            Label {
                                anchors.centerIn: parent
                                width: parent.width - 32
                                horizontalAlignment: Text.AlignHCenter
                                visible: listaEventos.count === 0 && !pagina.cargando
                                text: pagina.vista === "dia" ? "No hay eventos este día.\nToca dos veces el día o «Nuevo evento» para agregar uno."
                                      : pagina.vista === "proximos" ? "No hay eventos próximos" : "No hay eventos pasados"
                                color: Tema.textoSuave
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }
        }
    }
}
