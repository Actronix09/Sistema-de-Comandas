# Servidor del Sistema de Comandas en C
#
#   make            compila servidor_comandas
#   make limpiar    borra lo compilado
#
# Necesita las bibliotecas de desarrollo de civetweb, cJSON, libpq y libsodium.
# El Containerfile ya las instala; no hace falta tenerlas en el equipo.

CC      ?= cc
CFLAGS  ?= -O2 -g
CFLAGS  += -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Wpedantic -I../comun \
           $(shell pkg-config --cflags libcjson libpq libsodium)
LDLIBS  += -lcivetweb $(shell pkg-config --libs libcjson libpq libsodium) -lpthread

OBJETOS = main.o conexion.o despacho.o avisos.o datos.o bd.o respuesta.o registro.o \
          operaciones.o

servidor_comandas: $(OBJETOS)
	$(CC) $(LDFLAGS) -o $@ $(OBJETOS) $(LDLIBS)

# La regla de la contraseña es la misma del cliente: comun/validaciones.c
validaciones.o: ../comun/validaciones.c ../comun/validaciones.h
	$(CC) $(CFLAGS) -c -o $@ $<

OPERACION = operaciones.h cliente.h avisos.h bd.h datos.h respuesta.h registro.h
main.o: main.c bd.h conexion.h operaciones.h registro.h
conexion.o: conexion.c conexion.h avisos.h cliente.h despacho.h registro.h
despacho.o: despacho.c despacho.h cliente.h operaciones.h respuesta.h registro.h ../comun/protocolo.h
avisos.o: avisos.c avisos.h cliente.h
datos.o: datos.c datos.h cliente.h respuesta.h
operaciones.o: operaciones.c $(OPERACION)
op_salon.o: op_salon.c $(OPERACION)
op_comandas.o: op_comandas.c $(OPERACION)
op_inventario.o: op_inventario.c $(OPERACION)
op_usuarios.o: op_usuarios.c $(OPERACION) ../comun/validaciones.h
bd.o: bd.c bd.h respuesta.h registro.h
respuesta.o: respuesta.c respuesta.h
registro.o: registro.c registro.h

.PHONY: limpiar
limpiar:
	rm -f $(OBJETOS) servidor_comandas
