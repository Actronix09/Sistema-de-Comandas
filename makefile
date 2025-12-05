# Makefile para Sistema de Comandas

CC = gcc
CFLAGS = -Wall -g
LIBS_NCURSES = -lncurses
LIBS_CRYPTO = -lssl -lcrypto
LIBS_PTHREAD = -lpthread

# Archivos objeto comunes
OBJ_USUARIO = usuario.o
OBJ_PRODUCTOS = productos.o
OBJ_PEDIDOS = pedidos.o
OBJ_UI = ui.o
OBJ_LOG = logger.o

# Servidor
SERVIDOR = Servidor
SERVIDOR_SRC = Servidor.c
SERVIDOR_OBJ = $(SERVIDOR_SRC:.c=.o) logger.o

# Cliente
CLIENTE = Cliente
CLIENTE_SRC = Cliente.c
CLIENTE_OBJ = $(CLIENTE_SRC:.c=.o) interfaz_mesero.o interfaz_cocina.o

# Regla principal
all: $(SERVIDOR) $(CLIENTE)

# Compilar servidor
$(SERVIDOR): $(SERVIDOR_OBJ) $(OBJ_USUARIO) $(OBJ_PRODUCTOS) $(OBJ_PEDIDOS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS_PTHREAD) $(LIBS_CRYPTO)

# Compilar cliente
$(CLIENTE): $(CLIENTE_OBJ) $(OBJ_UI) $(OBJ_USUARIO) $(OBJ_PRODUCTOS) $(OBJ_PEDIDOS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS_NCURSES) $(LIBS_CRYPTO)

# Reglas para archivos objeto
%.o: %.c
	$(CC) $(CFLAGS) -c $<

usuario.o: usuario.c usuario.h
	$(CC) $(CFLAGS) -c usuario.c

productos.o: productos.c productos.h
	$(CC) $(CFLAGS) -c productos.c

pedidos.o: pedidos.c pedidos.h
	$(CC) $(CFLAGS) -c pedidos.c

ui.o: ui.c ui.h
	$(CC) $(CFLAGS) -c ui.c

interfaz_mesero.o: interfaz_mesero.c interfaz_mesero.h
	$(CC) $(CFLAGS) -c interfaz_mesero.c

interfaz_cocina.o: interfaz_cocina.c interfaz_cocina.h
	$(CC) $(CFLAGS) -c interfaz_cocina.c

logger.o: logger.c logger.h
	$(CC) $(CFLAGS) -c logger.c

# Limpiar archivos compilados
clean:
	rm -f *.o $(SERVIDOR) $(CLIENTE)
	rm -f servidor_usuarios_mem servidor_usuarios_sem servidor_estado_sem

# Limpiar todo incluyendo datos
cleanall: clean
	rm -f usuarios pedidos *.log

# Ejecutar servidor
run-servidor: $(SERVIDOR)
	./$(SERVIDOR)

# Ejecutar cliente
run-cliente: $(CLIENTE)
	./$(CLIENTE)

# Ayuda
help:
	@echo "Comandos disponibles:"
	@echo "  make              - Compilar servidor y cliente"
	@echo "  make clean        - Limpiar archivos compilados"
	@echo "  make cleanall     - Limpiar todo incluyendo datos"
	@echo "  make run-servidor - Compilar y ejecutar servidor"
	@echo "  make run-cliente  - Compilar y ejecutar cliente"

.PHONY: clean cleanall run-servidor run-cliente help