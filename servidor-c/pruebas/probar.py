"""Prueba el servidor en C con un cliente WebSocket mínimo (solo stdlib).

Uso: python3 -I probar.py <puerto> [--bd]

Sin --bd prueba solo la conexión y el protocolo, sin base de datos. Con --bd
prueba también login, productos y crear comanda; necesita la base con los datos
de ejemplo y las contraseñas de desarrollo (Demo_123.).
"""
import json
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cliente_ws  # noqa: E402
from cliente_ws import CLOSE, TEXT, CONT, cerrar, enviar, recibir  # noqa: E402,F401

cliente_ws.PUERTO = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 9100
CON_BD = "--bd" in sys.argv


def conectar(origen=None):
    return cliente_ws.conectar(origen)


resultados = []


def caso(numero, nombre, condicion, detalle):
    resultados.append(condicion)
    marca = "PASA" if condicion else "FALLA"
    print(f"[{marca}] {numero}. {nombre}\n        {detalle}")


def codigo(r):
    return (r.get("error") or {}).get("codigo")


# Casos 1 a 6: un cliente sin Origin (como la app de Qt o Android)
s, error = conectar()
if s is None:
    print("No se pudo abrir el WebSocket:", error)
    sys.exit(1)

enviar(s, TEXT, b"{esto no es json")
r = recibir(s)
caso(1, "JSON roto", r.get("ok") is False and codigo(r) == "PETICION_INVALIDA" and r.get("id") is None, r)

enviar(s, TEXT, json.dumps({"id": 2, "datos": {}}).encode())
r = recibir(s)
caso(2, "JSON sin op", codigo(r) == "PETICION_INVALIDA" and r.get("id") == 2, r)

enviar(s, TEXT, json.dumps({"id": 3, "op": "OP_NO_EXISTE"}).encode())
r = recibir(s)
caso(3, "op inexistente", codigo(r) == "OP_DESCONOCIDA" and r.get("id") == 3, r)

enviar(s, TEXT, json.dumps({"id": 4, "op": "OP_LISTAR_PRODUCTOS"}).encode())
r = recibir(s)
caso(4, "Operación real sin sesión", codigo(r) == "NO_AUTENTICADO" and r.get("id") == 4, r)

enviar(s, TEXT, json.dumps({"id": "abc", "op": "OP_LOGIN", "datos": {"usuario": "x"}}).encode())
r = recibir(s)
caso(5, "Login sin contraseña", codigo(r) == "VALIDACION_FALLIDA" and r.get("id") == "abc", r)

mensaje = json.dumps({"id": 6, "op": "OP_LISTAR_PEDIDOS"}).encode()
tercio = len(mensaje) // 3
enviar(s, TEXT, mensaje[:tercio], final=False)
enviar(s, CONT, mensaje[tercio:2 * tercio], final=False)
enviar(s, CONT, mensaje[2 * tercio:], final=True)
r = recibir(s)
caso(6, "Mensaje partido en 3 tramas", codigo(r) == "NO_AUTENTICADO" and r.get("id") == 6, r)
cerrar(s)

# Casos 7 y 8: un navegador manda Origin
s, error = conectar("https://actronix09.github.io")
caso(7, "Navegador desde una página permitida", s is not None, "WebSocket abierto" if s else error)
if s:
    cerrar(s)

s, error = conectar("https://pagina-ajena.example")
caso(8, "Navegador desde una página NO permitida (403)", s is None and " 403 " in (error or "") + " ",
     error if s is None else "¡se abrió el WebSocket!")
if s:
    cerrar(s)



# ---------------------------------------------------------------------------
#  Con base de datos
# ---------------------------------------------------------------------------

def pedir(s, op, datos=None, id_=0):
    enviar(s, TEXT, json.dumps({"id": id_, "op": op, "datos": datos}).encode())
    return recibir(s)


def mensaje(r):
    return (r.get("error") or {}).get("mensaje", "")


if CON_BD:
    s, error = conectar()

    r = pedir(s, "OP_LOGIN", {"usuario": "lmendez", "contrasena": "Equivocada_1"})
    caso(9, "Login con contraseña incorrecta", codigo(r) == "CREDENCIALES_INVALIDAS", r)

    r2 = pedir(s, "OP_LOGIN", {"usuario": "noexiste", "contrasena": "Demo_123."})
    caso(10, "Login de usuario que no existe (mismo mensaje)",
         codigo(r2) == "CREDENCIALES_INVALIDAS" and mensaje(r2) == mensaje(r), r2)

    r = pedir(s, "OP_LOGIN", {"usuario": " LMendez ", "contrasena": "Demo_123."})
    perfil = r.get("datos") or {}
    caso(11, "Login de mesero (sin importar espacios ni mayúsculas)",
         r.get("ok") is True and perfil.get("rol") == "mesero" and "contrasena_hash" not in perfil, r)

    r = pedir(s, "OP_LISTAR_PRODUCTOS")
    productos = r.get("datos") or []
    caso(12, "Listar productos", r.get("ok") is True and len(productos) > 0
         and all(isinstance(p.get("precio"), str) for p in productos),
         f"{len(productos)} productos; el primero: {productos[0] if productos else None}")

    # Un producto que alcance para 2, y uno con receta que no alcance para 50
    alcanza = next(p for p in productos if p["disponible"] and p["maximo"] >= 2)
    escaso = next((p for p in productos if p["ingredientes"] and p["maximo"] < 50), None)

    r = pedir(s, "OP_CREAR_PEDIDO", {"numero_mesa": 1, "secuencial": 1, "lineas": [
        {"id_producto": alcanza["id_producto"], "cantidad": 2, "detalles": "  sin hielo  "}]})
    d = r.get("datos") or {}
    caso(13, f"Crear 2 comandas de «{alcanza['nombre']}» en la cuenta 1 de la mesa 1",
         r.get("ok") is True and d.get("creadas") == 2 and len(d.get("id_comandas", [])) == 2, r)

    r = pedir(s, "OP_CREAR_PEDIDO", {"numero_mesa": 1, "secuencial": 3, "lineas": [
        {"id_producto": alcanza["id_producto"], "cantidad": 1}]})
    caso(14, "Cuenta que es de otra mesa", codigo(r) == "VALIDACION_FALLIDA", r)

    r = pedir(s, "OP_CREAR_PEDIDO", {"numero_mesa": 99, "secuencial": 1, "lineas": [
        {"id_producto": alcanza["id_producto"], "cantidad": 1}]})
    caso(15, "Mesa que no existe", codigo(r) == "VALIDACION_FALLIDA" and "99" in mensaje(r), r)

    # Todo o nada: la primera línea es válida, la segunda no
    r = pedir(s, "OP_CREAR_PEDIDO", {"numero_mesa": 1, "secuencial": 1, "lineas": [
        {"id_producto": alcanza["id_producto"], "cantidad": 1}, {"id_producto": 99999, "cantidad": 1}]})
    caso(16, "Producto que no existe (no se guarda ninguna línea)",
         codigo(r) == "VALIDACION_FALLIDA" and "99999" in mensaje(r), r)

    r = pedir(s, "OP_CREAR_PEDIDO", {"numero_mesa": 1, "secuencial": 1, "lineas": [
        {"id_producto": alcanza["id_producto"], "cantidad": 0}]})
    caso(17, "Cantidad 0", codigo(r) == "VALIDACION_FALLIDA", r)

    if escaso:
        r = pedir(s, "OP_CREAR_PEDIDO", {"numero_mesa": 1, "secuencial": 1, "lineas": [
            {"id_producto": escaso["id_producto"], "cantidad": escaso["maximo"] + 1}]})
        caso(18, f"Más de lo que alcanza de «{escaso['nombre']}» ({escaso['maximo'] + 1})",
             codigo(r) == "SIN_STOCK", r)

    r = pedir(s, "OP_LOGOUT")
    r2 = pedir(s, "OP_LISTAR_PRODUCTOS")
    caso(19, "Después de logout ya no hay sesión", r.get("ok") is True and codigo(r2) == "NO_AUTENTICADO", r2)

    r = pedir(s, "OP_LOGIN", {"usuario": "rgamez", "contrasena": "Demo_123."})
    r2 = pedir(s, "OP_CREAR_PEDIDO", {"numero_mesa": 1, "secuencial": 1, "lineas": [
        {"id_producto": alcanza["id_producto"], "cantidad": 1}]})
    caso(20, "Cocina no puede crear comandas", r.get("ok") is True and codigo(r2) == "NO_AUTORIZADO", r2)
    cerrar(s)

print(f"\n{sum(resultados)} de {len(resultados)} casos pasan")
sys.exit(0 if all(resultados) else 1)
