"""Recorre las operaciones del servidor como lo haría el salón en una noche.

Uso: python3 probar_operaciones.py <puerto>

Necesita la base con los datos de ejemplo y las contraseñas de desarrollo
(Demo_123.). Escribe en la base: abre cuentas, crea comandas, registra un pago
y agrega y borra un ingrediente y un usuario de prueba. Todo lo que crea lleva
un sufijo al azar, así que se puede correr varias veces.
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
CONTRASENA = "Demo_123."

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


def codigo(r):
    return (r.get("error") or {}).get("codigo")


def mensaje(r):
    return (r.get("error") or {}).get("mensaje", "")


def entrar(usuario):
    s, error = cliente_ws.conectar()
    if s is None:
        print("No se pudo abrir el WebSocket:", error)
        sys.exit(1)
    r = pedir(s, "OP_LOGIN", {"usuario": usuario, "contrasena": CONTRASENA})
    if not r.get("ok"):
        print(f"No se pudo entrar como {usuario}:", r)
        sys.exit(1)
    return s


def avisos_de(tipo):
    return [a for a in cliente_ws.avisos if a.get("aviso") == tipo]


# ------------------------------------------------------------------ evento
s, _ = cliente_ws.conectar()
r = pedir(s, "OP_EVENTO_EN_CURSO")
caso("Evento en curso sin sesión", r.get("ok") and r["datos"]["numero_evento"] == 17
     and len(r["datos"]["organizadores"]) == 2, r)
cerrar(s)

# ------------------------------------------------------------------ mesero
mesero = entrar("lmendez")
r = pedir(mesero, "OP_LISTAR_MESAS")
mesas = r.get("datos") or []
caso("Mesas: las del mesero primero", r.get("ok") and mesas and mesas[0]["mia"]
     and [m["numero_mesa"] for m in mesas if m["mia"]] == [1, 2], r)

r = pedir(mesero, "OP_LISTAR_CUENTAS", {"numero_mesa": 1})
caso("Cuentas abiertas de la mesa 1", r.get("ok") and {"Novios", "Padrinos"} <= {c["nombre"] for c in r["datos"]}, r)

nombre_cuenta = "Prueba " + SUFIJO
r = pedir(mesero, "OP_CREAR_CUENTA", {"numero_mesa": 2, "nombre": nombre_cuenta})
cuenta = r.get("datos") or {}
caso("Abrir cuenta en la mesa 2", r.get("ok") and cuenta.get("nombre") == nombre_cuenta
     and cuenta.get("folio", "").startswith("17-2-"), r)

r = pedir(mesero, "OP_CREAR_CUENTA", {"numero_mesa": 2, "nombre": nombre_cuenta.upper()})
caso("Nombre repetido en la misma mesa", codigo(r) == "VALIDACION_FALLIDA" and "Ya hay una cuenta" in mensaje(r), r)

r = pedir(mesero, "OP_CREAR_CUENTA", {"numero_mesa": 99, "nombre": "x"})
caso("Mesa que no existe", codigo(r) == "VALIDACION_FALLIDA" and "no existe" in mensaje(r), r)

cliente_ws.avisos.clear()
r = pedir(mesero, "OP_CREAR_PEDIDO", {"numero_mesa": 2, "secuencial": cuenta.get("secuencial"), "lineas": [
    {"id_producto": 1, "cantidad": 2, "detalles": "sin hielo"},
    {"id_producto": 3, "cantidad": 1},
]})
pedido = r.get("datos") or {}
ids = pedido.get("id_comandas", [])
caso("Pedido de 2 refrescos y 1 pasta", r.get("ok") and pedido.get("creadas") == 3, r)
caso("Llegan los avisos de comandas y cuentas", avisos_de("comandas") and avisos_de("cuentas"), cliente_ws.avisos)

r = pedir(mesero, "OP_LISTAR_PEDIDOS", {"estacion": "bar", "estados": ["pendiente"]})
del_pedido = [c for c in (r.get("datos") or []) if c["id_comanda"] in ids]
caso("Bar ve los 2 refrescos pendientes", r.get("ok") and len(del_pedido) == 2
     and all(c["estacion"] == "bar" and c["detalles"] == "sin hielo" for c in del_pedido), r)

r = pedir(mesero, "OP_LISTAR_USUARIOS")
caso("Mesero no puede ver usuarios", codigo(r) == "NO_AUTORIZADO", r)

r = pedir(mesero, "OP_CAMBIAR_ESTADO_PEDIDO", {"ids": ids[:2], "estado": "entregada"})
caso("Entregar algo que no está listo", codigo(r) == "VALIDACION_FALLIDA" and "No se puede pasar de «Pendiente»" in mensaje(r), r)

# ------------------------------------------------------------------ cocina y bar
cocina = entrar("rgamez")
r = pedir(cocina, "OP_LISTAR_INGREDIENTES")
antes = {i["nombre"]: i["stock"] for i in r.get("datos") or []}

r = pedir(cocina, "OP_CAMBIAR_ESTADO_PEDIDO", {"ids": ids[:1], "estado": "en_preparacion"})
caso("Cocina no prepara bebidas (tg_comanda_estacion)", codigo(r) == "VALIDACION_FALLIDA" and "bar" in mensaje(r), r)

r = pedir(cocina, "OP_CAMBIAR_ESTADO_PEDIDO", {"ids": ids[2:], "estado": "en_preparacion"})
caso("Cocina empieza la pasta", r.get("ok") and r["datos"]["actualizadas"] == 1, r)

r = pedir(cocina, "OP_LISTAR_INGREDIENTES")
despues = {i["nombre"]: i["stock"] for i in r.get("datos") or []}
caso("Se descontó la receta de la pasta (120 g)", despues.get("Pasta fettuccine") == antes.get("Pasta fettuccine", 0) - 120,
     {"antes": antes.get("Pasta fettuccine"), "despues": despues.get("Pasta fettuccine")})

bar = entrar("isoto")
cliente_ws.avisos.clear()
r = pedir(bar, "OP_CAMBIAR_ESTADO_PEDIDO", {"ids": ids[:2], "estado": "lista"})
caso("Bar termina los refrescos", r.get("ok") and r["datos"]["actualizadas"] == 2, r)
# El aviso de orden lista le llega solo al mesero, por su propia conexión
aviso = recibir(mesero, con_avisos=True)
while aviso.get("aviso") != "orden_lista":
    aviso = recibir(mesero, con_avisos=True)
caso("Al mesero le llega «orden lista»", aviso["datos"]["productos"] == ["2× Refresco"]
     and aviso["datos"]["numero_mesa"] == 2, aviso)

r = pedir(mesero, "OP_CAMBIAR_ESTADO_PEDIDO", {"ids": ids[:2], "estado": "entregada"})
caso("Mesero entrega los refrescos", r.get("ok"), r)

r = pedir(cocina, "OP_CAMBIAR_ESTADO_PEDIDO", {"ids": ids[2:], "estado": "pendiente"})
caso("Estado al que no se puede ir", codigo(r) == "VALIDACION_FALLIDA", r)

# ------------------------------------------------------------------ caja
admin = entrar("mrivas")
secuencial = cuenta.get("secuencial")
r = pedir(admin, "OP_LISTAR_CUENTAS_CAJA")
en_caja = [c for c in r.get("datos") or [] if c["secuencial"] == secuencial]
caso("Caja ve la cuenta con su total", r.get("ok") and en_caja and float(en_caja[0]["total"]) == 250
     and en_caja[0]["por_terminar"] == 1, en_caja)

r = pedir(admin, "OP_DETALLE_CUENTA", {"secuencial": secuencial})
caso("Detalle con 3 comandas y sin pagos", r.get("ok") and len(r["datos"]["comandas"]) == 3 and r["datos"]["pagos"] == [], r)

r = pedir(admin, "OP_REGISTRAR_PAGO", {"secuencial": secuencial, "monto": 300, "forma_pago": "efectivo"})
caso("Pago mayor que el saldo (tg_pago_saldo)", codigo(r) == "VALIDACION_FALLIDA" and "excede el saldo" in mensaje(r), r)

r = pedir(admin, "OP_REGISTRAR_PAGO", {"secuencial": secuencial, "monto": 80, "forma_pago": "tarjeta", "monto_propina": 10})
caso("Pago de 80 con 10 de propina", r.get("ok") and float(r["datos"]["saldo"]) == 180, r)

r = pedir(admin, "OP_CERRAR_CUENTA", {"secuencial": secuencial})
caso("No se cierra con saldo y comandas vivas", codigo(r) == "VALIDACION_FALLIDA", r)

r = pedir(admin, "OP_ELIMINAR_PEDIDO", {"ids": ids[2:]})
caso("Cancelar la pasta deja saldo 0", r.get("ok") and float(r["datos"]["saldo"]) == 0 and r["datos"]["canceladas"] == 1, r)

r = pedir(admin, "OP_ELIMINAR_PEDIDO", {"ids": ids[2:]})
caso("Cancelar dos veces", codigo(r) == "VALIDACION_FALLIDA" and "ya está cancelada" in mensaje(r), r)

r = pedir(admin, "OP_CERRAR_CUENTA", {"secuencial": secuencial})
caso("Cerrar la cuenta", r.get("ok") and r["datos"]["cuenta"].startswith(nombre_cuenta), r)

r = pedir(admin, "OP_CANCELAR_CUENTA", {"secuencial": 1})
caso("No se cancela una cuenta con pagos", codigo(r) == "VALIDACION_FALLIDA" and "pagos" in mensaje(r), r)

r = pedir(mesero, "OP_CREAR_CUENTA", {"numero_mesa": 1, "nombre": "Cancelar " + SUFIJO})
otra = (r.get("datos") or {}).get("secuencial")
pedir(mesero, "OP_CREAR_PEDIDO", {"numero_mesa": 1, "secuencial": otra, "lineas": [{"id_producto": 4, "cantidad": 1}]})
r = pedir(admin, "OP_CANCELAR_CUENTA", {"secuencial": otra})
caso("Cancelar una cuenta sin pagos", r.get("ok"), r)
r = pedir(admin, "OP_LISTAR_CUENTAS_CAJA")
cancelada = [c for c in r.get("datos") or [] if c["secuencial"] == otra]
caso("En caja aparece como cancelada", cancelada and cancelada[0]["cancelada"] is True, cancelada)

# ------------------------------------------------------------------ inventario
nombre_ing = "Prueba " + SUFIJO
r = pedir(admin, "OP_CREAR_INGREDIENTE", {"nombre": nombre_ing, "unidad_medida": "g", "stock": 500, "stock_minimo": 100})
ingrediente = r.get("datos") or {}
caso("Agregar ingrediente", r.get("ok") and ingrediente.get("stock") == 500 and ingrediente.get("unidad_medida") == "g", r)

r = pedir(admin, "OP_CREAR_INGREDIENTE", {"nombre": nombre_ing.lower(), "stock": 1})
caso("Nombre repetido sin importar mayúsculas", codigo(r) == "VALIDACION_FALLIDA" and "Ya existe" in mensaje(r), r)

r = pedir(admin, "OP_MODIFICAR_INGREDIENTE", {"id_ingrediente": ingrediente.get("id_ingrediente"),
                                              "nombre": nombre_ing, "stock": 750.5})
caso("Modificar la existencia", r.get("ok") and r["datos"]["stock"] == 750.5 and r["datos"]["unidad_medida"] == "g", r)

r = pedir(admin, "OP_AJUSTAR_STOCK", {"id_ingrediente": ingrediente.get("id_ingrediente"), "delta": -1000})
caso("Ajustar no baja de 0", r.get("ok") and r["datos"]["stock"] == 0, r)

r = pedir(admin, "OP_ELIMINAR_INGREDIENTE", {"id_ingrediente": ingrediente.get("id_ingrediente")})
caso("Eliminar ingrediente", r.get("ok") and r["datos"]["nombre"] == nombre_ing and r["datos"]["recetas"] == 0, r)

r = pedir(mesero, "OP_REINICIAR_STOCK")
caso("Mesero no puede reiniciar el stock", codigo(r) == "NO_AUTORIZADO", r)

# ------------------------------------------------------------------ usuarios
nuevo = {"nombre": "Usuario de Prueba", "usuario": "prueba" + SUFIJO, "fecha_nacimiento": "2000-02-29",
         "contrasena": "debil", "rol": "mesero", "turno_asignado": "nocturno"}
r = pedir(admin, "OP_CREAR_USUARIO", nuevo)
caso("Contraseña que no cumple la regla", codigo(r) == "VALIDACION_FALLIDA" and "8 caracteres" in mensaje(r), r)

nuevo["contrasena"] = "Prueba_1.x"
r = pedir(admin, "OP_CREAR_USUARIO", dict(nuevo, fecha_nacimiento="2001-02-29"))
caso("Fecha que no existe", codigo(r) == "VALIDACION_FALLIDA" and "AAAA-MM-DD" in mensaje(r), r)

r = pedir(admin, "OP_CREAR_USUARIO", nuevo)
creado = r.get("datos") or {}
caso("Crear mesero", r.get("ok") and creado.get("turno_asignado") == "nocturno" and creado.get("motivo_sin_baja") == ""
     and "contrasena_hash" not in creado, r)

s_nuevo, _ = cliente_ws.conectar()
r = pedir(s_nuevo, "OP_LOGIN", {"usuario": nuevo["usuario"], "contrasena": nuevo["contrasena"]})
caso("El usuario nuevo puede entrar", r.get("ok") and r["datos"]["rol"] == "mesero", r)
cerrar(s_nuevo)

r = pedir(admin, "OP_MODIFICAR_USUARIO", dict(nuevo, id_usuario=creado.get("id_usuario"), contrasena="",
                                              rol="bar_cocina", estacion_asignada="bar"))
caso("El rol no se cambia", codigo(r) == "VALIDACION_FALLIDA" and "rol" in mensaje(r), r)

r = pedir(admin, "OP_MODIFICAR_USUARIO", dict(nuevo, id_usuario=creado.get("id_usuario"), contrasena="",
                                              turno_asignado="matutino"))
caso("Cambiar el turno", r.get("ok") and r["datos"]["turno_asignado"] == "matutino", r)

r = pedir(admin, "OP_LISTAR_USUARIOS")
lista = {u["usuario"]: u for u in r.get("datos") or []}
caso("Lista con motivos de baja", r.get("ok") and lista.get("mrivas", {}).get("es_yo") is True
     and "tiene mesas" in lista.get("lmendez", {}).get("motivo_sin_baja", ""), {k: v.get("motivo_sin_baja") for k, v in lista.items()})

r = pedir(admin, "OP_ELIMINAR_USUARIO", {"id_usuario": 1})
caso("No se elimina a quien tiene historia", codigo(r) == "VALIDACION_FALLIDA" and "tiene mesas" in mensaje(r), r)

r = pedir(admin, "OP_ELIMINAR_USUARIO", {"id_usuario": 5})
caso("No se elimina a sí mismo", codigo(r) == "VALIDACION_FALLIDA", r)

r = pedir(admin, "OP_ELIMINAR_USUARIO", {"id_usuario": creado.get("id_usuario")})
caso("Eliminar el usuario de prueba", r.get("ok") and r["datos"]["nombre"] == "Usuario de Prueba", r)

for s in (mesero, cocina, bar, admin):
    cerrar(s)

print(f"\n{sum(resultados)} de {len(resultados)} casos pasan")
sys.exit(0 if all(resultados) else 1)
