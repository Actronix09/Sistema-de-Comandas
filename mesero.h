#ifndef MESERO_H
#define MESERO_H

#include "usuario.h"

// Mostrar interfaz principal del mesero
void mesero_show_interface(USUARIO* usuario);

// Funciones de las opciones del menú
void mesero_tomar_pedido(USUARIO* mesero);
void mesero_procesar_pago(USUARIO* mesero);

#endif