#ifndef CLIENTE_H
#define CLIENTE_H

#include <stddef.h>

struct mg_connection;

// Quién inició sesión en esta conexión. En Servidor.c esto vivía en la memoria
// compartida; aquí cada conexión WebSocket tiene la suya.
typedef struct {
    int activa;
    int id_usuario;
    char usuario[64];
    char rol[16];          // mesero, bar_cocina o cajero_admin
    char nivel_acceso[16]; // cajero o administrador; vacío para los demás roles
} Sesion;

// Todo lo que el servidor guarda de una conexión. Lo crea al_estar_listo() y
// lo libera al_cerrar(), en conexion.c.
typedef struct Cliente {
    struct mg_connection *conexion;
    char ip[48];
    Sesion sesion;

    // Un mensaje puede llegar partido en varias tramas: se juntan aquí
    char *mensaje;
    size_t largo;
    int recibiendo_texto;

    // Lista de conexiones abiertas, para los avisos (ver avisos.h)
    struct Cliente *siguiente;
} Cliente;

#endif
