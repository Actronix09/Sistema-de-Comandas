pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Ficha de un evento, para crearlo o editarlo:
//   datos y organizadores, repetición (solo al crear), mesas con su mesero,
//   paquetes con la cantidad acordada de cada producto, anticipos y estado.
// Guardar manda todo junto; los anticipos y los cambios de estado se aplican
// en el momento, porque son dinero recibido o el arranque del salón.
Item {
    id: pagina

    signal volver()

    property var evento: null               // null = nuevo
    property string fechaInicial: ""
    property bool guardando: false
    property string error: ""

    property var organizadores: []
    property var paquetesEvento: []         // [{ numero_paquete, titulo, monto, productos: [{ id_producto, nombre, cantidad, modo_cobro }] }]
    property int secuencialAnfitrion: 0      // 0 = sin cuenta del anfitrión

    // Qué pasa cuando se pide un producto del paquete (migración 003 de la base)
    readonly property var modosCobro: [
        { texto: "Se cobra aparte (no incluido)", valor: "aparte" },
        { texto: "Incluido; al acabarse se cobra", valor: "cobrar" },
        { texto: "Incluido; al acabarse no se vende", valor: "bloquear" },
        { texto: "Incluido; al acabarse paga el anfitrión", valor: "anfitrion" }
    ]
    readonly property bool usaAnfitrion: paquetesEvento.some(p => p.productos.some(x => x.modo_cobro === "anfitrion"))
    property var meseros: []
    property var catalogoPaquetes: []
    property var cuentasEvento: []          // las del evento, con su dinero (v_cuenta)

    readonly property bool esNuevo: evento === null
    readonly property bool editable: esNuevo || (evento.estado !== "finalizado" && evento.estado !== "cancelado")
    readonly property bool compacta: width < 760
    readonly property real totalPaquetes: paquetesEvento.reduce((s, p) => s + p.monto, 0)

    ListModel { id: modeloMesas }   // { numero_mesa, id_mesero, guardada }

    function cargarCuentas() {
        if (!evento)
            return
        Api.listarCuentasCaja(function (r) {
            if (pagina && r.codigo === "OK")
                pagina.cuentasEvento = r.datos
        }, evento.numero_evento)
    }

    function cargarDatos() {
        cargarCuentas()
        if (evento) {
            campoFecha.text = evento.fecha
            campoTipo.editText = evento.tipo_evento
            campoAsistentes.value = evento.numero_asistentes
            organizadores = evento.organizadores.slice()
            paquetesEvento = evento.paquetes.map(p => ({
                numero_paquete: p.numero_paquete, titulo: p.titulo, monto: p.monto,
                productos: p.productos.map(x => ({ id_producto: x.id_producto, nombre: x.nombre, cantidad: x.cantidad,
                                                   modo_cobro: x.modo_cobro }))
            }))
            secuencialAnfitrion = evento.secuencial_anfitrion || 0
            modeloMesas.clear()
            for (const m of evento.mesas)
                modeloMesas.append({ numero_mesa: m.numero_mesa, id_mesero: m.id_mesero, guardada: true })
        } else {
            campoFecha.text = fechaInicial
            campoAsistentes.value = 100
        }
        Api.listarUsuarios(function (r) {
            if (pagina && r.codigo === "OK")
                pagina.meseros = r.datos.filter(u => u.rol === "mesero")
        })
        Api.listarPaquetes(function (r) {
            if (pagina && r.codigo === "OK")
                pagina.catalogoPaquetes = r.datos
        })
    }

    // Tras registrar un anticipo o cambiar el estado, se traen solo esos datos
    // para no perder lo que se esté editando
    function refrescar() {
        if (!evento)
            return
        Api.listarEventos(evento.fecha, evento.fecha, function (r) {
            if (!pagina || r.codigo !== "OK")
                return
            const nuevo = r.datos.find(e => e.numero_evento === pagina.evento.numero_evento)
            if (nuevo) {
                const copia = Object.assign({}, pagina.evento)
                copia.estado = nuevo.estado
                copia.anticipos = nuevo.anticipos
                copia.total_anticipos = nuevo.total_anticipos
                copia.cuentas_abiertas = nuevo.cuentas_abiertas
                pagina.evento = copia
            }
        })
    }

    function agregarOrganizador() {
        const nombre = campoOrganizador.text.trim()
        if (nombre === "" || organizadores.indexOf(nombre) >= 0)
            return
        organizadores = organizadores.concat([nombre])
        campoOrganizador.text = ""
    }

    function agregarMesa() {
        let siguiente = 1
        for (let i = 0; i < modeloMesas.count; i++)
            siguiente = Math.max(siguiente, modeloMesas.get(i).numero_mesa + 1)
        modeloMesas.append({ numero_mesa: siguiente, id_mesero: meseros.length > 0 ? meseros[0].id_usuario : 0, guardada: false })
    }

    function agregarPaquete(paquete) {
        if (!paquete || paquetesEvento.some(p => p.numero_paquete === paquete.numero_paquete))
            return
        paquetesEvento = paquetesEvento.concat([{
            numero_paquete: paquete.numero_paquete, titulo: paquete.titulo, monto: paquete.monto,
            // Por omisión, una porción por asistente
            productos: paquete.productos.map(x => ({ id_producto: x.id_producto, nombre: x.nombre, cantidad: campoAsistentes.value,
                                                     modo_cobro: "cobrar" }))
        }])
    }

    function quitarPaquete(numero) {
        paquetesEvento = paquetesEvento.filter(p => p.numero_paquete !== numero)
    }

    function fijarModo(numeroPaquete, idProducto, modo) {
        const p = paquetesEvento.find(x => x.numero_paquete === numeroPaquete)
        const x = p ? p.productos.find(y => y.id_producto === idProducto) : null
        if (x)
            x.modo_cobro = modo
        paquetesEvento = paquetesEvento.slice()   // para que usaAnfitrion se recalcule
    }

    function fijarCantidad(numeroPaquete, idProducto, cantidad) {
        // Se cambia en sitio: reasignar la lista recrearía los campos mientras se escribe
        const p = paquetesEvento.find(x => x.numero_paquete === numeroPaquete)
        const x = p ? p.productos.find(y => y.id_producto === idProducto) : null
        if (x)
            x.cantidad = cantidad
    }

    // Las fechas que tendría cada copia, para mostrarlas antes de guardar
    readonly property var fechasRepetidas: {
        if (!esNuevo || !checkRepetir.checked || !/^\d{4}-\d{2}-\d{2}$/.test(campoFecha.text))
            return []
        const inicio = Tema.fechaDe(campoFecha.text)
        const fechas = []
        for (let k = 0; k < campoVeces.value; k++) {
            const f = new Date(inicio)
            const n = k * campoCada.value
            if (comboUnidad.currentValue === "dias")
                f.setDate(f.getDate() + n)
            else if (comboUnidad.currentValue === "semanas")
                f.setDate(f.getDate() + 7 * n)
            else
                f.setMonth(f.getMonth() + n)
            fechas.push(Tema.iso(f))
        }
        return fechas
    }

    function guardar() {
        const mesas = []
        for (let i = 0; i < modeloMesas.count; i++) {
            const m = modeloMesas.get(i)
            mesas.push({ numero_mesa: m.numero_mesa, id_mesero: m.id_mesero })
        }
        const datos = {
            fecha: campoFecha.text, tipo_evento: campoTipo.editText, numero_asistentes: campoAsistentes.value,
            organizadores: organizadores, mesas: mesas, paquetes: paquetesEvento
        }
        if (evento) {
            datos.numero_evento = evento.numero_evento
            datos.secuencial_anfitrion = secuencialAnfitrion || null
        } else if (checkRepetir.checked)
            datos.repetir = { cada: comboUnidad.currentValue, intervalo: campoCada.value, veces: campoVeces.value }
        guardando = true
        error = ""
        Api.guardarEvento(datos, function (r) {
            if (!pagina)
                return
            pagina.guardando = false
            if (r.codigo === "OK") {
                Avisos.exito(r.mensaje)
                pagina.volver()
            } else {
                pagina.error = r.mensaje
            }
        })
    }

    function cambiarEstado(estado) {
        Api.cambiarEstadoEvento(evento.numero_evento, estado, function (r) {
            if (!pagina)
                return
            if (r.codigo === "OK") {
                Avisos.exito(r.mensaje)
                pagina.refrescar()
            } else {
                Avisos.error(r.mensaje)
            }
        })
    }

    function registrarAnticipo() {
        Api.registrarAnticipo(evento.numero_evento, campoAnticipo.text, campoFechaAnticipo.text, function (r) {
            if (!pagina)
                return
            if (r.codigo === "OK") {
                Avisos.exito(r.mensaje)
                campoAnticipo.text = ""
                campoFechaAnticipo.text = ""
                pagina.refrescar()
            } else {
                Avisos.error(r.mensaje)
            }
        })
    }

    Component.onCompleted: cargarDatos()

    Connections {
        target: Api
        function onCuentasCambiaron() { pagina.cargarCuentas() }
    }

    Rectangle {
        anchors.fill: parent
        color: Tema.fondo
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Tema.margen
        spacing: 10

        // ---------------- Encabezado y estado ----------------
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            Button {
                text: "← Volver"
                flat: true
                onClicked: pagina.volver()
            }
            Label {
                Layout.fillWidth: true
                text: pagina.esNuevo ? "Nuevo evento" : "Evento #" + pagina.evento.numero_evento
                color: Tema.texto
                font.pixelSize: Tema.letraTitulo
                font.bold: true
                elide: Text.ElideRight
            }
            Etiqueta {
                visible: !pagina.esNuevo
                texto: pagina.evento ? Tema.nombreEstadoEvento(pagina.evento.estado) : ""
                tono: pagina.evento ? Tema.colorEstadoEvento(pagina.evento.estado) : Tema.textoSuave
                solida: true
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: !pagina.esNuevo && pagina.editable
            spacing: 8
            Button {
                visible: pagina.evento && pagina.evento.estado === "programado"
                text: "▶ Iniciar evento"
                highlighted: true
                onClicked: confirmarEstado.pedir("en_curso", "¿Iniciar el evento?",
                    "Pasa a ser el evento en curso: los meseros toman órdenes en sus mesas y cocina las ve.")
            }
            Button {
                visible: pagina.evento && pagina.evento.estado === "en_curso"
                text: "■ Finalizar evento"
                onClicked: confirmarEstado.pedir("finalizado", "¿Finalizar el evento?",
                    "Ya no recibirá comandas. Todas sus cuentas deben estar cerradas.")
            }
            Button {
                visible: pagina.evento && pagina.evento.estado === "programado"
                text: "Cancelar evento"
                flat: true
                onClicked: confirmarEstado.pedir("cancelado", "¿Cancelar el evento?",
                    "Un evento cancelado ya no se puede editar ni iniciar.")
            }
            Item { Layout.fillWidth: true }
            Label {
                visible: pagina.evento && pagina.evento.estado === "en_curso" && pagina.evento.cuentas_abiertas > 0
                text: pagina.evento ? pagina.evento.cuentas_abiertas + " cuentas abiertas" : ""
                color: Tema.textoSuave
            }
        }

        Label {
            Layout.fillWidth: true
            visible: !pagina.editable
            text: "Este evento está " + (pagina.evento ? Tema.nombreEstadoEvento(pagina.evento.estado).toLowerCase() : "")
                  + ": se puede consultar, pero ya no editar."
            color: Tema.advertencia
            wrapMode: Text.WordWrap
        }

        ScrollView {
            id: desplazable
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth

            ColumnLayout {
                width: desplazable.availableWidth
                spacing: Tema.margen

                // ---------------- Datos ----------------
                Seccion {
                    titulo: "Datos del evento"
                    GridLayout {
                        Layout.fillWidth: true
                        columns: pagina.compacta ? 1 : 3
                        columnSpacing: Tema.espacio
                        rowSpacing: 4
                        enabled: pagina.editable

                        Label { text: "Fecha"; color: Tema.texto; font.bold: true }
                        Label { text: "Tipo de evento"; color: Tema.texto; font.bold: true; visible: !pagina.compacta }
                        Label { text: "Asistentes"; color: Tema.texto; font.bold: true; visible: !pagina.compacta }
                        TextField {
                            id: campoFecha
                            Layout.fillWidth: true
                            placeholderText: "AAAA-MM-DD"
                            inputMethodHints: Qt.ImhDate
                            validator: RegularExpressionValidator { regularExpression: /^\d{0,4}(-\d{0,2}(-\d{0,2})?)?$/ }
                        }
                        Label { text: "Tipo de evento"; color: Tema.texto; font.bold: true; visible: pagina.compacta }
                        ComboBox {
                            id: campoTipo
                            Layout.fillWidth: true
                            editable: true
                            model: ["Boda", "XV años", "Graduación", "Bautizo", "Cumpleaños", "Corporativo"]
                        }
                        Label { text: "Asistentes"; color: Tema.texto; font.bold: true; visible: pagina.compacta }
                        SpinBox {
                            id: campoAsistentes
                            Layout.fillWidth: true
                            from: 1
                            to: 100000
                            stepSize: 10
                            editable: true
                        }
                    }
                    Label {
                        visible: /^\d{4}-\d{2}-\d{2}$/.test(campoFecha.text)
                        text: /^\d{4}-\d{2}-\d{2}$/.test(campoFecha.text) ? Tema.fechaLarga(campoFecha.text) : ""
                        color: Tema.textoSuave
                    }

                    Label { text: "Organizadores"; color: Tema.texto; font.bold: true; Layout.topMargin: 6 }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 6
                        Repeater {
                            model: pagina.organizadores
                            delegate: Rectangle {
                                id: chip
                                required property string modelData
                                height: 32
                                width: filaChip.implicitWidth + 16
                                radius: 16
                                color: Tema.superficieAlta
                                border.color: Tema.borde
                                RowLayout {
                                    id: filaChip
                                    anchors.centerIn: parent
                                    spacing: 4
                                    Label { text: chip.modelData; color: Tema.texto }
                                    ToolButton {
                                        visible: pagina.editable
                                        implicitWidth: 24
                                        implicitHeight: 24
                                        text: "✕"
                                        onClicked: pagina.organizadores = pagina.organizadores.filter(o => o !== chip.modelData)
                                    }
                                }
                            }
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        visible: pagina.editable
                        TextField {
                            id: campoOrganizador
                            Layout.fillWidth: true
                            Layout.maximumWidth: 360
                            maximumLength: 120
                            placeholderText: "Nombre del organizador o cliente"
                            onAccepted: pagina.agregarOrganizador()
                        }
                        Button {
                            text: "Agregar"
                            flat: true
                            onClicked: pagina.agregarOrganizador()
                        }
                    }
                }

                // ---------------- Repetir (solo al crear) ----------------
                Seccion {
                    visible: pagina.esNuevo
                    titulo: "Repetir"
                    CheckBox {
                        id: checkRepetir
                        text: "Crear el mismo evento varias veces (por ejemplo, un brunch cada domingo)"
                    }
                    RowLayout {
                        visible: checkRepetir.checked
                        spacing: 8
                        Label { text: "Cada"; color: Tema.texto }
                        SpinBox { id: campoCada; from: 1; to: 365; value: 1; editable: true }
                        ComboBox {
                            id: comboUnidad
                            textRole: "texto"
                            valueRole: "valor"
                            model: [
                                { texto: "días", valor: "dias" },
                                { texto: "semanas", valor: "semanas" },
                                { texto: "meses", valor: "meses" }
                            ]
                            Component.onCompleted: currentIndex = 1
                        }
                        Label { text: "en total"; color: Tema.texto }
                        SpinBox { id: campoVeces; from: 2; to: 52; value: 4; editable: true }
                        Label { text: "veces"; color: Tema.texto }
                    }
                    Label {
                        Layout.fillWidth: true
                        visible: checkRepetir.checked && pagina.fechasRepetidas.length > 0
                        text: "Se crearán " + pagina.fechasRepetidas.length + " eventos: "
                              + pagina.fechasRepetidas.slice(0, 6).map(f => Tema.fechaCorta(f)).join(", ")
                              + (pagina.fechasRepetidas.length > 6 ? "…" : "")
                              + ". Cada uno lleva los mismos organizadores, mesas y paquetes."
                        color: Tema.textoSuave
                        wrapMode: Text.WordWrap
                    }
                }

                // ---------------- Mesas ----------------
                Seccion {
                    titulo: "Mesas (" + modeloMesas.count + ")"
                    Label {
                        Layout.fillWidth: true
                        text: "Cada mesa la atiende un mesero. Las mesas guardadas no se quitan, como las cuentas; sí puedes cambiarles el mesero."
                        color: Tema.textoSuave
                        font.pixelSize: Tema.letraChica
                        wrapMode: Text.WordWrap
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 8
                        Repeater {
                            model: modeloMesas
                            delegate: Rectangle {
                                id: tarjetaMesa
                                required property int index
                                required property int numero_mesa
                                required property int id_mesero
                                required property bool guardada
                                width: 230
                                height: 52
                                radius: Tema.radio
                                color: Tema.superficieAlta
                                border.color: Tema.borde
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 10
                                    anchors.rightMargin: 4
                                    spacing: 6
                                    Label {
                                        text: "Mesa " + tarjetaMesa.numero_mesa
                                        color: Tema.texto
                                        font.bold: true
                                    }
                                    ComboBox {
                                        Layout.fillWidth: true
                                        enabled: pagina.editable
                                        textRole: "nombre"
                                        valueRole: "id_usuario"
                                        model: pagina.meseros
                                        displayText: currentIndex < 0 ? "Mesero…" : currentText
                                        // La lista de meseros llega después: se recalcula cuando llega
                                        currentIndex: pagina.meseros.findIndex(m => m.id_usuario === tarjetaMesa.id_mesero)
                                        onActivated: modeloMesas.setProperty(tarjetaMesa.index, "id_mesero", currentValue)
                                    }
                                    ToolButton {
                                        visible: pagina.editable && !tarjetaMesa.guardada
                                        text: "✕"
                                        onClicked: modeloMesas.remove(tarjetaMesa.index)
                                    }
                                }
                            }
                        }
                    }
                    Button {
                        visible: pagina.editable
                        text: "+ Mesa"
                        flat: true
                        onClicked: pagina.agregarMesa()
                    }
                }

                // ---------------- Paquetes ----------------
                Seccion {
                    titulo: "Paquetes · " + Tema.dinero(pagina.totalPaquetes)
                    Label {
                        Layout.fillWidth: true
                        text: "La cantidad de cada producto es lo acordado con el cliente para este evento."
                        color: Tema.textoSuave
                        font.pixelSize: Tema.letraChica
                        wrapMode: Text.WordWrap
                    }
                    Repeater {
                        model: pagina.paquetesEvento
                        delegate: Rectangle {
                            id: tarjetaPaquete
                            required property var modelData
                            Layout.fillWidth: true
                            implicitHeight: contenidoPaquete.implicitHeight + 20
                            radius: Tema.radio
                            color: Tema.superficieAlta
                            border.color: Tema.borde

                            ColumnLayout {
                                id: contenidoPaquete
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 10
                                spacing: 4
                                RowLayout {
                                    Layout.fillWidth: true
                                    Label {
                                        Layout.fillWidth: true
                                        text: tarjetaPaquete.modelData.titulo + " · " + Tema.dinero(tarjetaPaquete.modelData.monto)
                                        color: Tema.texto
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }
                                    ToolButton {
                                        visible: pagina.editable
                                        text: "✕"
                                        onClicked: pagina.quitarPaquete(tarjetaPaquete.modelData.numero_paquete)
                                    }
                                }
                                Repeater {
                                    model: tarjetaPaquete.modelData.productos
                                    delegate: GridLayout {
                                        id: renglonProducto
                                        required property var modelData
                                        Layout.fillWidth: true
                                        columns: pagina.compacta ? 2 : 3
                                        columnSpacing: 8
                                        rowSpacing: 2
                                        Label {
                                            Layout.fillWidth: true
                                            text: renglonProducto.modelData.nombre
                                            color: Tema.texto
                                        }
                                        ComboBox {
                                            Layout.preferredWidth: 300
                                            Layout.fillWidth: pagina.compacta
                                            Layout.row: pagina.compacta ? 1 : 0
                                            Layout.column: pagina.compacta ? 0 : 1
                                            Layout.columnSpan: pagina.compacta ? 2 : 1
                                            enabled: pagina.editable
                                            textRole: "texto"
                                            valueRole: "valor"
                                            model: pagina.modosCobro
                                            currentIndex: pagina.modosCobro.findIndex(m => m.valor === renglonProducto.modelData.modo_cobro)
                                            onActivated: pagina.fijarModo(tarjetaPaquete.modelData.numero_paquete,
                                                                          renglonProducto.modelData.id_producto, currentValue)
                                        }
                                        SpinBox {
                                            Layout.row: 0
                                            Layout.column: pagina.compacta ? 1 : 2
                                            enabled: pagina.editable
                                            from: 1
                                            to: 1000000
                                            stepSize: 10
                                            editable: true
                                            value: renglonProducto.modelData.cantidad
                                            onValueModified: pagina.fijarCantidad(tarjetaPaquete.modelData.numero_paquete,
                                                                                 renglonProducto.modelData.id_producto, value)
                                        }
                                    }
                                }
                            }
                        }
                    }
                    RowLayout {
                        visible: pagina.editable
                        ComboBox {
                            id: comboPaquete
                            Layout.preferredWidth: 280
                            textRole: "titulo"
                            model: pagina.catalogoPaquetes.filter(p => !pagina.paquetesEvento.some(x => x.numero_paquete === p.numero_paquete))
                            displayText: count === 0 ? "No hay más paquetes" : (currentIndex < 0 ? "Elige un paquete…" : currentText)
                        }
                        Button {
                            text: "Agregar paquete"
                            flat: true
                            enabled: comboPaquete.currentIndex >= 0
                            onClicked: pagina.agregarPaquete(comboPaquete.model[comboPaquete.currentIndex])
                        }
                    }

                    // Cuenta del anfitrión: a la que se cargan los excedentes en modo
                    // "paga el anfitrión". Se elige entre las cuentas del evento.
                    ColumnLayout {
                        Layout.fillWidth: true
                        visible: !pagina.esNuevo && (pagina.usaAnfitrion || pagina.secuencialAnfitrion > 0)
                        spacing: 4
                        Label { text: "Cuenta del anfitrión"; color: Tema.texto; font.bold: true }
                        ComboBox {
                            Layout.preferredWidth: 320
                            enabled: pagina.editable
                            textRole: "texto"
                            valueRole: "valor"
                            model: [{ texto: "Ninguna", valor: 0 }].concat(
                                pagina.evento ? pagina.evento.cuentas
                                    .filter(c => c.estado === "abierta" || c.secuencial === pagina.secuencialAnfitrion)
                                    .map(c => ({ texto: c.cuenta, valor: c.secuencial })) : [])
                            currentIndex: Math.max(0, model.findIndex(c => c.valor === pagina.secuencialAnfitrion))
                            onActivated: pagina.secuencialAnfitrion = currentValue
                        }
                        Label {
                            Layout.fillWidth: true
                            visible: pagina.usaAnfitrion && pagina.secuencialAnfitrion === 0
                            text: pagina.evento && pagina.evento.cuentas.length === 0
                                  ? "Todavía no hay cuentas: cuando el anfitrión abra la suya en su mesa, elígela aquí."
                                  : "Elige la cuenta del anfitrión; sin ella, al acabarse lo incluido esos productos no se podrán pedir."
                            color: Tema.advertencia
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                // ---------------- Anticipos (evento ya guardado) ----------------
                Seccion {
                    visible: !pagina.esNuevo
                    titulo: "Anticipos · " + Tema.dinero(pagina.evento ? pagina.evento.total_anticipos : 0)
                    Label {
                        Layout.fillWidth: true
                        visible: pagina.totalPaquetes > 0
                        text: "Por cobrar de los paquetes: "
                              + Tema.dinero(Math.max(0, pagina.totalPaquetes - (pagina.evento ? pagina.evento.total_anticipos : 0)))
                        color: Tema.texto
                    }
                    Repeater {
                        model: pagina.evento ? pagina.evento.anticipos : []
                        delegate: RowLayout {
                            id: renglonAnticipo
                            required property var modelData
                            Layout.fillWidth: true
                            Label {
                                Layout.fillWidth: true
                                text: Tema.fechaCorta(renglonAnticipo.modelData.fecha)
                                color: Tema.textoSuave
                            }
                            Label {
                                text: Tema.dinero(renglonAnticipo.modelData.monto)
                                color: Tema.texto
                                font.bold: true
                            }
                        }
                    }
                    Label {
                        visible: pagina.evento && pagina.evento.anticipos.length === 0
                        text: "Sin anticipos todavía"
                        color: Tema.textoSuave
                    }
                    RowLayout {
                        visible: pagina.evento && pagina.evento.estado !== "cancelado"
                        spacing: 8
                        TextField {
                            id: campoAnticipo
                            Layout.preferredWidth: 140
                            placeholderText: "Monto"
                            inputMethodHints: Qt.ImhFormattedNumbersOnly
                            validator: RegularExpressionValidator { regularExpression: /^\d{0,8}([.,]\d{0,2})?$/ }
                        }
                        TextField {
                            id: campoFechaAnticipo
                            Layout.preferredWidth: 140
                            placeholderText: "Fecha (hoy)"
                            inputMethodHints: Qt.ImhDate
                        }
                        Button {
                            text: "Registrar anticipo"
                            enabled: campoAnticipo.text !== ""
                            onClicked: pagina.registrarAnticipo()
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        text: "El dinero recibido no se edita ni se borra."
                        color: Tema.textoSuave
                        font.pixelSize: Tema.letraChica
                    }
                }

                // ---------------- Cuentas (consulta, también de eventos pasados) ----------------
                Seccion {
                    visible: !pagina.esNuevo
                    titulo: "Cuentas (" + pagina.cuentasEvento.length + ")"
                    Label {
                        Layout.fillWidth: true
                        visible: pagina.cuentasEvento.length > 0
                        text: "Consumo " + Tema.dinero(pagina.cuentasEvento.reduce((s, c) => s + c.total, 0))
                              + " · pagado " + Tema.dinero(pagina.cuentasEvento.reduce((s, c) => s + c.pagado, 0))
                              + " · por cobrar " + Tema.dinero(pagina.cuentasEvento.reduce((s, c) => s + Math.max(0, c.saldo), 0))
                        color: Tema.texto
                        wrapMode: Text.WordWrap
                    }
                    Label {
                        visible: pagina.cuentasEvento.length === 0
                        text: "Este evento no tiene cuentas"
                        color: Tema.textoSuave
                    }
                    Repeater {
                        model: pagina.cuentasEvento
                        delegate: Rectangle {
                            id: filaCuenta
                            required property var modelData
                            required property int index
                            Layout.fillWidth: true
                            implicitHeight: contenidoCuenta.implicitHeight + 16
                            radius: 6
                            color: ratonCuenta.containsMouse ? Tema.superficieAlta : "transparent"

                            MouseArea {
                                id: ratonCuenta
                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: detalleCuenta.abrir(filaCuenta.modelData, pagina.evento.numero_evento)
                            }
                            RowLayout {
                                id: contenidoCuenta
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                spacing: 8
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 0
                                    Label {
                                        Layout.fillWidth: true
                                        text: filaCuenta.modelData.cuenta
                                        color: Tema.texto
                                        font.bold: true
                                        elide: Text.ElideRight
                                    }
                                    Label {
                                        Layout.fillWidth: true
                                        text: "Mesa " + filaCuenta.modelData.numero_mesa + " · " + filaCuenta.modelData.mesero
                                        color: Tema.textoSuave
                                        font.pixelSize: Tema.letraChica
                                        elide: Text.ElideRight
                                    }
                                }
                                Etiqueta {
                                    texto: filaCuenta.modelData.cancelada ? "Cancelada"
                                           : filaCuenta.modelData.estado === "abierta" ? "Abierta" : "Cerrada"
                                    tono: filaCuenta.modelData.cancelada ? Tema.peligro
                                          : filaCuenta.modelData.estado === "abierta" ? Tema.info : Tema.textoSuave
                                }
                                Label {
                                    Layout.preferredWidth: 96
                                    horizontalAlignment: Text.AlignRight
                                    text: Tema.dinero(filaCuenta.modelData.total)
                                    color: Tema.texto
                                }
                                Label {
                                    visible: !pagina.compacta
                                    Layout.preferredWidth: 110
                                    horizontalAlignment: Text.AlignRight
                                    text: filaCuenta.modelData.saldo > 0 ? "debe " + Tema.dinero(filaCuenta.modelData.saldo) : "pagada"
                                    color: filaCuenta.modelData.saldo > 0 ? Tema.acento : Tema.textoSuave
                                    font.pixelSize: Tema.letraChica
                                }
                            }
                        }
                    }
                }

                Item { implicitHeight: 4 }
            }
        }

        // ---------------- Guardar ----------------
        Label {
            Layout.fillWidth: true
            visible: pagina.error !== ""
            text: pagina.error
            color: Tema.peligro
            wrapMode: Text.WordWrap
        }
        RowLayout {
            Layout.fillWidth: true
            visible: pagina.editable
            Item { Layout.fillWidth: true }
            Button {
                text: "Descartar"
                flat: true
                onClicked: pagina.volver()
            }
            Button {
                text: pagina.guardando ? "Guardando…" : (pagina.esNuevo ? "Crear evento" : "Guardar cambios")
                highlighted: true
                enabled: !pagina.guardando
                onClicked: pagina.guardar()
            }
        }
    }

    DialogoDetalleCuenta { id: detalleCuenta }

    DialogoConfirmar {
        id: confirmarEstado
        property string estado: ""
        function pedir(nuevoEstado, titulo, mensaje) {
            estado = nuevoEstado
            confirmarEstado.titulo = titulo
            confirmarEstado.mensaje = mensaje
            peligroso = nuevoEstado === "cancelado"
            textoAceptar = nuevoEstado === "en_curso" ? "Iniciar" : nuevoEstado === "finalizado" ? "Finalizar" : "Cancelar evento"
            open()
        }
        onConfirmado: pagina.cambiarEstado(estado)
    }

    // Recuadro con título para cada parte de la ficha
    component Seccion: Rectangle {
        id: seccion
        property string titulo: ""
        default property alias contenido: columna.data
        Layout.fillWidth: true
        implicitHeight: columna.implicitHeight + 32
        radius: Tema.radio
        color: Tema.superficie
        border.color: Tema.borde

        ColumnLayout {
            id: columna
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 16
            spacing: 8
            Label {
                text: seccion.titulo
                color: Tema.texto
                font.pixelSize: Tema.letraMediana
                font.bold: true
            }
        }
    }
}
