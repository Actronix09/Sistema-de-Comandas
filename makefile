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
OBJ_INVENTARIO = inventario.o
OBJ_UI = ui.o
OBJ_LOG = logger.o
OBJ_ADMIN = interfaz_admin.o
OBJ_RECUPERAR_PASS = recuperar_password.o

# Archivos memoria y semáforos
MEM = servidor_usuarios_mem
SEM1 = servidor_usuarios_sem
SEM2 = servidor_estado_sem

# Servidor
SERVIDOR = Servidor
SERVIDOR_SRC = Servidor.c
SERVIDOR_OBJ = Servidor.o logger.o

# Cliente
CLIENTE = Cliente
CLIENTE_SRC = Cliente.c
CLIENTE_OBJ = Cliente.o interfaces.o

# Regla principal
all: $(SERVIDOR) $(CLIENTE) $(MEM) $(SEM1) $(SEM2)

# Compilar servidor
$(SERVIDOR): $(SERVIDOR_OBJ) $(OBJ_USUARIO) $(OBJ_PRODUCTOS) $(OBJ_PEDIDOS) $(OBJ_INVENTARIO)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS_PTHREAD) $(LIBS_CRYPTO)

# Compilar cliente
$(CLIENTE): $(CLIENTE_OBJ) $(OBJ_UI) $(OBJ_USUARIO) $(OBJ_PRODUCTOS) $(OBJ_PEDIDOS) $(OBJ_INVENTARIO) $(OBJ_LOG) $(OBJ_RECUPERAR_PASS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS_NCURSES) $(LIBS_CRYPTO)

# Crear archivos de memoria compartida y semáforos
$(MEM) $(SEM1) $(SEM2):
	touch $@

# Compilación de archivos .c a .o
Servidor.o: Servidor.c
	$(CC) $(CFLAGS) -c Servidor.c

Cliente.o: Cliente.c
	$(CC) $(CFLAGS) -c Cliente.c

usuario.o: usuario.c usuario.h
	$(CC) $(CFLAGS) -c usuario.c

productos.o: productos.c productos.h
	$(CC) $(CFLAGS) -c productos.c

pedidos.o: pedidos.c pedidos.h
	$(CC) $(CFLAGS) -c pedidos.c

ui.o: ui.c ui.h
	$(CC) $(CFLAGS) -c ui.c

interfaces.o: interfaces.c interfaces.h
	$(CC) $(CFLAGS) -c interfaces.c

logger.o: logger.c logger.h
	$(CC) $(CFLAGS) -c logger.c


inventario.o: inventario.c inventario.h
	$(CC) $(CFLAGS) -c inventario.c

recuperar_password.o: recuperar_password.c recuperar_password.h
	$(CC) $(CFLAGS) -c recuperar_password.c

# Limpiar archivos compilados
clean:
	rm -f *.o $(SERVIDOR) $(CLIENTE)
	rm -f $(SEM1) $(SEM2) $(MEM)

# Limpiar todo incluyendo datos
cleanall: clean
	rm -f usuarios pedidos inventario producto_ingrediente *.log

datauser:
	# Datos de productos
	@echo "Creando archivos de datos iniciales..."
	@echo "Administrador|admin|0192023a7bbd73250516f069df18b500|admin@test.com|12345678|2" > usuarios
	@echo "CocinaTEST|cocina|81dc9bdb52d04dc20036dbd8313ed055|cocina@test.com|12345678|1" >> usuarios
	@echo "MeseroTEST|mesero|81dc9bdb52d04dc20036dbd8313ed055|mesero@test.com|12345678|0" >> usuarios
	@echo "Archivos de usuarios creados"
	@echo "Usuarios creados:"
	@echo "  - Administrador: Usuario: admin | contraseña: admin123"
	@echo "  - CocinaTEST: Usuario: cocina | contraseña: 1234"
	@echo "  - MeseroTEST: Usuario: mesero | contraseña: 1234"

datamenu:
	# Datos de productos
	@echo "1|Producto 1|Descripcion del producto 1|123.45" > productos
	@echo "2|Producto 2|Descripcion del producto 2|456.90" >> productos
	@echo "Datos de productos creados"
	# Datos de inventario
	@echo "1|IngredienteA|50" > inventario
	@echo "2|IngredienteB|30" >> inventario
	@echo "Datos de inventario creados"
	# Datos de relaciones producto - ingredientes
	@echo "1|1|2" > producto_ingrediente  # Producto 1 necesita 2 unidades de IngredienteA
	@echo "1|2|1" >> producto_ingrediente  # Producto 1 necesita 1 unidades de IngredienteB
	@echo	"2|2|3" >> producto_ingrediente	# Producto 2 necesita 3 unidades de IngredienteB
	@echo "Datos productos-ingredientes creado"

# Crear datos iniciales de productos y usuarios.
data: datauser datamenu

# Limpiar y crear datos iniciales
fresh: cleanall all data

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
	@echo "  make data         - Crea los datos iniciales de usuarios y productos"
	@echo "  make datauser     - Crea los datos iniciales de usuarios"
	@echo "  make datamenu     - Crea los datos iniciales de productos"
	@echo	"  make fresh        - Borra todos los datos y genera datos iniciales, y recompila el sistema"
	@echo "  make run-servidor - Compilar y ejecutar servidor"
	@echo "  make run-cliente  - Compilar y ejecutar cliente"

.PHONY: all clean cleanall data datauser datamenu fresh run-servidor run-cliente help
