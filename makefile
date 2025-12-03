# Makefile para Sistema de Comandas

CC = gcc
CFLAGS = -Wall -Wextra -g
LIBS = -lncurses -lssl -lcrypto

# Archivos objeto
OBJS = main.o ui.o usuario.o sesion.o

# Ejecutable
TARGET = sistema_comandas

# Regla principal
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LIBS)

main.o: main.c ui.h usuario.h sesion.h
	$(CC) $(CFLAGS) -c main.c

ui.o: ui.c ui.h
	$(CC) $(CFLAGS) -c ui.c

usuario.o: usuario.c usuario.h
	$(CC) $(CFLAGS) -c usuario.c

sesion.o: sesion.c sesion.h usuario.h ui.h
	$(CC) $(CFLAGS) -c sesion.c

# Limpiar archivos generados
clean:
	rm -f $(OBJS) $(TARGET) usuarios

# Reconstruir todo
rebuild: clean all

# Ejecutar el programa
run: $(TARGET)
	./$(TARGET)

.PHONY: all clean rebuild run