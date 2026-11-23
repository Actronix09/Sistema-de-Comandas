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

OBJETOS = main.o conexion.o despacho.o operaciones.o bd.o respuesta.o registro.o

servidor_comandas: $(OBJETOS)
	$(CC) $(LDFLAGS) -o $@ $(OBJETOS) $(LDLIBS)

main.o: main.c bd.h conexion.h operaciones.h registro.h
conexion.o: conexion.c conexion.h cliente.h despacho.h registro.h
despacho.o: despacho.c despacho.h cliente.h operaciones.h respuesta.h registro.h ../comun/protocolo.h
operaciones.o: operaciones.c operaciones.h cliente.h bd.h respuesta.h registro.h
bd.o: bd.c bd.h respuesta.h registro.h
respuesta.o: respuesta.c respuesta.h
registro.o: registro.c registro.h

.PHONY: limpiar
limpiar:
	rm -f $(OBJETOS) servidor_comandas
