"""Cliente WebSocket mínimo (solo stdlib) para las pruebas del servidor."""
import base64
import json
import os
import socket
import struct

HOST = "127.0.0.1"
PUERTO = 9100

TEXT, CONT, CLOSE = 0x1, 0x0, 0x8


def conectar(origen=None):
    """Hace el handshake. Regresa (socket, None) si abre o (None, primera línea) si no."""
    s = socket.create_connection((HOST, PUERTO), timeout=10)
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


# Avisos que llegaron mientras se esperaba una respuesta: {"aviso": ...}
avisos = []


def recibir(s, con_avisos=False):
    """Lee tramas hasta un mensaje de texto completo (ignora ping/pong).

    Los avisos del servidor no son respuestas: se guardan en "avisos" y se
    sigue esperando, salvo con con_avisos=True.
    """
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
                mensaje = json.loads(partes)
                partes = b""
                if "aviso" in mensaje and not con_avisos:
                    avisos.append(mensaje)
                    continue
                return mensaje


def cerrar(s):
    try:
        enviar(s, CLOSE, struct.pack(">H", 1000))
    except OSError:
        pass
    s.close()


