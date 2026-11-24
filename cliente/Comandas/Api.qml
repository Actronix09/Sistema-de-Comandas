pragma Singleton
import QtQuick
import QtWebSockets

// ---------------------------------------------------------------------------
//  Api
//
//  Es la única puerta entre las pantallas y los datos. Habla con el servidor
//  en C por un WebSocket con mensajes JSON:
//      petición   {id, op, datos}
//      respuesta  {id, ok: true, datos}  o  {id, ok: false, error: {codigo, mensaje}}
//      aviso      {aviso, datos}   (sin id: lo manda el servidor cuando otro cambia algo)
//
//  Cada método recibe al final un callback que se llama con
//      { codigo:  "OK" | "CREDENCIALES_INVALIDAS" | "VALIDACION_FALLIDA" |
//                 "SIN_STOCK" | "NO_AUTORIZADO" | "ERROR" | ...,
//        mensaje: texto para mostrar,
//        datos:   lo pedido, o null }
//  Las pantallas no saben que hay red: es la misma forma que tenía la Api
//  simulada (ver el historial de Git), y por eso no cambiaron.
//
//  El servidor manda los datos sueltos (una comanda por unidad) y aquí se
//  agrupan como las muestran las pantallas: rondas, líneas "2× Cerveza", etc.
// ---------------------------------------------------------------------------
QtObject {
    id: api

    signal comandasCambiaron()
    signal cuentasCambiaron()
    signal inventarioCambio()
    signal usuariosCambiaron()
    signal catalogoCambio()       // productos y paquetes
    signal eventosCambiaron()     // eventos, sus mesas, paquetes y anticipos
    // datos: { id_mesero, numero_mesa, cuenta, productos: ["1× Margarita", ...] }
    signal ordenLista(var datos)
    // Se cayó la conexión con la sesión abierta: el servidor la olvidó
    signal sesionPerdida()

    // A dónde conectarse. En el navegador es el servidor de la demo; en la red
    // local se pasa al abrir la app: cliente_comandas --servidor ws://192.168.1.99:9000
    property string servidor: _servidorInicial()
    // En el navegador corre la demo pública, con las cuentas de prueba
    readonly property bool modoDemo: Qt.platform.os === "wasm" || Qt.application.arguments.indexOf("--demo") >= 0
    readonly property bool conectado: _socket.status === WebSocket.Open
    readonly property string contrasenaDemo: "Demo_123."

    // Usuario con sesión iniciada (null si no hay sesión)
    property var usuarioActual: null
    // Dentro de cajero_admin, nivel_acceso separa lo que puede cada quien: el
    // cajero solo cobra y cierra cuentas (decisión 4.5 de PENDIENTES.md).
    readonly property bool esAdministrador: usuarioActual !== null && usuarioActual.nivel_acceso === "administrador"

    readonly property int esperaMaximaMs: 15000

    // ============================== Conexión ==============================

    property var _pendientes: ({})     // id → { cb, transformar, temporizador }
    property int _siguienteId: 1
    property var _cola: []             // lo que se pidió antes de conectar

    property WebSocket _socket: WebSocket {
        url: api.servidor
        active: true
        onTextMessageReceived: (texto) => api._recibir(texto)
        onStatusChanged: (estado) => {
            if (estado === WebSocket.Open) {
                api._reconexion.stop()
                const cola = api._cola
                api._cola = []
                for (const texto of cola)
                    sendTextMessage(texto)
            } else if (estado === WebSocket.Closed || estado === WebSocket.Error) {
                api._alPerderConexion()
            }
        }
    }

    // Reintenta cada 3 s mientras no haya conexión. Para reintentar se apaga y
    // se prende el socket, y apagarlo vuelve a avisar "cerrado" y a programar
    // este temporizador: por eso se detiene al conectar y no hace nada si ya
    // hay conexión. Sin eso, cerraba cada 3 s la conexión que acababa de abrir.
    property Timer _reconexion: Timer {
        interval: 3000
        repeat: false
        onTriggered: {
            if (api.conectado)
                return
            api._socket.active = false
            api._socket.active = true
        }
    }

    function _servidorInicial() {
        const argumentos = Qt.application.arguments
        const i = argumentos.indexOf("--servidor")
        if (i >= 0 && i + 1 < argumentos.length)
            return argumentos[i + 1]
        return Qt.platform.os === "wasm" ? "wss://lilyth.tail2ed638.ts.net" : "ws://localhost:9000"
    }

    function _alPerderConexion() {
        // Lo que esperaba respuesta ya no la va a recibir
        const pendientes = _pendientes
        _pendientes = ({})
        for (const id in pendientes) {
            pendientes[id].temporizador.destroy()
            _entregar(pendientes[id].cb, _falla("ERROR", "Se perdió la conexión con el servidor"))
        }
        // La sesión vive en la conexión: al reconectar hay que volver a entrar
        if (usuarioActual !== null) {
            usuarioActual = null
            sesionPerdida()
        }
        _reconexion.restart()
    }

    // Manda {id, op, datos}. Cuando llegue la respuesta llama a cb con la forma
    // de arriba; "transformar" recibe los datos del servidor y regresa
    // { datos, mensaje } para la pantalla.
    function _pedir(op, datos, cb, transformar) {
        const id = _siguienteId++
        const texto = JSON.stringify({ id: id, op: op, datos: datos === undefined ? null : datos })
        const temporizador = Qt.createQmlObject("import QtQml; Timer { repeat: false }", api, "EsperaApi")
        temporizador.interval = esperaMaximaMs
        temporizador.triggered.connect(function () {
            const pendiente = api._pendientes[id]
            if (!pendiente)
                return
            delete api._pendientes[id]
            temporizador.destroy()
            api._entregar(cb, api._falla("ERROR", "El servidor no respondió a tiempo"))
        })
        _pendientes[id] = { cb: cb, transformar: transformar, temporizador: temporizador }
        temporizador.start()
        if (conectado)
            _socket.sendTextMessage(texto)
        else
            _cola.push(texto)
    }

    function _recibir(texto) {
        let mensaje
        try {
            mensaje = JSON.parse(texto)
        } catch (e) {
            console.warn("Mensaje del servidor que no es JSON:", texto)
            return
        }
        if (mensaje.aviso !== undefined) {
            _avisar(mensaje.aviso, mensaje.datos)
            return
        }
        const pendiente = _pendientes[mensaje.id]
        if (!pendiente)
            return
        delete _pendientes[mensaje.id]
        pendiente.temporizador.destroy()

        if (!mensaje.ok) {
            const error = mensaje.error || {}
            _entregar(pendiente.cb, _falla(error.codigo || "ERROR", error.mensaje || "Error del servidor"))
            return
        }
        let resultado = { datos: mensaje.datos, mensaje: "" }
        if (pendiente.transformar) {
            try {
                resultado = pendiente.transformar(mensaje.datos)
            } catch (e) {
                console.warn("No se pudo leer la respuesta:", e)
                _entregar(pendiente.cb, _falla("ERROR", "La respuesta del servidor no se pudo leer"))
                return
            }
        }
        _entregar(pendiente.cb, _ok(resultado.datos, resultado.mensaje))
    }

    function _avisar(aviso, datos) {
        switch (aviso) {
        case "comandas": comandasCambiaron(); break
        case "cuentas": cuentasCambiaron(); break
        case "inventario": inventarioCambio(); break
        case "usuarios": usuariosCambiaron(); break
        case "catalogo": catalogoCambio(); break
        case "eventos": eventosCambiaron(); break
        case "orden_lista": ordenLista(datos); break
        }
    }

    // ============================== Sesión ==============================

    function iniciarSesion(usuario, contrasena, cb) {
        _pedir("OP_LOGIN", { usuario: String(usuario || ""), contrasena: String(contrasena || "") }, cb, function (d) {
            api.usuarioActual = d
            return { datos: d, mensaje: "Bienvenido, " + d.nombre }
        })
    }

    function cerrarSesion() {
        if (usuarioActual !== null)
            _pedir("OP_LOGOUT", null, null)
        usuarioActual = null
    }

    function eventoEnCurso(cb) {
        _pedir("OP_EVENTO_EN_CURSO", null, cb)
    }

    // ============================ Catálogo ============================

    // Cada producto trae sus ingredientes y cuántas unidades alcanzan con el
    // stock actual (maximo). La revisión definitiva la hace crearComandas.
    function listarProductos(cb) {
        _pedir("OP_LISTAR_PRODUCTOS", null, cb, function (lista) {
            for (const p of lista)
                p.precio = Number(p.precio)    // llega como texto ("95.00")
            return { datos: lista, mensaje: "" }
        })
    }

    // ========================== Mesas y cuentas ==========================

    // Las mesas del evento en curso; las del mesero con sesión van primero.
    function listarMesas(cb) {
        _pedir("OP_LISTAR_MESAS", null, cb)
    }

    function listarCuentas(numeroMesa, cb) {
        _pedir("OP_LISTAR_CUENTAS", { numero_mesa: numeroMesa }, cb)
    }

    // Abre una cuenta con nombre en una mesa
    function crearCuenta(numeroMesa, nombre, cb) {
        _pedir("OP_CREAR_CUENTA", { numero_mesa: numeroMesa, nombre: String(nombre || "") }, cb, function (d) {
            return { datos: d, mensaje: "Se abrió la cuenta " + d.cuenta }
        })
    }

    // ============================ Comandas ============================

    // lineas: [{ id_producto, cantidad, detalles }]
    // secuencial: una cuenta abierta de esa mesa (las nuevas se abren con crearCuenta).
    function crearComandas(numeroMesa, secuencial, lineas, cb) {
        _pedir("OP_CREAR_PEDIDO", { numero_mesa: numeroMesa, secuencial: secuencial, lineas: lineas }, cb, function (d) {
            d.cuenta = d.cuenta + " · " + d.folio
            return { datos: d, mensaje: "Se enviaron " + d.creadas + (d.creadas === 1 ? " comanda" : " comandas")
                                        + " · " + d.cuenta }
        })
    }

    // Comandas agrupadas en rondas: las que una cuenta pidió en el mismo envío.
    // filtro: { estacion: "cocina" | "bar" | "todas", id_mesero, estados: [...] }
    function listarRondas(filtro, cb) {
        const f = filtro || {}
        _pedir("OP_LISTAR_PEDIDOS", { estacion: f.estacion || "todas", id_mesero: f.id_mesero || 0,
                                      estados: f.estados || [] }, cb, function (comandas) {
            const grupos = {}
            const rondas = []
            for (const c of comandas) {
                c.precio_cobrado = Number(c.precio_cobrado)
                const clave = c.secuencial + "|" + c.fecha_hora + "|" + c.id_mesero
                if (!grupos[clave]) {
                    grupos[clave] = {
                        clave: clave, fecha_hora: c.fecha_hora,
                        secuencial: c.secuencial, numero_mesa: c.numero_mesa, folio: c.folio,
                        nombre_cuenta: c.nombre_cuenta, cuenta: c.cuenta,
                        id_mesero: c.id_mesero, mesero: c.mesero,
                        comandas: []
                    }
                    rondas.push(grupos[clave])
                }
                grupos[clave].comandas.push({
                    id_comanda: c.id_comanda, id_producto: c.id_producto, producto: c.producto,
                    categoria: c.categoria, estacion: c.estacion, estado: c.estado,
                    detalles: c.detalles, precio_cobrado: c.precio_cobrado, tipo_cobro: c.tipo_cobro
                })
            }
            for (const r of rondas) {
                r.lineas = api._agruparLineas(r.comandas)
                r.total = r.comandas.filter(c => c.estado !== "cancelada" && c.tipo_cobro !== "anfitrion")
                                    .reduce((s, c) => s + c.precio_cobrado, 0)
            }
            rondas.sort((a, b) => a.fecha_hora - b.fecha_hora)
            return { datos: rondas, mensaje: "" }
        })
    }

    function cambiarEstado(ids, nuevoEstado, cb) {
        _pedir("OP_CAMBIAR_ESTADO_PEDIDO", { ids: ids, estado: nuevoEstado }, cb, function (d) {
            const n = d.actualizadas
            return { datos: d, mensaje: n + (n === 1 ? " comanda" : " comandas") + " → " + Tema.nombreEstado(nuevoEstado) }
        })
    }

    // Cajero o administrador. Una cancelada ya no cambia; no se toca una cuenta
    // cerrada ni se deja saldo a favor.
    function cancelarComandas(ids, cb) {
        _pedir("OP_ELIMINAR_PEDIDO", { ids: ids }, cb, function (d) {
            d.saldo = Number(d.saldo)
            const n = d.canceladas
            return { datos: d, mensaje: n === 1 ? "Se canceló 1 comanda" : "Se cancelaron " + n + " comandas" }
        })
    }

    // ========================== Ingredientes ==========================

    // stock y stock_minimo van en la unidad del ingrediente (g, ml o pieza)
    function listarIngredientes(cb) {
        _pedir("OP_LISTAR_INGREDIENTES", null, cb, function (lista) {
            for (const i of lista)
                api._numerosIngrediente(i)
            return { datos: lista, mensaje: "" }
        })
    }

    // datos: { id_ingrediente (si se edita), nombre, stock, unidad_medida, stock_minimo }
    function guardarIngrediente(datos, cb) {
        const op = datos.id_ingrediente ? "OP_MODIFICAR_INGREDIENTE" : "OP_CREAR_INGREDIENTE"
        const enviar = {
            nombre: String(datos.nombre || ""), stock: Number(datos.stock),
            unidad_medida: datos.unidad_medida || null,
            stock_minimo: datos.stock_minimo === undefined || datos.stock_minimo === "" ? null : Number(datos.stock_minimo)
        }
        if (datos.id_ingrediente)
            enviar.id_ingrediente = datos.id_ingrediente
        _pedir(op, enviar, cb, function (d) {
            return { datos: api._numerosIngrediente(d),
                     mensaje: datos.id_ingrediente ? "Ingrediente actualizado" : "Ingrediente agregado" }
        })
    }

    function ajustarStock(idIngrediente, delta, cb) {
        _pedir("OP_AJUSTAR_STOCK", { id_ingrediente: idIngrediente, delta: Number(delta) }, cb, function (d) {
            return { datos: api._numerosIngrediente(d), mensaje: "" }
        })
    }

    // Como en el sistema en C, la baja también quita el ingrediente de las recetas.
    function eliminarIngrediente(idIngrediente, cb) {
        _pedir("OP_ELIMINAR_INGREDIENTE", { id_ingrediente: idIngrediente }, cb, function (d) {
            const n = d.recetas
            return { datos: d, mensaje: "Se eliminó «" + d.nombre + "»"
                                        + (n > 0 ? " y se quitó de " + n + (n === 1 ? " receta" : " recetas") : "") }
        })
    }

    function reiniciarStock(cb) {
        _pedir("OP_REINICIAR_STOCK", null, cb, function () {
            return { datos: null, mensaje: "Todas las existencias quedaron en 0" }
        })
    }

    // ============================== Caja ==============================

    // Todas las cuentas del evento con su total, lo pagado y el saldo (v_cuenta).
    // Llevan los mismos campos que una ronda para poder agruparlas igual.
    // numeroEvento es opcional: sin él, las del evento en curso. Las de otro
    // evento (uno pasado) solo las puede consultar el administrador.
    function listarCuentasCaja(cb, numeroEvento) {
        _pedir("OP_LISTAR_CUENTAS_CAJA", { numero_evento: numeroEvento || null }, cb, function (lista) {
            for (const c of lista) {
                c.total = Number(c.total)
                c.pagado = Number(c.pagado)
                c.saldo = Number(c.saldo)
            }
            return { datos: lista, mensaje: "" }
        })
    }

    function detalleCuenta(secuencial, cb, numeroEvento) {
        _pedir("OP_DETALLE_CUENTA", { secuencial: secuencial, numero_evento: numeroEvento || null }, cb, function (d) {
            const lineas = []
            // tipo_cobro: normal, paquete (incluida, 0) o anfitrion (la paga el anfitrión).
            // de_otra_cuenta: en la cuenta del anfitrión, de qué cuenta vino el cargo.
            for (const x of d.comandas) {
                const precio = Number(x.precio)
                const origen = x.de_otra_cuenta || ""
                let l = lineas.find(y => y.id_producto === x.id_producto && y.estado === x.estado && y.precio === precio
                                         && y.tipo_cobro === x.tipo_cobro && y.de_otra_cuenta === origen)
                if (!l) {
                    l = { id_producto: x.id_producto, producto: x.producto, estado: x.estado, precio: precio,
                          tipo_cobro: x.tipo_cobro, de_otra_cuenta: origen,
                          // Lo que suma a esta cuenta: no lo que se cargó al anfitrión desde aquí
                          cuenta_aqui: x.tipo_cobro !== "anfitrion" || origen !== "" || d.es_anfitrion,
                          cantidad: 0, ids: [] }
                    lineas.push(l)
                }
                l.cantidad++
                l.ids.push(x.id_comanda)
            }
            for (const p of d.pagos) {
                p.monto = Number(p.monto)
                p.monto_propina = p.monto_propina === null ? null : Number(p.monto_propina)
            }
            return { datos: { secuencial: d.secuencial, es_anfitrion: d.es_anfitrion, lineas: lineas, pagos: d.pagos },
                     mensaje: "" }
        })
    }

    // datos: { monto, forma_pago, monto_propina }. Las reglas son las de la
    // base (ck_pago_monto, ck_pago_propina_limite y tg_pago_saldo), con sus textos.
    function registrarPago(secuencial, datos, cb) {
        const monto = _centavos(datos.monto)
        const propina = _centavos(datos.monto_propina || 0)
        if (isNaN(monto) || monto <= 0) {
            _entregarDespues(cb, _falla("VALIDACION_FALLIDA", "El monto debe ser mayor que 0"))
            return
        }
        if (isNaN(propina) || propina < 0) {
            _entregarDespues(cb, _falla("VALIDACION_FALLIDA", "La propina no puede ser negativa"))
            return
        }
        _pedir("OP_REGISTRAR_PAGO", { secuencial: secuencial, monto: monto / 100, forma_pago: datos.forma_pago,
                                      monto_propina: propina / 100 }, cb, function (d) {
            d.saldo = Number(d.saldo)
            return { datos: d, mensaje: "Pago registrado · saldo " + Tema.dinero(d.saldo) }
        })
    }

    // tg_cuenta_cierre: todas las comandas entregadas o canceladas y saldo 0
    function cerrarCuenta(secuencial, cb) {
        _pedir("OP_CERRAR_CUENTA", { secuencial: secuencial }, cb, function (d) {
            return { datos: null, mensaje: "Se cerró la cuenta " + d.cuenta }
        })
    }

    // Cancela todas sus comandas y la cierra. Con pagos no se puede.
    function cancelarCuenta(secuencial, cb) {
        _pedir("OP_CANCELAR_CUENTA", { secuencial: secuencial }, cb, function (d) {
            return { datos: null, mensaje: "Se canceló la cuenta " + d.cuenta }
        })
    }

    // Bebidas van al bar; alimentos y dulcería a cocina, como tg_comanda_estacion.
    function estacionDe(categoria) {
        return categoria === "bebidas" ? "bar" : "cocina"
    }

    // ============================ Usuarios ============================

    // motivo_sin_baja explica por qué no se puede eliminar; vacío si sí se puede.
    function listarUsuarios(cb) {
        _pedir("OP_LISTAR_USUARIOS", null, cb)
    }

    // datos: { id_usuario (si se edita), nombre, usuario, fecha_nacimiento,
    //          contrasena (vacía al editar = no cambia), rol,
    //          turno_asignado | estacion_asignada | nivel_acceso y caja_asignada }
    function guardarUsuario(datos, cb) {
        const enviar = {
            nombre: String(datos.nombre || ""), usuario: String(datos.usuario || ""),
            fecha_nacimiento: String(datos.fecha_nacimiento || ""), contrasena: String(datos.contrasena || ""),
            rol: datos.rol, turno_asignado: datos.turno_asignado, estacion_asignada: datos.estacion_asignada,
            nivel_acceso: datos.nivel_acceso, caja_asignada: Number(datos.caja_asignada)
        }
        if (datos.id_usuario)
            enviar.id_usuario = datos.id_usuario
        const op = datos.id_usuario ? "OP_MODIFICAR_USUARIO" : "OP_CREAR_USUARIO"
        _pedir(op, enviar, cb, function (u) {
            if (api.usuarioActual && u.id_usuario === api.usuarioActual.id_usuario)
                api.usuarioActual = u
            return { datos: u, mensaje: datos.id_usuario ? "Usuario actualizado" : "Se creó el usuario " + u.usuario }
        })
    }

    function eliminarUsuario(idUsuario, cb) {
        _pedir("OP_ELIMINAR_USUARIO", { id_usuario: idUsuario }, cb, function (d) {
            return { datos: null, mensaje: "Se eliminó a " + d.nombre }
        })
    }

    // ============================ Productos ============================
    // La lista es listarProductos: cada producto ya trae su receta con
    // cantidades y motivo_sin_baja ('' si se puede eliminar).

    // datos: { id_producto (si se edita), nombre, descripcion, categoria, precio,
    //          id_producto_principal (null si no es variante),
    //          receta: [{ id_ingrediente, cantidad }] }
    function guardarProducto(datos, cb) {
        const op = datos.id_producto ? "OP_MODIFICAR_PRODUCTO" : "OP_CREAR_PRODUCTO"
        const enviar = {
            nombre: String(datos.nombre || ""), descripcion: String(datos.descripcion || ""),
            categoria: datos.categoria, precio: _centavos(datos.precio) / 100,
            id_producto_principal: datos.id_producto_principal || null,
            receta: (datos.receta || []).map(r => ({ id_ingrediente: r.id_ingrediente, cantidad: Number(r.cantidad) }))
        }
        if (datos.id_producto)
            enviar.id_producto = datos.id_producto
        _pedir(op, enviar, cb, function (d) {
            return { datos: d, mensaje: datos.id_producto ? "Producto actualizado" : "Se agregó «" + d.nombre + "»" }
        })
    }

    function eliminarProducto(idProducto, cb) {
        _pedir("OP_ELIMINAR_PRODUCTO", { id_producto: idProducto }, cb, function (d) {
            return { datos: null, mensaje: "Se eliminó «" + d.nombre + "»" }
        })
    }

    // ============================ Paquetes ============================
    // La base guarda título y descripción juntos en "detalles", como
    // "Título: descripción"; aquí se separan y se vuelven a juntar.

    function separarDetalles(detalles) {
        const texto = String(detalles || "")
        const i = texto.indexOf(": ")
        return i < 0 ? { titulo: texto, descripcion: "" }
                     : { titulo: texto.slice(0, i), descripcion: texto.slice(i + 2) }
    }

    function juntarDetalles(titulo, descripcion) {
        const t = String(titulo || "").trim()
        const d = String(descripcion || "").trim()
        return d === "" ? t : t + ": " + d
    }

    // [{ numero_paquete, titulo, descripcion, monto, productos: [{ id_producto, nombre, categoria }], eventos }]
    function listarPaquetes(cb) {
        _pedir("OP_LISTAR_PAQUETES", null, cb, function (lista) {
            for (const p of lista) {
                const partes = api.separarDetalles(p.detalles)
                p.titulo = partes.titulo
                p.descripcion = partes.descripcion
                p.monto = Number(p.monto)
            }
            return { datos: lista, mensaje: "" }
        })
    }

    // datos: { numero_paquete (si se edita), titulo, descripcion, monto, productos: [ids] }
    function guardarPaquete(datos, cb) {
        const op = datos.numero_paquete ? "OP_MODIFICAR_PAQUETE" : "OP_CREAR_PAQUETE"
        const enviar = { detalles: juntarDetalles(datos.titulo, datos.descripcion),
                         monto: _centavos(datos.monto) / 100, productos: datos.productos || [] }
        if (datos.numero_paquete)
            enviar.numero_paquete = datos.numero_paquete
        _pedir(op, enviar, cb, function (d) {
            return { datos: d, mensaje: datos.numero_paquete ? "Paquete actualizado" : "Se creó el paquete" }
        })
    }

    function eliminarPaquete(numeroPaquete, cb) {
        _pedir("OP_ELIMINAR_PAQUETE", { numero_paquete: numeroPaquete }, cb, function (d) {
            return { datos: null, mensaje: "Se eliminó «" + api.separarDetalles(d.detalles).titulo + "»" }
        })
    }

    // ============================= Eventos =============================

    // desde y hasta ("AAAA-MM-DD") son opcionales. Cada evento trae
    // organizadores, mesas, paquetes (con la cantidad acordada de cada
    // producto), anticipos y sus totales.
    function listarEventos(desde, hasta, cb) {
        _pedir("OP_LISTAR_EVENTOS", { desde: desde || null, hasta: hasta || null }, cb, function (lista) {
            for (const e of lista) {
                e.total_anticipos = Number(e.total_anticipos)
                e.total_paquetes = Number(e.total_paquetes)
                for (const a of e.anticipos)
                    a.monto = Number(a.monto)
                for (const p of e.paquetes) {
                    p.monto = Number(p.monto)
                    const partes = api.separarDetalles(p.detalles)
                    p.titulo = partes.titulo
                    p.descripcion = partes.descripcion
                }
            }
            return { datos: lista, mensaje: "" }
        })
    }

    // datos: { numero_evento (si se edita), fecha, tipo_evento, numero_asistentes,
    //          organizadores: [...], mesas: [{ numero_mesa, id_mesero }],
    //          paquetes: [{ numero_paquete, productos: [{ id_producto, cantidad, modo_cobro }] }],
    //          secuencial_anfitrion (solo al editar), repetir (solo al crear): { cada: "dias" | "semanas" | "meses", intervalo, veces } }
    function guardarEvento(datos, cb) {
        const enviar = {
            fecha: String(datos.fecha || "").trim(), tipo_evento: String(datos.tipo_evento || ""),
            numero_asistentes: Number(datos.numero_asistentes),
            organizadores: datos.organizadores || [],
            mesas: (datos.mesas || []).map(m => ({ numero_mesa: Number(m.numero_mesa), id_mesero: m.id_mesero })),
            paquetes: (datos.paquetes || []).map(p => ({
                numero_paquete: p.numero_paquete,
                productos: p.productos.map(x => ({ id_producto: x.id_producto, cantidad: Number(x.cantidad),
                                                   modo_cobro: x.modo_cobro || "cobrar" }))
            }))
        }
        if (datos.numero_evento)
            enviar.secuencial_anfitrion = datos.secuencial_anfitrion || null
        if (datos.numero_evento) {
            enviar.numero_evento = datos.numero_evento
            _pedir("OP_MODIFICAR_EVENTO", enviar, cb, function () {
                return { datos: null, mensaje: "Evento actualizado" }
            })
            return
        }
        if (datos.repetir)
            enviar.repetir = datos.repetir
        _pedir("OP_CREAR_EVENTO", enviar, cb, function (d) {
            return { datos: d, mensaje: d.creados === 1 ? "Se creó el evento" : "Se crearon " + d.creados + " eventos" }
        })
    }

    // estado: "en_curso" (iniciar), "finalizado" o "cancelado"
    function cambiarEstadoEvento(numeroEvento, estado, cb) {
        const textos = { en_curso: "Evento iniciado", finalizado: "Evento finalizado", cancelado: "Evento cancelado" }
        _pedir("OP_CAMBIAR_ESTADO_EVENTO", { numero_evento: numeroEvento, estado: estado }, cb, function () {
            return { datos: null, mensaje: textos[estado] || "" }
        })
    }

    // fecha opcional ("AAAA-MM-DD"); si no va, es hoy
    function registrarAnticipo(numeroEvento, monto, fecha, cb) {
        const centavos = _centavos(monto)
        if (isNaN(centavos) || centavos <= 0) {
            _entregarDespues(cb, _falla("VALIDACION_FALLIDA", "El monto del anticipo debe ser mayor que 0"))
            return
        }
        _pedir("OP_REGISTRAR_ANTICIPO", { numero_evento: numeroEvento, monto: centavos / 100, fecha: fecha || null }, cb,
               function () {
                   return { datos: null, mensaje: "Anticipo registrado · " + Tema.dinero(centavos / 100) }
               })
    }

    // ============================ Bitácora ============================

    // Lo que ha pasado en el servidor, de lo más reciente a lo más viejo:
    // [{ fecha: "2026-11-24 09:43:28", texto }]
    function listarBitacora(cb) {
        _pedir("OP_LISTAR_LOGS", null, cb)
    }

    // ========================== Uso interno ==========================

    // Junta las comandas iguales (producto, nota y estado) para mostrar "2× Cerveza".
    function _agruparLineas(comandas) {
        const lineas = []
        for (const c of comandas) {
            let l = lineas.find(x => x.id_producto === c.id_producto && x.detalles === c.detalles && x.estado === c.estado)
            if (!l) {
                l = { id_producto: c.id_producto, producto: c.producto, detalles: c.detalles,
                      estado: c.estado, estacion: c.estacion, cantidad: 0, ids: [] }
                lineas.push(l)
            }
            l.cantidad++
            l.ids.push(c.id_comanda)
        }
        return lineas
    }

    function _numerosIngrediente(i) {
        i.stock = Number(i.stock)
        i.stock_minimo = Number(i.stock_minimo)
        return i
    }

    // El dinero se manda en centavos redondeados para no arrastrar errores de float
    function _centavos(valor) { return Math.round(Number(String(valor).replace(",", ".")) * 100) }

    function _ok(datos, mensaje) {
        return { codigo: "OK", mensaje: mensaje || "", datos: datos === undefined ? null : datos }
    }

    function _falla(codigo, mensaje) {
        return { codigo: codigo, mensaje: mensaje, datos: null }
    }

    function _entregar(cb, respuesta) {
        if (cb)
            cb(respuesta)
    }

    // Para lo que se rechaza aquí mismo, sin ir al servidor: el callback llega
    // después, cuando la pantalla ya terminó de pedir, igual que una respuesta.
    function _entregarDespues(cb, respuesta) {
        const temporizador = Qt.createQmlObject("import QtQml; Timer { interval: 0; repeat: false }", api, "RespuestaLocal")
        temporizador.triggered.connect(function () {
            temporizador.destroy()
            api._entregar(cb, respuesta)
        })
        temporizador.start()
    }
}
