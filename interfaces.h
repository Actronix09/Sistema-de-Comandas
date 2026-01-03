#ifndef INTERFACES_H
#define INTERFACES_H

#include "usuario.h"
#include "productos.h"
#include "inventario.h"
#include "protocolo.h"

// Funciones de interfaz de administrador
void interfaz_admin_ejecutar(USUARIO *usuario_logueado);
void mostrar_menu_admin();
void mostrar_menu_usuarios();
void mostrar_menu_productos();
void mostrar_menu_ingredientes();
void mostrar_menu_logs();
void mostrar_logs(const char* nombre_archivo);
void inicializar_ncurses();
void finalizar_ncurses();
int mostrar_menu_principal();
void dibujar_botones_si_no(int centro_x, int boton_y, int seleccion, int ancho_boton, int alto_boton);
void dibujar_botones_confirmacion(int centro_x, int boton_y, int seleccion, int ancho_boton, int alto_boton, char *texto_opcion1, char *texto_opcion2);
void dibujar_recuadro_usuario_horizontal(int x, int y, int ancho, USUARIO *usr, int usr_index, int seleccionado);
void dibujar_recuadro_nuevo_usuario_horizontal(int x, int y, int ancho, int seleccionado);
void calcular_layout_productos(int num_productos, int *cols, int *filas, int *ancho, int *alto);
void dibujar_recuadro_producto_admin(int x, int y, int ancho, int alto, PRODUCTO *prod, int seleccionado);
void dibujar_recuadro_ingrediente_admin(int x, int y, int ancho, int alto, Ingrediente *ing, int seleccionado);

// Funciones de interfaz de cocina
void interfaz_cocina_ejecutar(USUARIO *usuario_logueado);
int enviar_peticion_cocina(Peticion *pet, Respuesta *resp);
void dibujar_recuadro_pedido_horizontal(int x, int y, int ancho, PedidoMsg *pedido, int seleccionado);

// Funciones de interfaz de mesero
void interfaz_mesero_ejecutar(USUARIO *usuario_logueado);
int enviar_peticion_mesero(Peticion *pet, Respuesta *resp);
void calcular_layout(int num_productos, int *cols, int *filas, int *ancho, int *alto);
int verificar_disponibilidad_producto(int id_producto);
void dibujar_recuadro_producto(int x, int y, int ancho, int alto, ProductoMsg *prod, int seleccionado, int disponible);

#endif