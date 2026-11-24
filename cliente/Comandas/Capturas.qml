import QtQuick

// Recorrido de capturas para revisar el diseño en distintos tamaños sin tocar
// la app a mano: entra con cada rol, abre cada pantalla y guarda una imagen.
//
//   cliente_comandas --servidor ws://… --capturas /carpeta --tam 390x760
//
// Solo se carga con --capturas (Main.qml). Necesita la base de la demo
// (usuarios meseroDemo1, cocinaDemo1, cajeroDemo y adminDemo).
QtObject {
    id: recorrido

    required property var ventana       // la ApplicationWindow
    required property var pila          // el StackView de Main.qml
    required property var lienzo        // lo que se ve: letrero de demo y pantallas
    property string carpeta: "."
    property string tam: ""
    property var pasos: []
    property int paso: 0
    property int fallas: 0

    function argumento(nombre, porOmision) {
        const a = Qt.application.arguments
        const i = a.indexOf(nombre)
        return i >= 0 && i + 1 < a.length ? a[i + 1] : porOmision
    }

    function pantalla() { return pila.currentItem }
    function seccion() { return pila.currentItem && pila.currentItem.seccion ? pila.currentItem.seccion : null }

    function capturar(nombre) {
        const archivo = carpeta + "/" + tam + "-" + nombre + ".png"
        recorrido.lienzo.grabToImage(function (r) {
            if (!r.saveToFile(archivo)) {
                console.warn("No se pudo guardar", archivo)
                recorrido.fallas++
            } else {
                console.log("Captura:", archivo)
            }
        })
    }

    function entrar(usuario) {
        Api.iniciarSesion(usuario, Api.contrasenaDemo, function (r) {
            if (r.codigo === "OK")
                recorrido.ventana.abrirInicioPorRol(r.datos)
            else {
                console.warn("No se pudo entrar como", usuario, r.mensaje)
                recorrido.fallas++
            }
        })
    }

    // Cada paso: [espera en ms antes de hacerlo, qué hacer]
    function armar() {
        const p = []
        const listo = 2500
        p.push([3000, () => capturar("01-login")])

        // Mesero
        p.push([100, () => entrar("meseroDemo1")])
        p.push([listo, () => capturar("02-mesero-elegir-cuenta")])
        p.push([100, function () {
            Api.listarCuentas(1, function (r) {
                const v = recorrido.pantalla().vista
                v.mesa = { numero_mesa: 1, mia: true }
                v.cuenta = r.datos[0]
            })
        }])
        p.push([listo, () => capturar("03-mesero-tomar-orden")])
        p.push([100, function () {
            const v = recorrido.pantalla().vista
            for (const prod of v.visibles.filter(x => x.disponible).slice(0, 3))
                v.agregar(prod)
        }])
        p.push([1200, () => capturar("04-mesero-orden-con-productos")])
        p.push([100, () => recorrido.pantalla().pestana = 1])
        p.push([listo, () => capturar("05-mesero-mis-ordenes")])
        p.push([100, () => ventana.cerrarSesion()])

        // Cocina
        p.push([1500, () => entrar("cocinaDemo1")])
        p.push([listo, () => capturar("06-cocina")])
        p.push([100, () => ventana.cerrarSesion()])

        // Cajero
        p.push([1500, () => entrar("cajeroDemo")])
        p.push([listo, () => capturar("07-caja-cuentas")])
        p.push([100, function () {
            const caja = recorrido.seccion()
            if (caja.cuentas.length > 0)
                caja.elegir(caja.cuentas[0])
        }])
        p.push([listo, () => capturar("08-caja-detalle")])
        p.push([100, () => ventana.cerrarSesion()])

        // Administrador: el menú y cada sección
        p.push([1500, () => entrar("adminDemo")])
        p.push([listo, () => capturar("09-admin-menu")])
        const secciones = ["eventos", "productos", "paquetes", "ingredientes", "usuarios", "bitacora"]
        let n = 10
        for (const clave of secciones) {
            const numero = n++
            p.push([100, () => recorrido.pantalla().abrirSeccion(clave)])
            p.push([listo + 1000, () => capturar(numero + "-admin-" + clave)])
            if (clave === "eventos") {
                // La ficha del evento en curso
                p.push([100, function () {
                    const ev = recorrido.seccion()
                    if (ev.actual)
                        ev.abrirEvento(ev.actual, ev.actual.fecha)
                }])
                p.push([listo + 1000, () => capturar(numero + "b-admin-ficha-evento")])
                p.push([100, () => recorrido.pantalla().atras()])
                p.push([800, () => {}])
            }
            p.push([100, () => recorrido.pantalla().atras()])
        }
        p.push([1500, function () {
            console.log("Capturas terminadas, fallas:", recorrido.fallas)
            Qt.exit(recorrido.fallas > 0 ? 1 : 0)
        }])
        return p
    }

    property Timer _reloj: Timer {
        repeat: false
        onTriggered: {
            const [, accion] = recorrido.pasos[recorrido.paso]
            try {
                accion()
            } catch (e) {
                console.warn("Falló el paso", recorrido.paso, e)
                recorrido.fallas++
            }
            recorrido.paso++
            if (recorrido.paso < recorrido.pasos.length) {
                interval = recorrido.pasos[recorrido.paso][0]
                start()
            }
        }
    }

    Component.onCompleted: {
        carpeta = argumento("--capturas", ".")
        tam = argumento("--tam", ventana.width + "x" + ventana.height)
        const partes = tam.split("x").map(Number)
        // Un celular acostado mide menos que el mínimo de la ventana de escritorio
        ventana.minimumWidth = 280
        ventana.minimumHeight = 200
        ventana.width = partes[0]
        ventana.height = partes[1]
        pasos = armar()
        _reloj.interval = pasos[0][0]
        _reloj.start()
    }
}
