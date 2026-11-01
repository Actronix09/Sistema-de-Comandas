pragma Singleton
import QtQuick

// ---------------------------------------------------------------------------
//  Api (simulada)
//
//  Es la única puerta entre las pantallas y los datos. Las pantallas nunca
//  tocan los arreglos de abajo: piden y reciben copias, como si llegaran por
//  la red. En el sistema real este archivo se sustituye por un cliente HTTP
//  con los mismos métodos, y las pantallas no cambian.
//
//  Cada método recibe al final un callback que se llama ~250 ms después con
//      { codigo:  "OK" | "CREDENCIALES_INVALIDAS" | "VALIDACION_FALLIDA" |
//                 "SIN_STOCK" | "NO_AUTORIZADO" | "ERROR",
//        mensaje: texto para mostrar,
//        datos:   lo pedido, o null }
//  Es la misma idea que el par Peticion/Respuesta de protocolo.h.
//
//  Operación del servidor que sustituirá a cada método (contrato de la Fase 4
//  de PENDIENTES.md; las marcadas "nueva" no existían en protocolo.h):
//      iniciarSesion        OP_LOGIN
//      eventoEnCurso        nueva, por definir
//      listarProductos      OP_LISTAR_PRODUCTOS
//      listarMesas          OP_LISTAR_MESAS (nueva)
//      listarCuentas        OP_LISTAR_CUENTAS (nueva)
//      crearCuenta          OP_CREAR_CUENTA (nueva)
//      crearComandas        OP_CREAR_PEDIDO
//      listarRondas         OP_LISTAR_PEDIDOS
//      cambiarEstado        OP_CAMBIAR_ESTADO_PEDIDO
//      listarIngredientes   OP_LISTAR_INGREDIENTES
//      guardarIngrediente   OP_CREAR_INGREDIENTE   o   OP_MODIFICAR_INGREDIENTE
//      ajustarStock         OP_MODIFICAR_INGREDIENTE
//      eliminarIngrediente  OP_ELIMINAR_INGREDIENTE
//      reiniciarStock       nueva, por definir
//  Las señales (comandasCambiaron, cuentasCambiaron, inventarioCambio) serán
//  los avisos que el servidor manda por el WebSocket.
// ---------------------------------------------------------------------------
QtObject {
    id: api

    signal comandasCambiaron()
    signal cuentasCambiaron()
    signal inventarioCambio()

    // Usuario con sesión iniciada (null si no hay sesión)
    property var usuarioActual: null

    readonly property int retardoMs: 250
    readonly property string contrasenaDemo: "1234"

    // =========================== Datos simulados ===========================
    // Lo marcado con (ejemplo) viene de sql/datos-ejemplo.sql de tu repo de BD;
    // lo demás es inventado para que las pantallas se vean llenas.

    // (ejemplo) evento 17
    property var _evento: ({
        numero_evento: 17, fecha: "2026-10-17", tipo_evento: "Boda",
        numero_asistentes: 180, estado: "en_curso",
        organizadores: ["Ana Torres", "Luis Herrera"]
    })

    // (ejemplo) supertipo usuario + el dato de su subtipo
    property var _usuarios: [
        { id_usuario: 1, nombre: "Laura Méndez Ruiz", usuario: "lmendez", rol: "mesero", turno_asignado: "vespertino" },
        { id_usuario: 2, nombre: "Óscar Pérez Lara",  usuario: "operez",  rol: "mesero", turno_asignado: "nocturno" },
        { id_usuario: 3, nombre: "Rosa Gámez Torres", usuario: "rgamez",  rol: "bar_cocina", estacion_asignada: "cocina" },
        { id_usuario: 4, nombre: "Iván Soto Cruz",    usuario: "isoto",   rol: "bar_cocina", estacion_asignada: "bar" },
        { id_usuario: 5, nombre: "Marta Rivas Peña",  usuario: "mrivas",  rol: "cajero_admin", nivel_acceso: "administrador", caja_asignada: 1 }
    ]

    // Mesas 1 a 3 (ejemplo) y 4 a 6 inventadas, con el mesero que las atiende
    property var _mesas: [
        { numero_mesa: 1, id_mesero: 1 }, { numero_mesa: 2, id_mesero: 1 },
        { numero_mesa: 3, id_mesero: 2 }, { numero_mesa: 4, id_mesero: 2 },
        { numero_mesa: 5, id_mesero: 1 }, { numero_mesa: 6, id_mesero: 2 }
    ]

    // (ejemplo) cuentas del evento 17; el folio se calcula: evento-mesa-secuencial.
    // El nombre guía al mesero; en pantalla va primero y el folio al final.
    property var _cuentas: [
        { secuencial: 1, numero_mesa: 1, nombre: "Novios", estado: "abierta" },
        { secuencial: 2, numero_mesa: 1, nombre: "Padrinos", estado: "abierta" },
        { secuencial: 3, numero_mesa: 3, nombre: "Familia Torres", estado: "abierta" }
    ]

    // Inventado: tu esquema aún no tiene existencias; esto es lo que el
    // menú "Administrador de ingredientes" necesita.
    property var _ingredientes: [
        { id_ingrediente: 1,  nombre: "Pasta (porción)", stock: 40 },
        { id_ingrediente: 2,  nombre: "Crema", stock: 30 },
        { id_ingrediente: 3,  nombre: "Queso parmesano", stock: 25 },
        { id_ingrediente: 4,  nombre: "Harina", stock: 20 },
        { id_ingrediente: 5,  nombre: "Cacao", stock: 15 },
        { id_ingrediente: 6,  nombre: "Refresco de cola (lata)", stock: 120 },
        { id_ingrediente: 7,  nombre: "Refresco sin azúcar (lata)", stock: 0 },
        { id_ingrediente: 8,  nombre: "Agua mineral (botella)", stock: 60 },
        { id_ingrediente: 9,  nombre: "Cerveza (botella)", stock: 96 },
        { id_ingrediente: 10, nombre: "Tequila (onza)", stock: 80 },
        { id_ingrediente: 11, nombre: "Limón", stock: 200 },
        { id_ingrediente: 12, nombre: "Lechuga romana", stock: 18 },
        { id_ingrediente: 13, nombre: "Pollo (porción)", stock: 30 },
        { id_ingrediente: 14, nombre: "Arrachera (porción)", stock: 6 },
        { id_ingrediente: 15, nombre: "Café molido (porción)", stock: 100 },
        { id_ingrediente: 16, nombre: "Leche", stock: 40 },
        { id_ingrediente: 17, nombre: "Huevo", stock: 60 },
        { id_ingrediente: 18, nombre: "Azúcar", stock: 50 },
        { id_ingrediente: 19, nombre: "Queso crema", stock: 8 }
    ]

    // Productos 1 a 4 (ejemplo) más inventados. Llevan la descripción que vas a
    // agregar al esquema y una receta con cantidades (pendiente en tu esquema).
    property var _productos: [
        { id_producto: 1, nombre: "Refresco", categoria: "bebidas", precio: 35.00, id_producto_principal: null,
          descripcion: "Refresco de cola de 355 ml, servido frío.", receta: [{ id_ingrediente: 6, cantidad: 1 }] },
        { id_producto: 2, nombre: "Refresco de dieta", categoria: "bebidas", precio: 35.00, id_producto_principal: 1,
          descripcion: "La versión sin azúcar del refresco de cola.", receta: [{ id_ingrediente: 7, cantidad: 1 }] },
        { id_producto: 3, nombre: "Pasta Alfredo", categoria: "alimentos", precio: 180.00, id_producto_principal: null,
          descripcion: "Fettuccine en salsa cremosa de queso parmesano.",
          receta: [{ id_ingrediente: 1, cantidad: 1 }, { id_ingrediente: 2, cantidad: 1 }, { id_ingrediente: 3, cantidad: 1 }] },
        { id_producto: 4, nombre: "Pastel de chocolate", categoria: "dulceria", precio: 95.00, id_producto_principal: null,
          descripcion: "Rebanada de pastel húmedo de chocolate.",
          receta: [{ id_ingrediente: 4, cantidad: 1 }, { id_ingrediente: 5, cantidad: 1 }, { id_ingrediente: 17, cantidad: 1 }] },
        { id_producto: 5, nombre: "Agua mineral", categoria: "bebidas", precio: 30.00, id_producto_principal: null,
          descripcion: "Botella de 600 ml.", receta: [{ id_ingrediente: 8, cantidad: 1 }] },
        { id_producto: 6, nombre: "Cerveza", categoria: "bebidas", precio: 60.00, id_producto_principal: null,
          descripcion: "Cerveza clara de 355 ml.", receta: [{ id_ingrediente: 9, cantidad: 1 }] },
        { id_producto: 7, nombre: "Margarita", categoria: "bebidas", precio: 110.00, id_producto_principal: null,
          descripcion: "Tequila, jugo de limón y escarchado de sal.",
          receta: [{ id_ingrediente: 10, cantidad: 2 }, { id_ingrediente: 11, cantidad: 2 }] },
        { id_producto: 8, nombre: "Café americano", categoria: "bebidas", precio: 40.00, id_producto_principal: null,
          descripcion: "Café de grano recién preparado.", receta: [{ id_ingrediente: 15, cantidad: 1 }] },
        { id_producto: 9, nombre: "Café con leche", categoria: "bebidas", precio: 45.00, id_producto_principal: 8,
          descripcion: "Café americano con leche caliente.",
          receta: [{ id_ingrediente: 15, cantidad: 1 }, { id_ingrediente: 16, cantidad: 1 }] },
        { id_producto: 10, nombre: "Ensalada César", categoria: "alimentos", precio: 140.00, id_producto_principal: null,
          descripcion: "Lechuga romana, pollo a la parrilla, crutones y aderezo César.",
          receta: [{ id_ingrediente: 12, cantidad: 1 }, { id_ingrediente: 13, cantidad: 1 }, { id_ingrediente: 3, cantidad: 1 }] },
        { id_producto: 11, nombre: "Arrachera", categoria: "alimentos", precio: 260.00, id_producto_principal: null,
          descripcion: "250 g de arrachera a la parrilla con guarnición.", receta: [{ id_ingrediente: 14, cantidad: 1 }] },
        { id_producto: 12, nombre: "Pasta Alfredo con pollo", categoria: "alimentos", precio: 210.00, id_producto_principal: 3,
          descripcion: "La pasta Alfredo de la casa con pollo a la parrilla.",
          receta: [{ id_ingrediente: 1, cantidad: 1 }, { id_ingrediente: 2, cantidad: 1 }, { id_ingrediente: 3, cantidad: 1 }, { id_ingrediente: 13, cantidad: 1 }] },
        { id_producto: 13, nombre: "Flan napolitano", categoria: "dulceria", precio: 70.00, id_producto_principal: null,
          descripcion: "Flan casero con caramelo.",
          receta: [{ id_ingrediente: 16, cantidad: 1 }, { id_ingrediente: 17, cantidad: 2 }, { id_ingrediente: 18, cantidad: 1 }] },
        { id_producto: 14, nombre: "Pay de queso", categoria: "dulceria", precio: 80.00, id_producto_principal: null,
          descripcion: "Rebanada de pay de queso con base de galleta.",
          receta: [{ id_ingrediente: 19, cantidad: 1 }, { id_ingrediente: 17, cantidad: 1 }, { id_ingrediente: 4, cantidad: 1 }] }
    ]

    // Una fila por unidad pedida, como la tabla comanda (se llena al arrancar)
    property var _comandas: []
    property int _siguienteComanda: 1

    // Transiciones permitidas y quién puede hacerlas. En el sistema en C no se
    // validaban (2 → 0 → 2 descontaba el stock dos veces); aquí sí.
    readonly property var _transiciones: ({
        pendiente: ["en_preparacion", "lista", "cancelada"],
        en_preparacion: ["lista", "cancelada"],
        lista: ["entregada"],
        entregada: [],
        cancelada: []
    })
    readonly property var _rolesPorEstado: ({
        en_preparacion: ["bar_cocina"],
        lista: ["bar_cocina"],
        entregada: ["mesero"],
        cancelada: ["cajero_admin"]
    })

    Component.onCompleted: _cargarComandasIniciales()

    // ============================== Sesión ==============================

    function iniciarSesion(usuario, contrasena, cb) {
        const nombre = String(usuario || "").trim().toLowerCase()
        const u = _usuarios.find(x => x.usuario === nombre)
        if (!u || contrasena !== contrasenaDemo) {
            _responder(cb, _falla("CREDENCIALES_INVALIDAS", "Usuario o contraseña incorrectos"))
            return
        }
        usuarioActual = _copia(u)
        _responder(cb, _ok(u, "Bienvenido, " + u.nombre))
    }

    function cerrarSesion() {
        usuarioActual = null
    }

    function eventoEnCurso(cb) {
        _responder(cb, _ok(_evento))
    }

    // ============================ Catálogo ============================

    // Cada producto trae sus ingredientes y cuántas unidades alcanzan con el
    // stock actual (maximo). La revisión definitiva la hace crearComandas.
    function listarProductos(cb) {
        const lista = _productos.map(function (p) {
            const principal = p.id_producto_principal ? _producto(p.id_producto_principal) : null
            const maximo = _maximoPosible(p)
            return {
                id_producto: p.id_producto,
                nombre: p.nombre,
                descripcion: p.descripcion,
                categoria: p.categoria,
                precio: p.precio,
                id_producto_principal: p.id_producto_principal,
                variante_de: principal ? principal.nombre : "",
                ingredientes: p.receta.map(r => _nombreIngrediente(r.id_ingrediente)),
                maximo: maximo,
                disponible: maximo > 0
            }
        })
        _responder(cb, _ok(lista))
    }

    // ========================== Mesas y cuentas ==========================

    // Las mesas del evento en curso; las del mesero con sesión van primero.
    function listarMesas(cb) {
        const yo = usuarioActual ? usuarioActual.id_usuario : -1
        const lista = _mesas.map(m => ({
            numero_mesa: m.numero_mesa,
            id_mesero: m.id_mesero,
            mesero: _usuario(m.id_mesero).nombre,
            mia: m.id_mesero === yo
        }))
        lista.sort((a, b) => (Number(b.mia) - Number(a.mia)) || (a.numero_mesa - b.numero_mesa))
        _responder(cb, _ok(lista))
    }

    function listarCuentas(numeroMesa, cb) {
        const lista = _cuentas
            .filter(c => c.numero_mesa === numeroMesa && c.estado === "abierta")
            .map(c => ({ secuencial: c.secuencial, numero_mesa: c.numero_mesa, nombre: c.nombre,
                         folio: _folio(c), cuenta: _nombreCuenta(c), total: _totalCuenta(c.secuencial) }))
        _responder(cb, _ok(lista))
    }

    // Abre una cuenta con nombre en una mesa. Como ux_cuenta_nombre_abierta,
    // no permite dos cuentas abiertas con el mismo nombre en la misma mesa.
    function crearCuenta(numeroMesa, nombre, cb) {
        const negado = _exigirRol(["mesero", "cajero_admin"])
        if (negado) {
            _responder(cb, negado)
            return
        }
        const r = _abrirCuenta(numeroMesa, nombre)
        if (r.codigo === "OK")
            cuentasCambiaron()
        _responder(cb, r)
    }

    // ============================ Comandas ============================

    // lineas: [{ id_producto, cantidad, detalles }]
    // secuencial: una cuenta abierta de esa mesa (las nuevas se abren con crearCuenta).
    function crearComandas(numeroMesa, secuencial, lineas, cb) {
        const negado = _exigirRol(["mesero"])
        if (negado) {
            _responder(cb, negado)
            return
        }
        _responder(cb, _insertarComandas(numeroMesa, secuencial, lineas, usuarioActual.id_usuario))
    }

    // Comandas agrupadas en rondas: las que una cuenta pidió en el mismo envío.
    // filtro: { estacion: "cocina" | "bar" | "todas", id_mesero, estados: [...] }
    function listarRondas(filtro, cb) {
        const f = filtro || {}
        const grupos = {}
        const claves = []
        for (const c of _comandas) {
            const p = _producto(c.id_producto)
            const estacion = estacionDe(p.categoria)
            if (f.estacion && f.estacion !== "todas" && estacion !== f.estacion)
                continue
            if (f.id_mesero && c.id_mesero !== f.id_mesero)
                continue
            if (f.estados && f.estados.indexOf(c.estado) < 0)
                continue
            const clave = c.secuencial + "|" + c.fecha_hora + "|" + c.id_mesero
            if (!grupos[clave]) {
                const cuenta = _cuenta(c.secuencial)
                grupos[clave] = {
                    clave: clave, fecha_hora: c.fecha_hora,
                    secuencial: c.secuencial, numero_mesa: cuenta.numero_mesa, folio: _folio(cuenta),
                    nombre_cuenta: cuenta.nombre, cuenta: _nombreCuenta(cuenta),
                    id_mesero: c.id_mesero, mesero: _usuario(c.id_mesero).nombre,
                    comandas: []
                }
                claves.push(clave)
            }
            grupos[clave].comandas.push({
                id_comanda: c.id_comanda, id_producto: c.id_producto, producto: p.nombre,
                categoria: p.categoria, estacion: estacion, estado: c.estado,
                detalles: c.detalles, precio_cobrado: c.precio_cobrado
            })
        }
        const rondas = claves.map(k => grupos[k])
        for (const r of rondas) {
            r.lineas = _agruparLineas(r.comandas)
            r.total = r.comandas.filter(c => c.estado !== "cancelada").reduce((s, c) => s + c.precio_cobrado, 0)
        }
        rondas.sort((a, b) => a.fecha_hora - b.fecha_hora)
        _responder(cb, _ok(rondas))
    }

    function cambiarEstado(ids, nuevoEstado, cb) {
        const roles = _rolesPorEstado[nuevoEstado]
        if (!roles) {
            _responder(cb, _falla("VALIDACION_FALLIDA", "Estado desconocido: " + nuevoEstado))
            return
        }
        const negado = _exigirRol(roles)
        if (negado) {
            _responder(cb, negado)
            return
        }
        const elegidas = _comandas.filter(c => ids.indexOf(c.id_comanda) >= 0)
        if (elegidas.length === 0) {
            _responder(cb, _falla("VALIDACION_FALLIDA", "No se indicó ninguna comanda"))
            return
        }
        for (const c of elegidas) {
            if (_transiciones[c.estado].indexOf(nuevoEstado) < 0) {
                _responder(cb, _falla("VALIDACION_FALLIDA", "No se puede pasar de «" + Tema.nombreEstado(c.estado)
                                      + "» a «" + Tema.nombreEstado(nuevoEstado) + "»"))
                return
            }
        }
        for (const c of elegidas) {
            c.estado = nuevoEstado
            // ck_comanda_preparacion: fuera de pendiente/cancelada debe haber estación
            if ((nuevoEstado === "en_preparacion" || nuevoEstado === "lista") && c.id_bar_cocina === null)
                c.id_bar_cocina = usuarioActual.id_usuario
            // ck_comanda_cancelacion: cancelada si y solo si la canceló un cajero/admin
            if (nuevoEstado === "cancelada")
                c.id_cajero_cancela = usuarioActual.id_usuario
        }
        comandasCambiaron()
        const n = elegidas.length
        _responder(cb, _ok({ actualizadas: n }, n + (n === 1 ? " comanda" : " comandas") + " → " + Tema.nombreEstado(nuevoEstado)))
    }

    // Imita a otro mesero enviando una orden desde otra computadora; en el
    // sistema real esto llega por el WebSocket. Devuelve los datos de la ronda.
    function simularRondaEntrante() {
        const mesa = _mesas[Math.floor(Math.random() * _mesas.length)]
        const abiertas = _cuentas.filter(c => c.numero_mesa === mesa.numero_mesa && c.estado === "abierta")
        const cuenta = abiertas.length > 0 && Math.random() < 0.7
                ? abiertas[Math.floor(Math.random() * abiertas.length)] : null
        const disponibles = _productos.filter(p => _maximoPosible(p) > 0)
        if (disponibles.length === 0)
            return null
        const notas = ["", "", "", "para compartir", "sin cebolla", "sin hielo", "bien cocido"]
        const lineas = []
        const cuantos = 1 + Math.floor(Math.random() * 3)
        for (let i = 0; i < cuantos; i++) {
            const p = disponibles[Math.floor(Math.random() * disponibles.length)]
            if (lineas.some(l => l.id_producto === p.id_producto))
                continue
            lineas.push({ id_producto: p.id_producto, cantidad: 1 + Math.floor(Math.random() * 2),
                          detalles: notas[Math.floor(Math.random() * notas.length)] })
        }
        let secuencial = cuenta ? cuenta.secuencial : null
        if (!secuencial) {
            const nueva = _abrirCuenta(mesa.numero_mesa, "Invitados " + (_cuentas.length + 1))
            if (nueva.codigo !== "OK")
                return null
            secuencial = nueva.datos.secuencial
        }
        const r = _insertarComandas(mesa.numero_mesa, secuencial, lineas, mesa.id_mesero)
        return r.codigo === "OK" ? r.datos : null
    }

    // ========================== Ingredientes ==========================

    function listarIngredientes(cb) {
        const lista = _ingredientes.map(i => ({
            id_ingrediente: i.id_ingrediente,
            nombre: i.nombre,
            stock: i.stock,
            usos: _productos.filter(p => p.receta.some(r => r.id_ingrediente === i.id_ingrediente)).length
        }))
        lista.sort((a, b) => a.nombre.localeCompare(b.nombre, "es"))
        _responder(cb, _ok(lista))
    }

    // datos: { id_ingrediente (si se edita), nombre, stock }
    function guardarIngrediente(datos, cb) {
        const negado = _exigirRol(["cajero_admin"])
        if (negado) {
            _responder(cb, negado)
            return
        }
        const nombre = String(datos.nombre || "").trim()
        const stock = Number(datos.stock)
        if (nombre.length === 0) {
            _responder(cb, _falla("VALIDACION_FALLIDA", "El nombre no puede quedar vacío"))
            return
        }
        if (nombre.length > 60) {
            _responder(cb, _falla("VALIDACION_FALLIDA", "El nombre admite hasta 60 caracteres"))
            return
        }
        if (!Number.isInteger(stock) || stock < 0) {
            _responder(cb, _falla("VALIDACION_FALLIDA", "La existencia debe ser un número entero mayor o igual a 0"))
            return
        }
        const repetido = _ingredientes.find(i => i.nombre.toLowerCase() === nombre.toLowerCase()
                                                 && i.id_ingrediente !== datos.id_ingrediente)
        if (repetido) {
            _responder(cb, _falla("VALIDACION_FALLIDA", "Ya existe un ingrediente llamado «" + repetido.nombre + "»"))
            return
        }
        let ingrediente
        if (datos.id_ingrediente) {
            ingrediente = _ingrediente(datos.id_ingrediente)
            if (!ingrediente) {
                _responder(cb, _falla("ERROR", "El ingrediente ya no existe"))
                return
            }
            ingrediente.nombre = nombre
            ingrediente.stock = stock
        } else {
            ingrediente = { id_ingrediente: _ingredientes.reduce((m, i) => Math.max(m, i.id_ingrediente), 0) + 1,
                            nombre: nombre, stock: stock }
            _ingredientes.push(ingrediente)
        }
        inventarioCambio()
        _responder(cb, _ok(ingrediente, datos.id_ingrediente ? "Ingrediente actualizado" : "Ingrediente agregado"))
    }

    function ajustarStock(idIngrediente, delta, cb) {
        const negado = _exigirRol(["cajero_admin"])
        if (negado) {
            _responder(cb, negado)
            return
        }
        const ingrediente = _ingrediente(idIngrediente)
        if (!ingrediente) {
            _responder(cb, _falla("ERROR", "El ingrediente ya no existe"))
            return
        }
        ingrediente.stock = Math.max(0, ingrediente.stock + delta)
        inventarioCambio()
        _responder(cb, _ok(ingrediente))
    }

    // Como en el sistema en C, la baja también quita el ingrediente de las recetas.
    function eliminarIngrediente(idIngrediente, cb) {
        const negado = _exigirRol(["cajero_admin"])
        if (negado) {
            _responder(cb, negado)
            return
        }
        const ingrediente = _ingrediente(idIngrediente)
        if (!ingrediente) {
            _responder(cb, _falla("ERROR", "El ingrediente ya no existe"))
            return
        }
        let recetas = 0
        for (const p of _productos) {
            const antes = p.receta.length
            p.receta = p.receta.filter(r => r.id_ingrediente !== idIngrediente)
            if (p.receta.length !== antes)
                recetas++
        }
        _ingredientes.splice(_ingredientes.indexOf(ingrediente), 1)
        inventarioCambio()
        _responder(cb, _ok({ recetas: recetas }, "Se eliminó «" + ingrediente.nombre + "»"
                           + (recetas > 0 ? " y se quitó de " + recetas + (recetas === 1 ? " receta" : " recetas") : "")))
    }

    function reiniciarStock(cb) {
        const negado = _exigirRol(["cajero_admin"])
        if (negado) {
            _responder(cb, negado)
            return
        }
        for (const i of _ingredientes)
            i.stock = 0
        inventarioCambio()
        _responder(cb, _ok(null, "Todas las existencias quedaron en 0"))
    }

    // Bebidas van al bar; alimentos y dulcería a cocina. Es un supuesto: en tu
    // esquema falta decidir de dónde sale la estación de cada producto.
    function estacionDe(categoria) {
        return categoria === "bebidas" ? "bar" : "cocina"
    }

    // ========================== Uso interno ==========================

    function _insertarComandas(numeroMesa, secuencial, lineas, idMesero) {
        if (!_mesas.some(m => m.numero_mesa === numeroMesa))
            return _falla("VALIDACION_FALLIDA", "La mesa " + numeroMesa + " no existe en este evento")
        if (!lineas || lineas.length === 0)
            return _falla("VALIDACION_FALLIDA", "La orden está vacía")

        // Todo o nada: primero se suma lo que pide la orden completa, por ingrediente.
        const necesita = {}
        for (const l of lineas) {
            const p = _producto(l.id_producto)
            if (!p)
                return _falla("VALIDACION_FALLIDA", "No existe el producto " + l.id_producto)
            if (!(l.cantidad > 0))
                return _falla("VALIDACION_FALLIDA", "Cantidad inválida para " + p.nombre)
            for (const r of p.receta)
                necesita[r.id_ingrediente] = (necesita[r.id_ingrediente] || 0) + r.cantidad * l.cantidad
        }
        for (const id in necesita) {
            const ingrediente = _ingrediente(Number(id))
            if (!ingrediente || ingrediente.stock < necesita[id])
                return _falla("SIN_STOCK", "No alcanza " + (ingrediente ? ingrediente.nombre : "un ingrediente")
                              + ": se necesitan " + necesita[id] + " y hay " + (ingrediente ? ingrediente.stock : 0))
        }

        const cuenta = secuencial ? _cuenta(secuencial) : null
        if (!cuenta || cuenta.numero_mesa !== numeroMesa)
            return _falla("VALIDACION_FALLIDA", "Elige una cuenta de la mesa " + numeroMesa)
        // Mismo texto que tg_comanda_cuenta_abierta
        if (cuenta.estado !== "abierta")
            return _falla("VALIDACION_FALLIDA", "No se pueden agregar comandas a una cuenta cerrada")

        // El mismo instante para toda la orden: así cocina la ve como una sola
        // ronda. En PostgreSQL, now() vale lo mismo en toda la transacción.
        const instante = Date.now()
        let creadas = 0
        for (const l of lineas) {
            const p = _producto(l.id_producto)
            for (let i = 0; i < l.cantidad; i++) {   // una comanda por unidad
                _comandas.push({
                    id_comanda: _siguienteComanda++, fecha_hora: instante, precio_cobrado: p.precio,
                    estado: "pendiente", detalles: String(l.detalles || "").trim().slice(0, 255),
                    numero_evento: _evento.numero_evento, secuencial: cuenta.secuencial,
                    id_mesero: idMesero, id_producto: p.id_producto,
                    id_bar_cocina: null, id_cajero_cancela: null
                })
                creadas++
            }
        }
        // En la simulación el stock se descuenta al crear la orden. Cuándo
        // descontarlo de verdad es una decisión pendiente de tu esquema.
        for (const id in necesita)
            _ingrediente(Number(id)).stock -= necesita[id]

        comandasCambiaron()
        inventarioCambio()
        return _ok({ creadas: creadas, secuencial: cuenta.secuencial, folio: _folio(cuenta), cuenta: _nombreCuenta(cuenta),
                     numero_mesa: numeroMesa },
                   "Se enviaron " + creadas + (creadas === 1 ? " comanda" : " comandas") + " · " + _nombreCuenta(cuenta))
    }

    function _abrirCuenta(numeroMesa, nombre) {
        const limpio = String(nombre || "").trim()
        if (!_mesas.some(m => m.numero_mesa === numeroMesa))
            return _falla("VALIDACION_FALLIDA", "La mesa " + numeroMesa + " no existe en este evento")
        if (limpio.length === 0)
            return _falla("VALIDACION_FALLIDA", "Escribe un nombre para la cuenta")
        if (limpio.length > 60)
            return _falla("VALIDACION_FALLIDA", "El nombre de la cuenta admite hasta 60 caracteres")
        const repetida = _cuentas.find(c => c.numero_mesa === numeroMesa && c.estado === "abierta"
                                            && c.nombre.toLowerCase() === limpio.toLowerCase())
        if (repetida)
            return _falla("VALIDACION_FALLIDA", "Ya hay una cuenta abierta llamada «" + repetida.nombre
                          + "» en la mesa " + numeroMesa)
        const cuenta = { secuencial: _cuentas.reduce((m, c) => Math.max(m, c.secuencial), 0) + 1,
                         numero_mesa: numeroMesa, nombre: limpio, estado: "abierta" }
        _cuentas.push(cuenta)
        return _ok({ secuencial: cuenta.secuencial, numero_mesa: numeroMesa, nombre: limpio,
                     folio: _folio(cuenta), cuenta: _nombreCuenta(cuenta), total: 0 },
                   "Se abrió la cuenta " + _nombreCuenta(cuenta))
    }

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

    function _cargarComandasIniciales() {
        const ahora = Date.now()
        // [hace minutos, secuencial, id_mesero, id_producto, estado, id_bar_cocina, id_cajero_cancela, detalles]
        // Refresco, Pasta Alfredo, Pastel y Refresco de dieta vienen de datos-ejemplo.sql;
        // el resto llena el tablero.
        const filas = [
            [38, 1, 1, 1,  "entregada",      4,    null, ""],
            [30, 2, 1, 8,  "lista",          4,    null, ""],
            [27, 3, 2, 13, "lista",          3,    null, ""],
            [23, 1, 1, 3,  "en_preparacion", 3,    null, "sin queso"],
            [14, 2, 1, 4,  "pendiente",      null, null, ""],
            [9,  3, 2, 2,  "cancelada",      null, 5,    ""],
            [6,  3, 2, 11, "pendiente",      null, null, "término medio"],
            [6,  3, 2, 6,  "pendiente",      null, null, ""],
            [6,  3, 2, 6,  "pendiente",      null, null, ""],
            [3,  1, 1, 7,  "pendiente",      null, null, "sin sal en el borde"],
            [3,  1, 1, 10, "pendiente",      null, null, ""]
        ]
        for (const f of filas) {
            _comandas.push({
                id_comanda: _siguienteComanda++, fecha_hora: ahora - f[0] * 60000,
                precio_cobrado: _producto(f[3]).precio, estado: f[4], detalles: f[7],
                numero_evento: _evento.numero_evento, secuencial: f[1], id_mesero: f[2], id_producto: f[3],
                id_bar_cocina: f[5], id_cajero_cancela: f[6]
            })
        }
    }

    function _maximoPosible(producto) {
        if (producto.receta.length === 0)
            return 99
        let maximo = Infinity
        for (const r of producto.receta) {
            const ingrediente = _ingrediente(r.id_ingrediente)
            maximo = Math.min(maximo, ingrediente ? Math.floor(ingrediente.stock / r.cantidad) : 0)
        }
        return maximo
    }

    function _totalCuenta(secuencial) {
        return _comandas
            .filter(c => c.secuencial === secuencial && c.estado !== "cancelada")
            .reduce((suma, c) => suma + c.precio_cobrado, 0)
    }

    function _exigirRol(roles) {
        if (!usuarioActual)
            return _falla("NO_AUTORIZADO", "Inicia sesión primero")
        if (roles.indexOf(usuarioActual.rol) < 0)
            return _falla("NO_AUTORIZADO", "Tu rol no puede hacer esta operación")
        return null
    }

    function _folio(cuenta) { return _evento.numero_evento + "-" + cuenta.numero_mesa + "-" + cuenta.secuencial }
    // "Novios · 17-1-1": el nombre guía al mesero y el folio queda al final
    function _nombreCuenta(cuenta) { return cuenta.nombre + " · " + _folio(cuenta) }
    function _producto(id) { return _productos.find(p => p.id_producto === id) || null }
    function _ingrediente(id) { return _ingredientes.find(i => i.id_ingrediente === id) || null }
    function _nombreIngrediente(id) { const i = _ingrediente(id); return i ? i.nombre : "?" }
    function _usuario(id) { return _usuarios.find(u => u.id_usuario === id) || null }
    function _cuenta(secuencial) { return _cuentas.find(c => c.secuencial === secuencial) || null }
    function _copia(valor) { return JSON.parse(JSON.stringify(valor)) }

    function _ok(datos, mensaje) {
        return { codigo: "OK", mensaje: mensaje || "", datos: datos === undefined ? null : datos }
    }

    function _falla(codigo, mensaje) {
        return { codigo: codigo, mensaje: mensaje, datos: null }
    }

    // Responde con retardo y con una copia, como haría la red.
    function _responder(cb, respuesta) {
        const copia = _copia(respuesta)
        const temporizador = Qt.createQmlObject("import QtQml; Timer { repeat: false }", api, "RetardoApi")
        temporizador.interval = retardoMs
        temporizador.triggered.connect(function () {
            temporizador.destroy()
            if (cb)
                cb(copia)
        })
        temporizador.start()
    }
}
