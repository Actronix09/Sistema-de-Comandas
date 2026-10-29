pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Marcador de las pantallas que construirás tú. Cada una trae su especificación,
// sacada de los menús del sistema en terminal y de tu esquema de BD.
Item {
    id: pantalla

    required property string clave
    signal volver()

    readonly property var especificaciones: ({
        logs: {
            ejercicio: 1, titulo: "Ver logs", origen: "Admin › Ver logs",
            puntos: [
                "Dos registros para elegir: autenticación (inicios de sesión, altas, recuperaciones) y cocina (comandas creadas y cambios de estado).",
                "Muestra las últimas 20 líneas del registro elegido, la más reciente arriba.",
                "Mensaje claro cuando el registro está vacío.",
                "Botón para volver al menú de administración."
            ],
            api: ["GET /logs", "GET /logs/{nombre}?lineas=20"]
        },
        recuperar: {
            ejercicio: 2, titulo: "Recuperar contraseña", origen: "Menú principal › Recuperar contraseña",
            puntos: [
                "Paso 1: pide el usuario y verifica en el servidor que exista.",
                "Rechaza las cuentas de administrador, igual que hoy.",
                "Paso 2: nueva contraseña oculta, con el panel de requisitos: 8 caracteres o más, una mayúscula, una minúscula, un número y 2 símbolos de * / _ - + .",
                "Ante cualquier error permite reintentar sin salir de la pantalla.",
                "Ojo: no verifica identidad. Conviene que este cambio lo haga un administrador."
            ],
            api: ["GET /usuarios/disponible?usuario=", "POST /auth/recuperar"]
        },
        crear_usuario: {
            ejercicio: 3, titulo: "Crear usuario", origen: "Menú principal › Crear usuario",
            puntos: [
                "Nombre completo y usuario; el usuario se verifica en el servidor para que no se repita.",
                "Contraseña con el mismo panel de requisitos que Recuperar contraseña.",
                "Fecha de nacimiento: tu esquema la exige (NOT NULL).",
                "Rol: mesero o bar/cocina. El administrador solo se crea desde Administración.",
                "Dato del puesto: turno (matutino, vespertino o nocturno) para mesero; estación (cocina o bar) para bar/cocina.",
                "Sin correo ni teléfono."
            ],
            api: ["GET /usuarios/disponible?usuario=", "POST /usuarios"]
        },
        usuarios: {
            ejercicio: 4, titulo: "Administrador de usuarios", origen: "Admin › Administrador de usuarios",
            puntos: [
                "Lista con nombre, usuario y rol. Nunca muestra hashes de contraseña.",
                "Alta con el formulario del ejercicio 3, más el rol cajero/administrador (nivel de acceso y caja asignada).",
                "Edición: si cambia la contraseña, se valida otra vez y el servidor guarda solo el hash. En el sistema en C se guardaba en claro.",
                "Baja con confirmación; impide borrar tu propia cuenta."
            ],
            api: ["GET /usuarios", "POST /usuarios", "PATCH /usuarios/{id}", "DELETE /usuarios/{id}"]
        },
        productos: {
            ejercicio: 5, titulo: "Administrador de productos", origen: "Admin › Administrador de productos",
            puntos: [
                "Cuadrícula de productos con nombre, descripción, categoría, precio y variante (puedes reutilizar TarjetaProducto).",
                "Alta y edición: nombre, descripción, categoría (alimentos, bebidas o dulcería), precio y, si es variante, su producto principal.",
                "Receta: ingredientes con cantidad, con + y − como en la terminal. En el sistema en C estos cambios nunca se guardaban; aquí deben llegar al servidor.",
                "Baja con confirmación."
            ],
            api: ["GET /productos", "POST /productos", "PATCH /productos/{id}", "PUT /productos/{id}/receta", "DELETE /productos/{id}"]
        }
    })

    readonly property var spec: especificaciones[clave] || { ejercicio: 0, titulo: clave, origen: "", puntos: [], api: [] }

    Rectangle {
        anchors.fill: parent
        color: Tema.fondo
    }

    Flickable {
        anchors.fill: parent
        contentHeight: columna.implicitHeight + 80
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.vertical: ScrollBar {}

        ColumnLayout {
            id: columna
            x: Math.max(32, (parent.width - width) / 2)
            y: 32
            width: Math.min(760, pantalla.width - 64)
            spacing: 12

            Button {
                text: "← Volver"
                flat: true
                onClicked: pantalla.volver()
            }

            Etiqueta {
                texto: "Por construir · Ejercicio " + pantalla.spec.ejercicio
                tono: Tema.advertencia
            }

            Label {
                text: pantalla.spec.titulo
                color: Tema.texto
                font.pixelSize: 30
                font.bold: true
            }

            Label {
                text: "Equivale a: " + pantalla.spec.origen
                color: Tema.textoSuave
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: 8
                implicitHeight: requisitos.implicitHeight + 32
                radius: Tema.radio
                color: Tema.superficie
                border.color: Tema.borde

                ColumnLayout {
                    id: requisitos
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 16
                    spacing: 8

                    Label {
                        text: "Esta pantalla la construyes tú. Debe cumplir:"
                        color: Tema.texto
                        font.bold: true
                    }
                    Repeater {
                        model: pantalla.spec.puntos
                        delegate: RowLayout {
                            id: punto
                            required property string modelData
                            Layout.fillWidth: true
                            spacing: 8
                            Label {
                                Layout.alignment: Qt.AlignTop
                                text: "•"
                                color: Tema.acento
                                font.bold: true
                            }
                            Label {
                                Layout.fillWidth: true
                                text: punto.modelData
                                color: Tema.texto
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: llamadas.implicitHeight + 32
                radius: Tema.radio
                color: Tema.superficie
                border.color: Tema.borde

                ColumnLayout {
                    id: llamadas
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 16
                    spacing: 6

                    Label {
                        text: "Llamadas a la API que va a necesitar"
                        color: Tema.texto
                        font.bold: true
                    }
                    Repeater {
                        model: pantalla.spec.api
                        delegate: Label {
                            required property string modelData
                            text: modelData
                            color: Tema.info
                            font.family: "monospace"
                        }
                    }
                    Label {
                        Layout.fillWidth: true
                        Layout.topMargin: 6
                        text: "Mientras no exista el servidor, agrégalas a ApiSimulada.qml con la misma forma {codigo, mensaje, datos}."
                        color: Tema.textoSuave
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                text: "Patrón a seguir: AdminIngredientes.qml (lista, diálogo y confirmación) y DialogoIngrediente.qml (formulario que muestra la validación del servidor)."
                color: Tema.textoSuave
                wrapMode: Text.WordWrap
            }
        }
    }
}
