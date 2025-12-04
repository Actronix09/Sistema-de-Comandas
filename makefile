# Makefile para Sistema de Comandas

CC = gcc
CFLAGS = -Wall -Wextra -g
LIBS = -lncurses -lssl -lcrypto


OBJS = main.o ui.o usuario.o sesion.o mesero.o


TARGET = sistema_comandas


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


mesero.o: mesero.c mesero.h usuario.h ui.h
	$(CC) $(CFLAGS) -c mesero.c


clean:
	rm -f $(OBJS) $(TARGET) usuarios


rebuild: clean all


run: $(TARGET)
	./$(TARGET)

.PHONY: all clean rebuild run