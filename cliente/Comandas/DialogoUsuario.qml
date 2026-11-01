import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Alta y edición de un usuario, con el dato del puesto que pide su subtipo.
// Las reglas las aplica el servidor (aquí, Api); el diálogo muestra sus mensajes.
// El rol solo se elige en el alta. El contenido se desplaza si no cabe en el teléfono.
Dialog {
    id: dialogo

    property var usuario: null      // null = alta
    property bool guardando: false
    property string error: ""
    readonly property string rol: comboRol.currentValue || ""

    function elegir(combo, valor) {
        combo.currentIndex = Math.max(0, combo.indexOfValue(valor))
    }

    function abrirNuevo() {
        usuario = null
        campoNombre.text = ""
        campoUsuario.text = ""
        campoFecha.text = ""
        comboRol.currentIndex = 0
        comboTurno.currentIndex = 0
        comboEstacion.currentIndex = 0
        comboNivel.currentIndex = 0
        campoCaja.value = 1
        abrir()
    }

    function abrirEdicion(datos) {
        usuario = datos
        campoNombre.text = datos.nombre
        campoUsuario.text = datos.usuario
        campoFecha.text = datos.fecha_nacimiento
        elegir(comboRol, datos.rol)
        elegir(comboTurno, datos.turno_asignado)
        elegir(comboEstacion, datos.estacion_asignada)
        elegir(comboNivel, datos.nivel_acceso)
        campoCaja.value = datos.caja_asignada || 1
        abrir()
    }

    function abrir() {
        campoContrasena.text = ""
        verContrasena.checked = false
        error = ""
        desplazable.contentY = 0
        open()
    }

    function guardar() {
        guardando = true
        error = ""
        const datos = {
            nombre: campoNombre.text, usuario: campoUsuario.text, fecha_nacimiento: campoFecha.text,
            contrasena: campoContrasena.text, rol: rol, turno_asignado: comboTurno.currentValue,
            estacion_asignada: comboEstacion.currentValue, nivel_acceso: comboNivel.currentValue,
            caja_asignada: campoCaja.value
        }
        if (usuario)
            datos.id_usuario = usuario.id_usuario
        Api.guardarUsuario(datos, function (r) {
            if (!dialogo)
                return
            dialogo.guardando = false
            if (r.codigo === "OK") {
                dialogo.close()
                Avisos.exito(r.mensaje)
            } else {
                dialogo.error = r.mensaje
                // El mensaje queda al final del formulario: hay que dejarlo a la vista
                Qt.callLater(function () {
                    desplazable.contentY = Math.max(0, desplazable.contentHeight - desplazable.height)
                })
            }
        })
    }

    title: usuario ? "Editar usuario" : "Nuevo usuario"
    modal: true
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(460, parent ? parent.width - 32 : 460)
    height: Math.min(implicitHeight, parent ? parent.height - 32 : implicitHeight)

    contentItem: Flickable {
        id: desplazable
        implicitHeight: formulario.implicitHeight
        contentHeight: formulario.implicitHeight
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: formulario
            width: desplazable.width
            spacing: 6
            enabled: !dialogo.guardando

            Label {
                text: "Nombre completo"
                color: Tema.texto
                font.bold: true
            }
            TextField {
                id: campoNombre
                Layout.fillWidth: true
                maximumLength: 120
                placeholderText: "p. ej. Laura Méndez Ruiz"
            }

            Label {
                text: "Usuario"
                color: Tema.texto
                font.bold: true
            }
            TextField {
                id: campoUsuario
                Layout.fillWidth: true
                maximumLength: 40
                placeholderText: "p. ej. lmendez"
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhPreferLowercase
            }

            Label {
                text: "Fecha de nacimiento"
                color: Tema.texto
                font.bold: true
            }
            TextField {
                id: campoFecha
                Layout.fillWidth: true
                maximumLength: 10
                placeholderText: "AAAA-MM-DD"
                inputMethodHints: Qt.ImhDate
            }

            Label {
                text: dialogo.usuario ? "Contraseña nueva" : "Contraseña"
                color: Tema.texto
                font.bold: true
            }
            RowLayout {
                Layout.fillWidth: true
                TextField {
                    id: campoContrasena
                    Layout.fillWidth: true
                    echoMode: verContrasena.checked ? TextInput.Normal : TextInput.Password
                    placeholderText: dialogo.usuario ? "Vacía para no cambiarla" : "Contraseña"
                }
                Button {
                    id: verContrasena
                    checkable: true
                    flat: true
                    text: checked ? "Ocultar" : "Mostrar"
                }
            }
            Label {
                Layout.fillWidth: true
                text: "8 caracteres o más, una mayúscula, una minúscula, un número y 2 símbolos de * / _ - + ."
                color: Tema.textoSuave
                font.pixelSize: Tema.letraChica
                wrapMode: Text.WordWrap
            }

            Label {
                text: "Rol"
                color: Tema.texto
                font.bold: true
            }
            ComboBox {
                id: comboRol
                Layout.fillWidth: true
                enabled: dialogo.usuario === null
                textRole: "texto"
                valueRole: "valor"
                model: [
                    { texto: "Mesero", valor: "mesero" },
                    { texto: "Bar / cocina", valor: "bar_cocina" },
                    { texto: "Cajero / administrador", valor: "cajero_admin" }
                ]
            }

            Label {
                visible: dialogo.rol === "mesero"
                text: "Turno"
                color: Tema.texto
                font.bold: true
            }
            ComboBox {
                id: comboTurno
                visible: dialogo.rol === "mesero"
                Layout.fillWidth: true
                textRole: "texto"
                valueRole: "valor"
                model: [
                    { texto: "Matutino", valor: "matutino" },
                    { texto: "Vespertino", valor: "vespertino" },
                    { texto: "Nocturno", valor: "nocturno" }
                ]
            }

            Label {
                visible: dialogo.rol === "bar_cocina"
                text: "Estación"
                color: Tema.texto
                font.bold: true
            }
            ComboBox {
                id: comboEstacion
                visible: dialogo.rol === "bar_cocina"
                Layout.fillWidth: true
                textRole: "texto"
                valueRole: "valor"
                model: [
                    { texto: "Cocina", valor: "cocina" },
                    { texto: "Bar", valor: "bar" }
                ]
            }

            Label {
                visible: dialogo.rol === "cajero_admin"
                text: "Nivel de acceso"
                color: Tema.texto
                font.bold: true
            }
            ComboBox {
                id: comboNivel
                visible: dialogo.rol === "cajero_admin"
                Layout.fillWidth: true
                textRole: "texto"
                valueRole: "valor"
                model: [
                    { texto: "Cajero: cobra y cierra cuentas", valor: "cajero" },
                    { texto: "Administrador: todo el sistema", valor: "administrador" }
                ]
            }

            Label {
                visible: dialogo.rol === "cajero_admin"
                text: "Caja asignada"
                color: Tema.texto
                font.bold: true
            }
            SpinBox {
                id: campoCaja
                visible: dialogo.rol === "cajero_admin"
                Layout.fillWidth: true
                from: 1
                to: 99
                editable: true
            }

            Label {
                Layout.fillWidth: true
                visible: dialogo.error !== ""
                text: dialogo.error
                color: Tema.peligro
                wrapMode: Text.WordWrap
            }
        }
    }

    footer: DialogButtonBox {
        Button {
            text: "Cancelar"
            flat: true
            enabled: !dialogo.guardando
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
        Button {
            text: dialogo.guardando ? "Guardando…" : "Guardar"
            highlighted: true
            enabled: !dialogo.guardando
            DialogButtonBox.buttonRole: DialogButtonBox.ActionRole
            onClicked: dialogo.guardar()
        }
    }
}
