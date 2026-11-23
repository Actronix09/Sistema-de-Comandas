"""Prueba el servidor en C con un cliente WebSocket mínimo (solo stdlib).

Uso: python3 -I probar.py <puerto> [--bd]

Sin --bd prueba solo la conexión y el protocolo, sin base de datos. Con --bd
prueba también login, productos y crear comanda; necesita la base con los datos
de ejemplo y las contraseñas de desarrollo (Demo_123.).
"""
import base64
import json
import os
import socket
import struct
import sys

HOST = "127.0.0.1"
PUERTO = int(sys.argv[1]) if len(sys.argv) > 1 and sys.argv[1].isdigit() else 9100
CON_BD = "--bd" in sys.argv

TEXT, CONT, CLOSE = 0x1, 0x0, 0x8


def conectar(origen=None):
    """Hace el handshake. Regresa (socket, None) si abre o (None, primera línea) si no."""
    s = socket.create_connection((HOST, PUERTO), timeout=5)
    clave = base64.b64encode(os.urandom(16)).decode()
    cabeceras = [
        "GET / HTTP/1.1",
        f"Host: {HOST}:{PUERTO}",
        "Upgrade: websocket",
        "Connection: Upgrade",
        f"Sec-WebSocket-Key: {clave}",
        "Sec-WebSocket-Version: 13",
    ]
    if origen:
        cabeceras.append(f"Origin: {origen}")
    s.sendall(("\r\n".join(cabeceras) + "\r\n\r\n").encode())

    datos = b""
    try:
        while b"\r\n\r\n" not in datos:
            trozo = s.recv(4096)
            if not trozo:
                break
            datos += trozo
    except (socket.timeout, ConnectionResetError):
        pass
    primera = datos.split(b"\r\n", 1)[0].decode(errors="replace")
    if " 101 " in primera + " ":
        return s, None
    s.close()
    return None, primera or "(el servidor cerró sin responder)"


def enviar(s, opcode, carga, final=True):
    cabeza = bytes([(0x80 if final else 0) | opcode])
    n = len(carga)
    if n < 126:
        cabeza += bytes([0x80 | n])
    elif n < 65536:
        cabeza += bytes([0x80 | 126]) + struct.pack(">H", n)
    else:
        cabeza += bytes([0x80 | 127]) + struct.pack(">Q", n)
    mascara = os.urandom(4)
    s.sendall(cabeza + mascara + bytes(b ^ mascara[i % 4] for i, b in enumerate(carga)))


def leer_exacto(s, n):
    datos = b""
    while len(datos) < n:
        trozo = s.recv(n - len(datos))
        if not trozo:
            raise ConnectionError("el servidor cerró la conexión")
        datos += trozo
    return datos


def recibir(s):
    """Lee tramas hasta un mensaje de texto completo (ignora ping/pong)."""
    partes = b""
    while True:
        b0, b1 = leer_exacto(s, 2)
        opcode, final = b0 & 0x0F, b0 & 0x80
        n = b1 & 0x7F
        if n == 126:
            n = struct.unpack(">H", leer_exacto(s, 2))[0]
        elif n == 127:
            n = struct.unpack(">Q", leer_exacto(s, 8))[0]
        carga = leer_exacto(s, n)
        if opcode == CLOSE:
            raise ConnectionError("el servidor mandó CLOSE")
        if opcode in (TEXT, CONT):
            partes += carga
            if final:
                return json.loads(partes)


def cerrar(s):
    try:
        enviar(s, CLOSE, struct.pack(">H", 1000))
    except OSError:
        pass
    s.close()


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
