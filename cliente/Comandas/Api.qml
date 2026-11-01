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
//      listarCuentasCaja    OP_LISTAR_CUENTAS (nueva)
//      detalleCuenta        nueva, por definir
//      registrarPago        OP_REGISTRAR_PAGO (nueva)
//      cerrarCuenta         OP_CERRAR_CUENTA (nueva)
//      cancelarCuenta       OP_CANCELAR_CUENTA (nueva)
//      cancelarComandas     OP_ELIMINAR_PEDIDO (ahora cancela, no borra)
//      listarUsuarios       OP_LISTAR_USUARIOS
//      guardarUsuario       OP_CREAR_USUARIO   o   OP_MODIFICAR_USUARIO
//      eliminarUsuario      OP_ELIMINAR_USUARIO
//  Las señales (comandasCambiaron, cuentasCambiaron, inventarioCambio,
//  usuariosCambiaron y ordenLista) serán los avisos que el servidor manda por el WebSocket;
//  ordenLista es COMANDA_LISTA, que solo le llega al mesero de esa orden.
// ---------------------------------------------------------------------------
QtObject {
    id: api

    signal comandasCambiaron()
    signal cuentasCambiaron()
    signal inventarioCambio()
    signal usuariosCambiaron()
    // datos: { id_mesero, numero_mesa, cuenta, productos: ["1× Margarita", ...] }
    signal ordenLista(var datos)

    // Mientras un mesero tiene la sesión abierta, una cocina simulada termina
    // sus órdenes de vez en cuando para que se vea el aviso. En el sistema real
    // esto lo hace cocina desde su propia pantalla.
    property bool simularCocina: true
    property Timer _cocinaSimulada: Timer {
        interval: 30000
        repeat: true
        running: api.simularCocina && api.usuarioActual !== null && api.usuarioActual.rol === "mesero"
        onTriggered: api._terminarOrdenSimulada()
    }

    // Usuario con sesión iniciada (null si no hay sesión)
    property var usuarioActual: null
    // Dentro de cajero_admin, nivel_acceso separa lo que puede cada quien: el
    // cajero solo cobra y cierra cuentas (decisión 4.5 de PENDIENTES.md).
    readonly property bool esAdministrador: usuarioActual !== null && usuarioActual.nivel_acceso === "administrador"

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

    // (ejemplo) supertipo usuario + el dato de su subtipo; la administradora es
    // inventada. Quien no tenga "contrasena" entra con contrasenaDemo. En el
    // sistema real el servidor guarda solo el hash y nunca lo devuelve.
    property var _usuarios: [
        { id_usuario: 1, nombre: "Laura Méndez Ruiz", usuario: "lmendez", fecha_nacimiento: "1998-04-12", rol: "mesero", turno_asignado: "vespertino" },
        { id_usuario: 2, nombre: "Óscar Pérez Lara",  usuario: "operez",  fecha_nacimiento: "2001-09-30", rol: "mesero", turno_asignado: "nocturno" },
        { id_usuario: 3, nombre: "Rosa Gámez Torres", usuario: "rgamez",  fecha_nacimiento: "1990-01-05", rol: "bar_cocina", estacion_asignada: "cocina" },
        { id_usuario: 4, nombre: "Iván Soto Cruz",    usuario: "isoto",   fecha_nacimiento: "1995-07-21", rol: "bar_cocina", estacion_asignada: "bar" },
        { id_usuario: 5, nombre: "Marta Rivas Peña",  usuario: "mrivas",  fecha_nacimiento: "1985-11-02", rol: "cajero_admin", nivel_acceso: "cajero", caja_asignada: 1 },
        { id_usuario: 6, nombre: "Daniela Campos Vega", usuario: "dcampos", fecha_nacimiento: "1982-03-15", rol: "cajero_admin", nivel_acceso: "administrador", caja_asignada: 2 }
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

    // (ejemplo) un pago con propina, como en datos-ejemplo.sql. monto incluye
    // la propina: lo que abona al consumo es monto - monto_propina.
    property var _pagos: [
        { numero_pago: 1, secuencial: 1, monto: 85.00, forma_pago: "efectivo", monto_propina: 50.00,
          id_mesero_propina: 1, id_cajero: 5, fecha_hora: Date.now() - 20 * 60000 }
    ]

    // Transiciones permitidas y quién puede hacerlas. En el sistema en C no se
    // validaban (2 → 0 → 2 descontaba el stock dos veces); aquí sí. Cancelar no
    // está aquí porque solo lo hace un administrador: ver cancelarComandas.
    readonly property var _transiciones: ({
        pendiente: ["en_preparacion", "lista"],
        en_preparacion: ["lista"],
        lista: ["entregada"],
        entregada: [],
        cancelada: []
    })
    readonly property var _rolesPorEstado: ({
        en_preparacion: ["bar_cocina"],
        lista: ["bar_cocina"],
        entregada: ["mesero"]
    })

    Component.onCompleted: _cargarComandasIniciales()

    // ============================== Sesión ==============================

    function iniciarSesion(usuario, contrasena, cb) {
        const nombre = String(usuario || "").trim().toLowerCase()
        const u = _usuarios.find(x => x.usuario === nombre)
        if (!u || contrasena !== (u.contrasena || contrasenaDemo)) {
            _responder(cb, _falla("CREDENCIALES_INVALIDAS", "Usuario o contraseña incorrectos"))
            return
        }
        usuarioActual = _sinContrasena(u)
        _responder(cb, _ok(usuarioActual, "Bienvenido, " + u.nombre))
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
        _marcarEstado(elegidas, nuevoEstado, usuarioActual.id_usuario)
        const n = elegidas.length
        _responder(cb, _ok({ actualizadas: n }, n + (n === 1 ? " comanda" : " comandas") + " → " + Tema.nombreEstado(nuevoEstado)))
    }

    // Como validar_cambio_estado: se cancela desde cualquier estado y una cancelada
    // ya no cambia. Además (la base todavía no lo revisa) no se toca una cuenta
    // cerrada ni se deja saldo a favor, porque no hay forma de registrar devoluciones.
    function cancelarComandas(ids, cb) {
        const negado = _exigirAdministrador()
        if (negado) {
            _responder(cb, negado)
            return
        }
        const elegidas = _comandas.filter(c => ids.indexOf(c.id_comanda) >= 0)
        if (elegidas.length === 0) {
            _responder(cb, _falla("VALIDACION_FALLIDA", "No se indicó ninguna comanda"))
            return
        }
        const porCuenta = {}
        for (const c of elegidas) {
            if (c.estado === "cancelada") {
                _responder(cb, _falla("VALIDACION_FALLIDA", "La comanda " + c.id_comanda + " ya está cancelada"))
                return
            }
            porCuenta[c.secuencial] = (porCuenta[c.secuencial] || 0) + _centavos(c.precio_cobrado)
        }
        for (const s in porCuenta) {
            const secuencial = Number(s)
            if (_cuenta(secuencial).estado !== "abierta") {
                _responder(cb, _falla("VALIDACION_FALLIDA", "La cuenta está cerrada; sus comandas ya no se pueden cancelar"))
                return
            }
            if (_saldo(secuencial) < porCuenta[s]) {
                _responder(cb, _falla("VALIDACION_FALLIDA", "No se puede cancelar: el consumo que queda sería menor que lo ya pagado ("
                                      + (_centavos(_pagadoCuenta(secuencial)) / 100).toFixed(2) + ")"))
                return
            }
        }
        _cancelar(elegidas)
        const n = elegidas.length
        _responder(cb, _ok({ canceladas: n, saldo: _saldo(elegidas[0].secuencial) / 100 },
                           n === 1 ? "Se canceló 1 comanda" : "Se cancelaron " + n + " comandas"))
    }

    // La simulación aparta el stock al crear la orden y la base lo descuenta al
    // entrar a preparación: una pendiente no consumió nada y lo apartado regresa;
    // la que ya se empezó a preparar gastó sus ingredientes y no los devuelve.
    function _cancelar(comandas) {
        for (const c of comandas) {
            if (c.estado !== "pendiente" || !c.stock_apartado)
                continue
            for (const r of _producto(c.id_producto).receta) {
                const ingrediente = _ingrediente(r.id_ingrediente)
                if (ingrediente)
                    ingrediente.stock += r.cantidad
            }
        }
        _marcarEstado(comandas, "cancelada", usuarioActual.id_usuario)
        inventarioCambio()
    }

    function _marcarEstado(elegidas, nuevoEstado, idUsuario) {
        for (const c of elegidas) {
            c.estado = nuevoEstado
            // ck_comanda_preparacion: fuera de pendiente/cancelada debe haber estación
            if ((nuevoEstado === "en_preparacion" || nuevoEstado === "lista") && c.id_bar_cocina === null)
                c.id_bar_cocina = idUsuario
            // ck_comanda_cancelacion: cancelada si y solo si la canceló un cajero/admin
            if (nuevoEstado === "cancelada")
                c.id_cajero_cancela = idUsuario
        }
        comandasCambiaron()
        if (nuevoEstado === "lista")
            _avisarListas(elegidas)
    }

    // Un aviso por ronda (cuenta + envío) para el mesero que la levantó
    function _avisarListas(comandas) {
        const rondas = {}
        for (const c of comandas) {
            const clave = c.secuencial + "|" + c.fecha_hora + "|" + c.id_mesero
            if (!rondas[clave]) {
                const cuenta = _cuenta(c.secuencial)
                rondas[clave] = { id_mesero: c.id_mesero, numero_mesa: cuenta.numero_mesa,
                                  cuenta: _nombreCuenta(cuenta), conteo: {} }
            }
            const nombre = _producto(c.id_producto).nombre
            rondas[clave].conteo[nombre] = (rondas[clave].conteo[nombre] || 0) + 1
        }
        for (const k in rondas) {
            const r = rondas[k]
            r.productos = Object.keys(r.conteo).map(n => r.conteo[n] + "× " + n)
            delete r.conteo
            ordenLista(_copia(r))
        }
    }

    // La ronda más antigua del mesero con algo por terminar pasa completa a "lista"
    function _terminarOrdenSimulada() {
        const yo = usuarioActual.id_usuario
        const porTerminar = _comandas.filter(c => c.id_mesero === yo
                                             && (c.estado === "pendiente" || c.estado === "en_preparacion"))
        if (porTerminar.length === 0)
            return
        const primera = porTerminar.reduce((a, b) => a.fecha_hora <= b.fecha_hora ? a : b)
        const ronda = porTerminar.filter(c => c.secuencial === primera.secuencial && c.fecha_hora === primera.fecha_hora)
        for (const c of ronda)    // la estación según la categoría: Iván (bar) o Rosa (cocina)
            if (c.id_bar_cocina === null)
                c.id_bar_cocina = estacionDe(_producto(c.id_producto).categoria) === "bar" ? 4 : 3
        _marcarEstado(ronda, "lista", null)
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
        const negado = _exigirAdministrador()
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
        const negado = _exigirAdministrador()
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
        const negado = _exigirAdministrador()
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
        const negado = _exigirAdministrador()
        if (negado) {
            _responder(cb, negado)
            return
        }
        for (const i of _ingredientes)
            i.stock = 0
        inventarioCambio()
        _responder(cb, _ok(null, "Todas las existencias quedaron en 0"))
    }

    // ============================== Caja ==============================

    // Todas las cuentas del evento con su total, lo pagado y el saldo (v_cuenta).
    // Llevan los mismos campos que una ronda para poder agruparlas igual.
    function listarCuentasCaja(cb) {
        const negado = _exigirRol(["cajero_admin"])
        if (negado) {
            _responder(cb, negado)
            return
        }
        const lista = _cuentas.map(function (c) {
            const mesa = _mesas.find(m => m.numero_mesa === c.numero_mesa)
            const suyas = _comandas.filter(x => x.secuencial === c.secuencial)
            const ultima = suyas.reduce((m, x) => Math.max(m, x.fecha_hora), 0)
            const conPagos = _pagos.some(p => p.secuencial === c.secuencial)
            return {
                secuencial: c.secuencial, numero_mesa: c.numero_mesa, nombre: c.nombre, folio: _folio(c),
                cuenta: _nombreCuenta(c), estado: c.estado,
                // ck_cuenta_estado solo admite abierta y cerrada: una cuenta cancelada
                // es una cerrada sin pagos y sin ninguna comanda viva.
                cancelada: c.estado === "cerrada" && !conPagos && suyas.every(x => x.estado === "cancelada"),
                id_mesero: mesa.id_mesero, mesero: _usuario(mesa.id_mesero).nombre,
                fecha_hora: ultima,
                total: _centavos(_totalCuenta(c.secuencial)) / 100,
                pagado: _centavos(_pagadoCuenta(c.secuencial)) / 100,
                saldo: _saldo(c.secuencial) / 100,
                por_terminar: suyas.filter(x => x.estado !== "entregada" && x.estado !== "cancelada").length
            }
        })
        _responder(cb, _ok(lista))
    }

    function detalleCuenta(secuencial, cb) {
        const negado = _exigirRol(["cajero_admin"])
        if (negado) {
            _responder(cb, negado)
            return
        }
        const c = _cuenta(secuencial)
        if (!c) {
            _responder(cb, _falla("ERROR", "La cuenta ya no existe"))
            return
        }
        const comandas = _comandas.filter(x => x.secuencial === secuencial).map(x => ({
            id_comanda: x.id_comanda, id_producto: x.id_producto, producto: _producto(x.id_producto).nombre,
            detalles: x.detalles, estado: x.estado, precio: x.precio_cobrado
        }))
        const lineas = []
        for (const x of comandas) {
            let l = lineas.find(y => y.id_producto === x.id_producto && y.estado === x.estado && y.precio === x.precio)
            if (!l) {
                l = { id_producto: x.id_producto, producto: x.producto, estado: x.estado, precio: x.precio, cantidad: 0, ids: [] }
                lineas.push(l)
            }
            l.cantidad++
            l.ids.push(x.id_comanda)
        }
        const pagos = _pagos.filter(p => p.secuencial === secuencial).map(p => ({
            numero_pago: p.numero_pago, monto: p.monto, forma_pago: p.forma_pago,
            monto_propina: p.monto_propina, fecha_hora: p.fecha_hora,
            mesero_propina: p.id_mesero_propina ? _usuario(p.id_mesero_propina).nombre : ""
        }))
        _responder(cb, _ok({ secuencial: secuencial, lineas: lineas, pagos: pagos }))
    }

    // datos: { monto, forma_pago, monto_propina }. Las reglas son las de la
    // base (ck_pago_monto, ck_pago_propina_limite y tg_pago_saldo), con sus textos.
    function registrarPago(secuencial, datos, cb) {
        const negado = _exigirRol(["cajero_admin"])
        if (negado) {
            _responder(cb, negado)
            return
        }
        const c = _cuenta(secuencial)
        if (!c) {
            _responder(cb, _falla("ERROR", "La cuenta ya no existe"))
            return
        }
        if (c.estado !== "abierta") {
            _responder(cb, _falla("VALIDACION_FALLIDA", "La cuenta está cerrada; ya no recibe pagos"))
            return
        }
        const monto = _centavos(datos.monto)
        const propina = _centavos(datos.monto_propina || 0)
        if (isNaN(monto) || monto <= 0) {
            _responder(cb, _falla("VALIDACION_FALLIDA", "El monto debe ser mayor que 0"))
            return
        }
        if (["efectivo", "tarjeta", "transferencia"].indexOf(datos.forma_pago) < 0) {
            _responder(cb, _falla("VALIDACION_FALLIDA", "Elige la forma de pago"))
            return
        }
        if (isNaN(propina) || propina < 0) {
            _responder(cb, _falla("VALIDACION_FALLIDA", "La propina no puede ser negativa"))
            return
        }
        if (propina > monto) {
            _responder(cb, _falla("VALIDACION_FALLIDA", "La propina no puede ser mayor que el monto del pago"))
            return
        }
        const saldo = _saldo(secuencial)
        if (monto - propina > saldo) {
            _responder(cb, _falla("VALIDACION_FALLIDA", "El pago excede el saldo de la cuenta (" + (saldo / 100).toFixed(2) + ")"))
            return
        }
        const mesa = _mesas.find(m => m.numero_mesa === c.numero_mesa)
        _pagos.push({
            numero_pago: _pagos.reduce((m, p) => Math.max(m, p.numero_pago), 0) + 1,
            secuencial: secuencial, monto: monto / 100, forma_pago: datos.forma_pago,
            monto_propina: propina > 0 ? propina / 100 : null,
            id_mesero_propina: propina > 0 ? mesa.id_mesero : null,
            id_cajero: usuarioActual.id_usuario, fecha_hora: Date.now()
        })
        cuentasCambiaron()
        const restante = _saldo(secuencial)
        _responder(cb, _ok({ saldo: restante / 100 }, "Pago registrado · saldo " + Tema.dinero(restante / 100)))
    }

    // Como tg_cuenta_cierre: todas las comandas entregadas o canceladas y saldo 0.
    function cerrarCuenta(secuencial, cb) {
        const negado = _exigirRol(["cajero_admin"])
        if (negado) {
            _responder(cb, negado)
            return
        }
        const c = _cuenta(secuencial)
        if (!c || c.estado !== "abierta") {
            _responder(cb, _falla("VALIDACION_FALLIDA", "La cuenta ya está cerrada"))
            return
        }
        if (_comandas.some(x => x.secuencial === secuencial && x.estado !== "entregada" && x.estado !== "cancelada")) {
            _responder(cb, _falla("VALIDACION_FALLIDA", "La cuenta tiene comandas sin terminar"))
            return
        }
        const saldo = _saldo(secuencial)
        if (saldo !== 0) {
            _responder(cb, _falla("VALIDACION_FALLIDA", "La cuenta tiene saldo pendiente (" + (saldo / 100).toFixed(2) + ")"))
            return
        }
        c.estado = "cerrada"
        cuentasCambiaron()
        _responder(cb, _ok(null, "Se cerró la cuenta " + _nombreCuenta(c)))
    }

    // Sin tocar ck_cuenta_estado: cancela todas sus comandas y la cierra, lo que
    // tg_cuenta_cierre acepta. Con pagos no se puede, porque quedaría saldo a favor.
    function cancelarCuenta(secuencial, cb) {
        const negado = _exigirAdministrador()
        if (negado) {
            _responder(cb, negado)
            return
        }
        const c = _cuenta(secuencial)
        if (!c || c.estado !== "abierta") {
            _responder(cb, _falla("VALIDACION_FALLIDA", "La cuenta ya está cerrada"))
            return
        }
        if (_pagos.some(p => p.secuencial === secuencial)) {
            _responder(cb, _falla("VALIDACION_FALLIDA", "La cuenta ya tiene pagos registrados; no se puede cancelar"))
            return
        }
        _cancelar(_comandas.filter(x => x.secuencial === secuencial && x.estado !== "cancelada"))
        c.estado = "cerrada"
        cuentasCambiaron()
        _responder(cb, _ok(null, "Se canceló la cuenta " + _nombreCuenta(c)))
    }

    // Bebidas van al bar; alimentos y dulcería a cocina. Es un supuesto: en tu
    // esquema falta decidir de dónde sale la estación de cada producto.
    function estacionDe(categoria) {
        return categoria === "bebidas" ? "bar" : "cocina"
    }

    // ============================ Usuarios ============================

    // motivo_sin_baja explica por qué no se puede eliminar; vacío si sí se puede.
    function listarUsuarios(cb) {
        const negado = _exigirAdministrador()
        if (negado) {
            _responder(cb, negado)
            return
        }
        const lista = _usuarios.map(function (u) {
            const copia = _sinContrasena(u)
            copia.es_yo = u.id_usuario === usuarioActual.id_usuario
            copia.motivo_sin_baja = _motivoSinBaja(u)
            return copia
        })
        lista.sort((a, b) => a.nombre.localeCompare(b.nombre, "es"))
        _responder(cb, _ok(lista))
    }

    // datos: { id_usuario (si se edita), nombre, usuario, fecha_nacimiento,
    //          contrasena (vacía al editar = no cambia), rol,
    //          turno_asignado | estacion_asignada | nivel_acceso y caja_asignada }
    function guardarUsuario(datos, cb) {
        const negado = _exigirAdministrador()
        if (negado) {
            _responder(cb, negado)
            return
        }
        const actual = datos.id_usuario ? _usuario(datos.id_usuario) : null
        if (datos.id_usuario && !actual) {
            _responder(cb, _falla("ERROR", "El usuario ya no existe"))
            return
        }
        const error = _errorUsuario(datos, actual)
        if (error) {
            _responder(cb, _falla("VALIDACION_FALLIDA", error))
            return
        }
        const u = actual || { id_usuario: _usuarios.reduce((m, x) => Math.max(m, x.id_usuario), 0) + 1, rol: datos.rol }
        u.nombre = datos.nombre.trim()
        u.usuario = datos.usuario.trim().toLowerCase()
        u.fecha_nacimiento = datos.fecha_nacimiento.trim()
        if (datos.contrasena)
            u.contrasena = datos.contrasena
        if (u.rol === "mesero") {
            u.turno_asignado = datos.turno_asignado
        } else if (u.rol === "bar_cocina") {
            u.estacion_asignada = datos.estacion_asignada
        } else {
            u.nivel_acceso = datos.nivel_acceso
            u.caja_asignada = Number(datos.caja_asignada)
        }
        if (!actual)
            _usuarios.push(u)
        if (u.id_usuario === usuarioActual.id_usuario)
            usuarioActual = _sinContrasena(u)
        usuariosCambiaron()
        _responder(cb, _ok(_sinContrasena(u), actual ? "Usuario actualizado" : "Se creó el usuario " + u.usuario))
    }

    function eliminarUsuario(idUsuario, cb) {
        const negado = _exigirAdministrador()
        if (negado) {
            _responder(cb, negado)
            return
        }
        const u = _usuario(idUsuario)
        if (!u) {
            _responder(cb, _falla("ERROR", "El usuario ya no existe"))
            return
        }
        const motivo = _motivoSinBaja(u)
        if (motivo) {
            _responder(cb, _falla("VALIDACION_FALLIDA", motivo))
            return
        }
        _usuarios.splice(_usuarios.indexOf(u), 1)
        usuariosCambiaron()
        _responder(cb, _ok(null, "Se eliminó a " + u.nombre))
    }

    function _errorUsuario(datos, actual) {
        const nombre = String(datos.nombre || "").trim()
        const usuario = String(datos.usuario || "").trim().toLowerCase()
        const fecha = String(datos.fecha_nacimiento || "").trim()
        if (nombre.length === 0 || nombre.length > 120)
            return "El nombre es obligatorio y admite hasta 120 caracteres"
        if (!/^[a-z0-9._-]{1,40}$/.test(usuario))
            return "El usuario admite hasta 40 letras sin acento, números, punto, guion y guion bajo"
        const repetido = _usuarios.find(u => u.usuario === usuario && u !== actual)
        if (repetido)
            return "El usuario «" + usuario + "» ya existe"
        if (!_fechaValida(fecha))
            return "Escribe la fecha de nacimiento como AAAA-MM-DD"
        if (fecha < "1900-01-01" || fecha > Qt.formatDate(new Date(), "yyyy-MM-dd"))
            return "La fecha de nacimiento está fuera de rango"
        if (!actual && !datos.contrasena)
            return "Escribe una contraseña"
        if (datos.contrasena && !_contrasenaValida(datos.contrasena))
            return "La contraseña necesita 8 caracteres o más, una mayúscula, una minúscula, un número y 2 símbolos de * / _ - + ."

        // El rol va en la clave foránea de cada subtipo con ON UPDATE RESTRICT: no cambia
        const rol = actual ? actual.rol : datos.rol
        if (actual && datos.rol !== actual.rol)
            return "El rol no se puede cambiar; crea otro usuario"
        if (rol === "mesero") {
            if (["matutino", "vespertino", "nocturno"].indexOf(datos.turno_asignado) < 0)
                return "Elige el turno"
        } else if (rol === "bar_cocina") {
            if (["bar", "cocina"].indexOf(datos.estacion_asignada) < 0)
                return "Elige la estación"
        } else if (rol === "cajero_admin") {
            if (["cajero", "administrador"].indexOf(datos.nivel_acceso) < 0)
                return "Elige el nivel de acceso"
            if (!Number.isInteger(Number(datos.caja_asignada)) || Number(datos.caja_asignada) <= 0)
                return "La caja debe ser un número entero mayor que 0"
            // Como nadie se puede bajar de nivel a sí mismo, siempre queda un administrador
            if (actual && actual.id_usuario === usuarioActual.id_usuario && datos.nivel_acceso !== "administrador")
                return "No puedes quitarte a ti mismo el nivel de administrador"
        } else {
            return "Elige el rol"
        }
        return ""
    }

    // Las claves foráneas hacia los subtipos son ON DELETE RESTRICT: quien ya
    // atendió mesas, preparó, canceló o cobró se queda para no perder la historia.
    function _motivoSinBaja(u) {
        const id = u.id_usuario
        if (usuarioActual && id === usuarioActual.id_usuario)
            return "No puedes eliminar tu propia cuenta"
        if (_mesas.some(m => m.id_mesero === id)
                || _comandas.some(c => c.id_mesero === id || c.id_bar_cocina === id || c.id_cajero_cancela === id)
                || _pagos.some(p => p.id_cajero === id || p.id_mesero_propina === id))
            return u.nombre + " tiene mesas, comandas o pagos registrados; no se puede eliminar"
        return ""
    }

    // Las mismas reglas que validar_password de usuario.c
    function _contrasenaValida(contrasena) {
        return contrasena.length >= 8 && /[0-9]/.test(contrasena) && /[A-Z]/.test(contrasena) && /[a-z]/.test(contrasena)
                && (contrasena.match(/[*\/\-_+.]/g) || []).length >= 2
    }

    function _fechaValida(texto) {
        if (!/^\d{4}-\d{2}-\d{2}$/.test(texto))
            return false
        const partes = texto.split("-").map(Number)
        const fecha = new Date(partes[0], partes[1] - 1, partes[2])
        return fecha.getMonth() === partes[1] - 1 && fecha.getDate() === partes[2]
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
                    id_bar_cocina: null, id_cajero_cancela: null, stock_apartado: true
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
            [9,  3, 2, 2,  "cancelada",      null, 6,    ""],
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

    // Lo que los pagos abonan al consumo, sin propinas
    function _pagadoCuenta(secuencial) {
        return _pagos.filter(p => p.secuencial === secuencial)
                     .reduce((suma, p) => suma + p.monto - (p.monto_propina || 0), 0)
    }

    // El dinero se compara en centavos enteros para no arrastrar errores de float
    function _centavos(valor) { return Math.round(Number(String(valor).replace(",", ".")) * 100) }
    function _saldo(secuencial) { return _centavos(_totalCuenta(secuencial)) - _centavos(_pagadoCuenta(secuencial)) }

    function _exigirRol(roles) {
        if (!usuarioActual)
            return _falla("NO_AUTORIZADO", "Inicia sesión primero")
        if (roles.indexOf(usuarioActual.rol) < 0)
            return _falla("NO_AUTORIZADO", "Tu rol no puede hacer esta operación")
        return null
    }

    function _exigirAdministrador() {
        const negado = _exigirRol(["cajero_admin"])
        if (negado)
            return negado
        if (usuarioActual.nivel_acceso !== "administrador")
            return _falla("NO_AUTORIZADO", "Solo un administrador puede hacer esta operación")
        return null
    }

    function _sinContrasena(usuario) {
        const copia = _copia(usuario)
        delete copia.contrasena
        return copia
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
