"""Prueba la administración: productos, paquetes, eventos, anticipos, el cobro
de los paquetes, los logs y lo que puede cancelar el cajero.

Uso: python3 probar_admin.py <puerto>

Necesita la base de la demo: los datos de la Práctica 4 (db/demo/03-poblar.sh)
más db/demo/datos-demo.sql, porque entra con adminDemo, cajeroDemo y
meseroDemo1. No depende de ningún id fijo: busca en lo que regresa el servidor.
Crea eventos, y los eventos no se borran, así que conviene correrla contra una
base desechable, no contra la de trabajo.
"""
import json
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cliente_ws  # noqa: E402
from cliente_ws import TEXT, cerrar, enviar, recibir  # noqa: E402

cliente_ws.PUERTO = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 9100
SUFIJO = str(random.randint(1000, 9999))

resultados = []
siguiente_id = [1]


def caso(nombre, condicion, detalle=""):
    resultados.append(bool(condicion))
    marca = "PASA" if condicion else "FALLA"
    texto = json.dumps(detalle, ensure_ascii=False) if not isinstance(detalle, str) else detalle
    print(f"[{marca}] {len(resultados)}. {nombre}\n        {texto[:300]}")


def pedir(s, op, datos=None):
    siguiente_id[0] += 1
    enviar(s, TEXT, json.dumps({"id": siguiente_id[0], "op": op, "datos": datos}).encode())
    return recibir(s)


def mensaje(r):
    return (r.get("error") or {}).get("mensaje", "")


def codigo(r):
    return (r.get("error") or {}).get("codigo")


def entrar(usuario):
    s, error = cliente_ws.conectar()
    if s is None:
        sys.exit(f"No se pudo abrir el WebSocket: {error}")
    r = pedir(s, "OP_LOGIN", {"usuario": usuario, "contrasena": "Demo_123."})
    if not r.get("ok"):
        sys.exit(f"No se pudo entrar como {usuario}: {r}")
    return s


admin = entrar("adminDemo")
cajero = entrar("cajeroDemo")
mesero = entrar("meseroDemo1")
ID_MESERO_1, ID_MESERO_2, ID_COCINA = 201, 202, 203   # datos-demo.sql

eventos = pedir(admin, "OP_LISTAR_EVENTOS").get("datos") or []
en_curso = next((e for e in eventos if e["estado"] == "en_curso"), {})
productos = pedir(mesero, "OP_LISTAR_PRODUCTOS").get("datos") or []
ingredientes = pedir(mesero, "OP_LISTAR_INGREDIENTES").get("datos") or []

# ------------------------------------------------------------------ datos de la Práctica 4
caso("La demo trae los datos de la Práctica 4 (240 eventos, más de 200 productos e ingredientes)",
     len(eventos) >= 240 and len(productos) >= 200 and len(ingredientes) >= 200,
     {"eventos": len(eventos), "productos": len(productos), "ingredientes": len(ingredientes)})
caso("Hay un evento en curso con mesas, y es de hoy", en_curso.get("mesas") and en_curso.get("cuentas_abiertas", 0) > 0,
     {k: en_curso.get(k) for k in ("numero_evento", "tipo_evento", "fecha", "cuentas_abiertas")})

# ------------------------------------------------------------------ cajero
r = pedir(cajero, "OP_LISTAR_PEDIDOS", {"estacion": "todas", "estados": ["pendiente"]})
pendiente = (r.get("datos") or [{}])[0]
r = pedir(cajero, "OP_ELIMINAR_PEDIDO", {"ids": [pendiente.get("id_comanda")]})
caso("El cajero cancela una comanda suelta", r.get("ok") and r["datos"]["canceladas"] == 1, r)
r = pedir(cajero, "OP_CANCELAR_CUENTA", {"secuencial": pendiente.get("secuencial")})
caso("El cajero no cancela cuentas completas", codigo(r) == "NO_AUTORIZADO", r)
r = pedir(cajero, "OP_LISTAR_EVENTOS")
caso("El cajero no administra eventos", codigo(r) == "NO_AUTORIZADO", r)

# ------------------------------------------------------------------ productos
pedido = next((p for p in productos if len(p["receta"]) >= 2 and "comandas" in p["motivo_sin_baja"]), {})
caso("La lista de productos trae la receta con cantidades y unidad", pedido
     and all(i["cantidad"] > 0 and i["unidad_medida"] in ("g", "ml", "pieza") for i in pedido["receta"]), pedido.get("receta"))
caso("Un producto ya pedido no se puede borrar", "comandas" in pedido.get("motivo_sin_baja", ""), pedido.get("motivo_sin_baja"))

i1, i2 = ingredientes[0]["id_ingrediente"], ingredientes[1]["id_ingrediente"]
nombre = "Mojito " + SUFIJO
r = pedir(admin, "OP_CREAR_PRODUCTO", {"nombre": nombre, "descripcion": "Con hierbabuena", "categoria": "bebidas",
                                       "precio": 95.5, "id_producto_principal": None,
                                       "receta": [{"id_ingrediente": i1, "cantidad": 1}, {"id_ingrediente": i2, "cantidad": 2.5}]})
nuevo = r.get("datos") or {}
caso("Crear producto con receta", r.get("ok") and nuevo.get("id_producto"), r)

r = pedir(admin, "OP_CREAR_PRODUCTO", {"nombre": nombre.upper(), "categoria": "bebidas", "precio": 1, "receta": []})
caso("Nombre repetido sin importar mayúsculas", "Ya existe" in mensaje(r), r)

r = pedir(admin, "OP_CREAR_PRODUCTO", {"nombre": "x" + SUFIJO, "categoria": "bebidas", "precio": 1,
                                       "receta": [{"id_ingrediente": i1, "cantidad": 1}, {"id_ingrediente": i1, "cantidad": 2}]})
caso("Ingrediente repetido en la receta", "dos veces" in mensaje(r), r)

variante = next((p for p in productos if p["id_producto_principal"]), {})
r = pedir(admin, "OP_CREAR_PRODUCTO", {"nombre": "Variante " + SUFIJO, "categoria": "bebidas", "precio": 99,
                                       "id_producto_principal": variante.get("id_producto"), "receta": []})
caso("Variante de una variante (tg_producto_variante)", r.get("ok") is False and "variante" in mensaje(r), r)

principal = next((p for p in productos if not p["id_producto_principal"] and p["categoria"] == "bebidas"), {})
r = pedir(admin, "OP_MODIFICAR_PRODUCTO", {"id_producto": nuevo.get("id_producto"), "nombre": nombre,
                                           "categoria": "bebidas", "precio": 100,
                                           "id_producto_principal": principal.get("id_producto"),
                                           "receta": [{"id_ingrediente": i1, "cantidad": 3}]})
caso("Modificar: precio, variante de otro producto y receta nueva", r.get("ok"), r)
productos = pedir(mesero, "OP_LISTAR_PRODUCTOS").get("datos") or []
mojito = next((p for p in productos if p["nombre"] == nombre), {})
caso("Se ve el cambio", mojito.get("precio") == "100.00" and mojito.get("variante_de") == principal.get("nombre")
     and [i["cantidad"] for i in mojito.get("receta", [])] == [3] and mojito.get("motivo_sin_baja") == "", mojito)

# ------------------------------------------------------------------ paquetes
p1, p2 = productos[0]["id_producto"], productos[1]["id_producto"]
detalles = "Paquete Prueba " + SUFIJO + ": cena y bebidas"
r = pedir(admin, "OP_CREAR_PAQUETE", {"detalles": detalles, "monto": 15000, "productos": [p1, p2, mojito.get("id_producto")]})
paquete = (r.get("datos") or {}).get("numero_paquete")
caso("Crear paquete con 3 productos", r.get("ok") and paquete, r)

r = pedir(admin, "OP_ELIMINAR_PRODUCTO", {"id_producto": mojito.get("id_producto")})
caso("Un producto en un paquete no se borra", "paquete" in mensaje(r), r)

r = pedir(admin, "OP_MODIFICAR_PAQUETE", {"numero_paquete": paquete, "detalles": detalles, "monto": 16000, "productos": [p1, p2]})
caso("Quitar un producto del paquete", r.get("ok"), r)
r = pedir(admin, "OP_LISTAR_PAQUETES")
mio = next((p for p in r.get("datos") or [] if p["numero_paquete"] == paquete), {})
caso("El paquete queda con 2 productos y el monto nuevo", len(mio.get("productos", [])) == 2 and float(mio.get("monto", 0)) == 16000, mio)

r = pedir(admin, "OP_ELIMINAR_PRODUCTO", {"id_producto": mojito.get("id_producto")})
caso("Ya fuera del paquete, el producto se borra", r.get("ok") and r["datos"]["nombre"] == nombre, r)

# ------------------------------------------------------------------ eventos
evento = {"fecha": "2027-01-15", "tipo_evento": "Graduación " + SUFIJO, "numero_asistentes": 120,
          "organizadores": ["Escuela Prueba", "Escuela Prueba", "Comité"],
          "mesas": [{"numero_mesa": 1, "id_mesero": ID_MESERO_1}, {"numero_mesa": 2, "id_mesero": ID_MESERO_2}],
          "paquetes": [{"numero_paquete": paquete, "productos": [{"id_producto": p1, "cantidad": 120},
                                                                 {"id_producto": p2, "cantidad": 240}]}],
          "repetir": {"cada": "meses", "intervalo": 1, "veces": 3}}
r = pedir(admin, "OP_CREAR_EVENTO", evento)
caso("Crear evento que se repite 3 meses", r.get("ok") and r["datos"]["creados"] == 3, r)

r = pedir(admin, "OP_LISTAR_EVENTOS", {"desde": "2027-01-01", "hasta": "2027-12-31"})
mios = [e for e in r.get("datos") or [] if e["tipo_evento"] == evento["tipo_evento"]]
caso("Las 3 copias, un mes aparte, con organizadores, mesas y paquete",
     [e["fecha"] for e in mios] == ["2027-01-15", "2027-02-15", "2027-03-15"]
     and all(len(e["organizadores"]) == 2 and len(e["mesas"]) == 2 and len(e["paquetes"]) == 1 for e in mios),
     [(e["fecha"], e["organizadores"], len(e["mesas"])) for e in mios])
primero = mios[0]["numero_evento"] if mios else 0
tercero = mios[2]["numero_evento"] if len(mios) > 2 else 0

r = pedir(admin, "OP_ELIMINAR_PAQUETE", {"numero_paquete": paquete})
caso("Un paquete con eventos no se borra", "eventos" in mensaje(r), r)

cambios = dict(evento, numero_evento=primero, numero_asistentes=150, organizadores=["Comité"],
               mesas=[{"numero_mesa": 1, "id_mesero": ID_MESERO_2}, {"numero_mesa": 2, "id_mesero": ID_MESERO_2},
                      {"numero_mesa": 3, "id_mesero": ID_MESERO_1}],
               paquetes=[])
del cambios["repetir"]
r = pedir(admin, "OP_MODIFICAR_EVENTO", cambios)
caso("Modificar: asistentes, organizadores, mesa nueva y sin paquetes", r.get("ok"), r)
r = pedir(admin, "OP_MODIFICAR_EVENTO", dict(cambios, mesas=[{"numero_mesa": 1, "id_mesero": ID_MESERO_2}]))
caso("Una mesa guardada no se quita", "no se puede quitar" in mensaje(r), r)
r = pedir(admin, "OP_MODIFICAR_EVENTO", dict(cambios, mesas=[{"numero_mesa": 1, "id_mesero": ID_COCINA},
                                                              {"numero_mesa": 2, "id_mesero": ID_MESERO_2},
                                                              {"numero_mesa": 3, "id_mesero": ID_MESERO_1}]))
caso("El mesero de una mesa debe ser mesero", "no es mesero" in mensaje(r), r)

r = pedir(admin, "OP_CAMBIAR_ESTADO_EVENTO", {"numero_evento": primero, "estado": "en_curso"})
caso("No se inicia otro con uno en curso", "Ya hay un evento en curso" in mensaje(r), r)
r = pedir(admin, "OP_CAMBIAR_ESTADO_EVENTO", {"numero_evento": en_curso.get("numero_evento"), "estado": "finalizado"})
caso("No se finaliza con cuentas abiertas", "cuentas abiertas" in mensaje(r), r)
r = pedir(admin, "OP_CAMBIAR_ESTADO_EVENTO", {"numero_evento": tercero, "estado": "cancelado"})
caso("Cancelar la tercera copia", r.get("ok"), r)
r = pedir(admin, "OP_MODIFICAR_EVENTO", dict(cambios, numero_evento=tercero))
caso("Un evento cancelado ya no se modifica", "cancelado" in mensaje(r), r)

r = pedir(admin, "OP_REGISTRAR_ANTICIPO", {"numero_evento": primero, "monto": 5000})
r2 = pedir(admin, "OP_REGISTRAR_ANTICIPO", {"numero_evento": primero, "monto": 2500.5, "fecha": "2026-12-01"})
r3 = pedir(admin, "OP_LISTAR_EVENTOS", {"desde": "2027-01-15", "hasta": "2027-01-15"})
uno = next((e for e in r3.get("datos") or [] if e["numero_evento"] == primero), {})
caso("Dos anticipos suman 7500.50", r.get("ok") and r2.get("ok") and float(uno.get("total_anticipos", 0)) == 7500.5
     and len(uno.get("anticipos", [])) == 2 and uno.get("numero_asistentes") == 150 and len(uno.get("mesas", [])) == 3, uno)
r = pedir(admin, "OP_REGISTRAR_ANTICIPO", {"numero_evento": primero, "monto": 0})
caso("Anticipo de 0", "mayor que 0" in mensaje(r), r)

# ------------------------------------------------------------------ cobro de paquetes (migración 003)
# En el evento en curso: un platillo y una bebida que no vienen en sus paquetes,
# con una porción incluida cada uno. Al acabarse, el platillo ya no se vende y
# la bebida la paga la cuenta del anfitrión.
incluidos = {x["id_producto"] for p in en_curso.get("paquetes", []) for x in p["productos"]}
libres = [p for p in productos if p["disponible"] and p["maximo"] >= 3 and p["id_producto"] not in incluidos]
platillo = next((p for p in libres if p["categoria"] == "alimentos"), {})
bebida = next((p for p in libres if p["categoria"] == "bebidas"), {})

cuentas_mesa = pedir(mesero, "OP_LISTAR_CUENTAS", {"numero_mesa": 1}).get("datos") or []
if not cuentas_mesa:
    cuentas_mesa = [pedir(mesero, "OP_CREAR_CUENTA", {"numero_mesa": 1, "nombre": "Prueba " + SUFIJO})["datos"]]
invitado = cuentas_mesa[0]
anfitrion = next(c for c in en_curso["cuentas"] if c["estado"] == "abierta" and c["secuencial"] != invitado["secuencial"])

datos_evento = {"numero_evento": en_curso["numero_evento"], "fecha": en_curso["fecha"],
                "tipo_evento": en_curso["tipo_evento"], "numero_asistentes": en_curso["numero_asistentes"],
                "organizadores": en_curso["organizadores"],
                "mesas": [{"numero_mesa": m["numero_mesa"], "id_mesero": m["id_mesero"]} for m in en_curso["mesas"]],
                "paquetes": [{"numero_paquete": paquete, "productos": [
                    {"id_producto": platillo.get("id_producto"), "cantidad": 1, "modo_cobro": "bloquear"},
                    {"id_producto": bebida.get("id_producto"), "cantidad": 1, "modo_cobro": "anfitrion"}]}],
                "secuencial_anfitrion": anfitrion["secuencial"]}
r = pedir(admin, "OP_MODIFICAR_EVENTO", datos_evento)
caso("Evento en curso: 1 platillo (luego no se vende) y 1 bebida (luego paga el anfitrión)", r.get("ok"), r)

orden = {"numero_mesa": 1, "secuencial": invitado["secuencial"]}
r = pedir(mesero, "OP_CREAR_PEDIDO", dict(orden, lineas=[{"id_producto": platillo["id_producto"], "cantidad": 1}]))
r2 = pedir(mesero, "OP_CREAR_PEDIDO", dict(orden, lineas=[{"id_producto": platillo["id_producto"], "cantidad": 1}]))
caso("El platillo incluido sale y el siguiente ya no se vende", r.get("ok") and "todo lo acordado" in mensaje(r2),
     [r.get("ok"), mensaje(r2)])

r = pedir(mesero, "OP_CREAR_PEDIDO", dict(orden, lineas=[{"id_producto": bebida["id_producto"], "cantidad": 2}]))
r2 = pedir(admin, "OP_DETALLE_CUENTA", {"secuencial": invitado["secuencial"]})
tipos = [c["tipo_cobro"] for c in r2["datos"]["comandas"] if c["id_producto"] == bebida["id_producto"]]
caso("2 bebidas: una incluida y la otra para el anfitrión", r.get("ok") and tipos[-2:] == ["paquete", "anfitrion"], tipos)
r = pedir(admin, "OP_DETALLE_CUENTA", {"secuencial": anfitrion["secuencial"]})
cargo = [c for c in r["datos"]["comandas"] if c.get("de_otra_cuenta")]
caso("La cuenta del anfitrión ve el cargo y de qué cuenta viene", r["datos"]["es_anfitrion"] and len(cargo) == 1
     and cargo[0]["de_otra_cuenta"] == invitado["cuenta"], cargo)

productos = pedir(mesero, "OP_LISTAR_PRODUCTOS").get("datos") or []
restantes = next((p["incluidos_restantes"] for p in productos if p["id_producto"] == platillo["id_producto"]), None)
caso("El mesero ve que ya no quedan platillos incluidos", restantes == 0, restantes)

r = pedir(admin, "OP_MODIFICAR_EVENTO", dict(datos_evento, secuencial_anfitrion=invitado["secuencial"]))
caso("El anfitrión ya no se cambia si tiene cargos", "ya no se puede cambiar" in mensaje(r), r)

# ------------------------------------------------------------------ cuentas de eventos pasados
pasado = next((e for e in eventos if e["estado"] == "finalizado"), {})
r = pedir(admin, "OP_LISTAR_CUENTAS_CAJA", {"numero_evento": pasado.get("numero_evento")})
cuentas_pasadas = r.get("datos") or []
caso("El admin consulta las cuentas de un evento finalizado", r.get("ok") and len(cuentas_pasadas) > 0
     and all(c["estado"] == "cerrada" for c in cuentas_pasadas), len(cuentas_pasadas))
r = pedir(admin, "OP_DETALLE_CUENTA", {"secuencial": cuentas_pasadas[0]["secuencial"] if cuentas_pasadas else 0,
                                       "numero_evento": pasado.get("numero_evento")})
caso("Y el detalle de una de ellas", r.get("ok") and len(r["datos"]["comandas"]) > 0, r.get("error"))
r = pedir(cajero, "OP_LISTAR_CUENTAS_CAJA", {"numero_evento": pasado.get("numero_evento")})
caso("El cajero solo ve las del evento en curso", codigo(r) == "NO_AUTORIZADO", r)

# ------------------------------------------------------------------ logs
r = pedir(admin, "OP_LISTAR_LOGS")
textos = [l["texto"] for l in r.get("datos") or []]
caso("Los logs tienen lo que hizo el admin, lo más nuevo primero",
     r.get("ok") and "rechazada" in textos[0] and any("ya no se puede cambiar" in t for t in textos[:3])
     and any("anticipo" in t for t in textos)
     and any("creó 3 evento(s)" in t for t in textos), textos[:3])
r = pedir(cajero, "OP_LISTAR_LOGS")
caso("El cajero no ve los logs", codigo(r) == "NO_AUTORIZADO", r)

for s in (admin, cajero, mesero):
    cerrar(s)
print(f"\n{sum(resultados)} de {len(resultados)} casos pasan")
sys.exit(0 if all(resultados) else 1)
