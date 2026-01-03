#include "logger.h"
#include "productos.h"
#include "inventario.h"
#include "ui.h"
#include "usuario.h"
#include "protocolo.h"
#include "interfaces.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ncurses.h>
#include <openssl/evp.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

#define PERMISOS 0644
#define MAX_CLIENTES 10

// Estructura de datos compartidos
typedef struct {
    int cliente_conectado;
    int peticion_lista;
    int respuesta_lista;
    Peticion peticion;
    Respuesta respuesta;
    int cliente_id;
    int hilo_asignado;
} DatosCliente;

typedef struct {
    int servidor_activo;
    int num_clientes_activos;
    DatosCliente clientes[MAX_CLIENTES];
} DatosCompartidos;

// Variables externas
extern DatosCompartidos *datos_servidor;
extern int semid_servidor;
extern int mi_slot;

// Funciones externas de comunicación con servidor
// (definidas en Cliente.c)
extern void down(int semid);
extern void up(int semid);
extern int enviar_peticion(Peticion *pet, Respuesta *resp);

// Funciones de la interfaz de administrador

// Función para dibujar botones de selección Sí/No
void dibujar_botones_si_no(int centro_x, int boton_y, int seleccion, int ancho_boton, int alto_boton) {
    dibujar_botones_confirmacion(centro_x, boton_y, seleccion, ancho_boton, alto_boton, "SI", "NO");
}

// Función para dibujar botones de confirmación generales
void dibujar_botones_confirmacion(int centro_x, int boton_y, int seleccion, int ancho_boton, int alto_boton, char *texto_opcion1, char *texto_opcion2) {
    int boton_x_op1 = centro_x;
    int boton_x_op2 = centro_x + 20; // Separación entre botones

    // Botón opción 1
    int color_op1 = (seleccion == 0) ? 5 : 10;
    attron(COLOR_PAIR(color_op1) | (seleccion == 0 ? A_BOLD : 0));

    // Borde superior
    mvaddch(boton_y, boton_x_op1, '+');
    for(int i = 1; i < ancho_boton-1; i++) mvaddch(boton_y, boton_x_op1+i, '=');
    mvaddch(boton_y, boton_x_op1+ancho_boton-1, '+');

    // Lados
    for(int j = 1; j < alto_boton-1; j++) {
        mvaddch(boton_y+j, boton_x_op1, '|');
        mvaddch(boton_y+j, boton_x_op1+ancho_boton-1, '|');
    }

    // Texto en botón opción 1
    int padding = (ancho_boton - strlen(texto_opcion1) - 2) / 2;
    if(padding < 0) padding = 0;
    mvprintw(boton_y+1, boton_x_op1+padding+1, "%s", texto_opcion1);

    // Borde inferior
    mvaddch(boton_y+alto_boton-1, boton_x_op1, '+');
    for(int i = 1; i < ancho_boton-1; i++) mvaddch(boton_y+alto_boton-1, boton_x_op1+i, '=');
    mvaddch(boton_y+alto_boton-1, boton_x_op1+ancho_boton-1, '+');

    attroff(COLOR_PAIR(color_op1) | (seleccion == 0 ? A_BOLD : 0));

    // Botón opción 2
    int color_op2 = (seleccion == 1) ? 5 : 10;
    attron(COLOR_PAIR(color_op2) | (seleccion == 1 ? A_BOLD : 0));

    // Borde superior
    mvaddch(boton_y, boton_x_op2, '+');
    for(int i = 1; i < ancho_boton-1; i++) mvaddch(boton_y, boton_x_op2+i, '=');
    mvaddch(boton_y, boton_x_op2+ancho_boton-1, '+');

    // Lados
    for(int j = 1; j < alto_boton-1; j++) {
        mvaddch(boton_y+j, boton_x_op2, '|');
        mvaddch(boton_y+j, boton_x_op2+ancho_boton-1, '|');
    }

    // Texto en botón opción 2
    padding = (ancho_boton - strlen(texto_opcion2) - 2) / 2;
    if(padding < 0) padding = 0;
    mvprintw(boton_y+1, boton_x_op2+padding+1, "%s", texto_opcion2);

    // Borde inferior
    mvaddch(boton_y+alto_boton-1, boton_x_op2, '+');
    for(int i = 1; i < ancho_boton-1; i++) mvaddch(boton_y+alto_boton-1, boton_x_op2+i, '=');
    mvaddch(boton_y+alto_boton-1, boton_x_op2+ancho_boton-1, '+');

    attroff(COLOR_PAIR(color_op2) | (seleccion == 1 ? A_BOLD : 0));
}

// Función para dibujar recuadro de usuario similar al de cocina
void dibujar_recuadro_usuario_horizontal(int x, int y, int ancho, USUARIO *usr, int usr_index, int seleccionado) {
    int color_borde = seleccionado ? 5 : 10;

    attron(COLOR_PAIR(color_borde) | (seleccionado ? A_BOLD : 0));

    // Borde superior
    mvaddch(y, x, '+');
    for(int i = 1; i < ancho-1; i++) mvaddch(y, x+i, '=');
    mvaddch(y, x+ancho-1, '+');

    // Primera línea de contenido
    mvaddch(y+1, x, '|');
    mvaddch(y+1, x+ancho-1, '|');

    // Segunda línea de contenido
    mvaddch(y+2, x, '|');
    mvaddch(y+2, x+ancho-1, '|');

    // Tercera línea de contenido
    mvaddch(y+3, x, '|');
    mvaddch(y+3, x+ancho-1, '|');

    // Cuarta línea de contenido
    mvaddch(y+4, x, '|');
    mvaddch(y+4, x+ancho-1, '|');

    // Borde inferior
    mvaddch(y+5, x, '+');
    for(int i = 1; i < ancho-1; i++) mvaddch(y+5, x+i, '=');
    mvaddch(y+5, x+ancho-1, '+');

    attroff(COLOR_PAIR(color_borde) | (seleccionado ? A_BOLD : 0));

    if(usr) {
        // Línea 1: ID de usuario y nombre
        attron(COLOR_PAIR(10) | A_BOLD);
        mvprintw(y+1, x+2, "USUARIO #%d", usr_index + 1); // Mostrar el número de usuario en la lista
        mvprintw(y+1, x+15, "Nombre: %s", usr->name);
        attroff(COLOR_PAIR(10) | A_BOLD);

        // Línea 2: Usuario y tipo
        attron(COLOR_PAIR(10));
        char info_line[256];
        snprintf(info_line, 256, "Usuario: %s | Tipo: ", usr->user);

        // Agregar descripción del tipo - Corregido según la definición correcta: 0=mesero, 1=cocina, 2=administrador
        if(usr->tipo == 0) strcat(info_line, "Mesero");
        else if(usr->tipo == 1) strcat(info_line, "Cocina");
        else if(usr->tipo == 2) strcat(info_line, "Administrador");
        else strcat(info_line, "Desconocido"); // En caso de un tipo no válido

        // Truncar si es muy largo
        if(strlen(info_line) > ancho-4) {
            info_line[ancho-4] = '\0';
        }
        mvprintw(y+2, x+2, "%s", info_line);

        // Línea 3: Correo
        char mail_line[256];
        snprintf(mail_line, 256, "Correo: %s", usr->mail);
        if(strlen(mail_line) > ancho-4) {
            mail_line[ancho-4] = '\0';
        }
        mvprintw(y+3, x+2, "%s", mail_line);

        // Línea 4: Teléfono
        char telf_line[256];
        snprintf(telf_line, 256, "Telefono: %s", usr->telf);
        if(strlen(telf_line) > ancho-4) {
            telf_line[ancho-4] = '\0';
        }
        mvprintw(y+4, x+2, "%s", telf_line);

        attroff(COLOR_PAIR(10));
    } else {
        // Si usr es NULL, mostramos la tarjeta para nuevo usuario
        attron(COLOR_PAIR(10) | A_BOLD);
        mvprintw(y+2, x+10, "NUEVO USUARIO");
        mvprintw(y+3, x+5, "Presione Enter para agregar");
        attroff(COLOR_PAIR(10) | A_BOLD);
    }
}

// Función para dibujar recuadro de nuevo usuario
void dibujar_recuadro_nuevo_usuario_horizontal(int x, int y, int ancho, int seleccionado) {
    int color_borde = seleccionado ? 5 : 10;

    attron(COLOR_PAIR(color_borde) | (seleccionado ? A_BOLD : 0));

    // Borde superior
    mvaddch(y, x, '+');
    for(int i = 1; i < ancho-1; i++) mvaddch(y, x+i, '=');
    mvaddch(y, x+ancho-1, '+');

    // Primera línea de contenido
    mvaddch(y+1, x, '|');
    mvaddch(y+1, x+ancho-1, '|');

    // Segunda línea de contenido
    mvaddch(y+2, x, '|');
    mvaddch(y+2, x+ancho-1, '|');

    // Tercera línea de contenido
    mvaddch(y+3, x, '|');
    mvaddch(y+3, x+ancho-1, '|');

    // Cuarta línea de contenido
    mvaddch(y+4, x, '|');
    mvaddch(y+4, x+ancho-1, '|');

    // Borde inferior
    mvaddch(y+5, x, '+');
    for(int i = 1; i < ancho-1; i++) mvaddch(y+5, x+i, '=');
    mvaddch(y+5, x+ancho-1, '+');

    attroff(COLOR_PAIR(color_borde) | (seleccionado ? A_BOLD : 0));

    attron(COLOR_PAIR(10) | A_BOLD);
    mvprintw(y+1, x+2, "NUEVO USUARIO");
    mvprintw(y+1, x+20, "Presione Enter para agregar");
    attroff(COLOR_PAIR(10) | A_BOLD);
}

void inicializar_ncurses() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
}

void finalizar_ncurses() {
    endwin();
}

int mostrar_menu_principal() {
    int opcion = 0;
    int tecla;
    int seleccionado = 0;
    int num_opciones = 5;

    char *opciones[] = {
        "Administrador de usuarios",
        "Administrador de productos",
        "Administrador de ingredientes",
        "Ver logs",
        "Salir"
    };

    while(1) {
        clear();

        // Título del menú
        attron(A_BOLD);
        mvprintw(2, (COLS - 20) / 2, "MENU ADMINISTRADOR");
        attroff(A_BOLD);

        // Mostrar opciones
        for(int i = 0; i < num_opciones; i++) {
            if(i == seleccionado) {
                attron(A_REVERSE);
                mvprintw(6 + i * 2, (COLS - strlen(opciones[i])) / 2, "%s", opciones[i]);
                attroff(A_REVERSE);
            } else {
                mvprintw(6 + i * 2, (COLS - strlen(opciones[i])) / 2, "%s", opciones[i]);
            }
        }

        mvprintw(LINES - 3, 1, "Use las flechas para navegar y Enter para seleccionar");

        refresh();

        tecla = getch();

        switch(tecla) {
            case KEY_UP:
                seleccionado = (seleccionado - 1 + num_opciones) % num_opciones;
                break;
            case KEY_DOWN:
                seleccionado = (seleccionado + 1) % num_opciones;
                break;
            case 10: // Enter
                opcion = seleccionado + 1;
                switch(opcion) {
                    case 1:
                        mostrar_menu_usuarios();
                        break;
                    case 2:
                        mostrar_menu_productos();
                        break;
                    case 3:
                        mostrar_menu_ingredientes();
                        break;
                    case 4:
                        mostrar_menu_logs();
                        break;
                    case 5:
                        return 0; // Salir
                }
                break;
            case 27: // Tecla Escape
                return 0; // Salir
        }
    }

    return opcion;
}

// Calcular layout dinámico para productos
void calcular_layout_productos(int num_productos, int *cols, int *filas, int *ancho, int *alto) {
    int espacio_util_x = COLS - 4;

    // Dimensiones minimas y maximas para los recuadros
    int ancho_min = 30;
    int ancho_max = 50;
    int alto_fijo = 8;

    // Calcular cuántas columnas caben
    int max_cols = espacio_util_x / (ancho_min + 2);
    if(max_cols < 1) max_cols = 1;
    if(max_cols > 4) max_cols = 4;

    // Calcular ancho optimo para las columnas
    int ancho_calculado = (espacio_util_x - (max_cols - 1) * 2) / max_cols;
    if(ancho_calculado > ancho_max) {
        ancho_calculado = ancho_max;
        // Recalcular columnas con ancho maximo
        max_cols = espacio_util_x / (ancho_max + 2);
        if(max_cols < 1) max_cols = 1;
    }

    // Calcular filas necesarias (añadir 1 para la tarjeta "Agregar nuevo producto")
    int filas_necesarias = (num_productos + 1 + max_cols - 1) / max_cols;

    *cols = max_cols;
    *filas = filas_necesarias;
    *ancho = ancho_calculado;
    *alto = alto_fijo;
}

// Dibujar recuadro de producto para administración
void dibujar_recuadro_producto_admin(int x, int y, int ancho, int alto, PRODUCTO *prod, int seleccionado) {
    int color = seleccionado ? 5 : 10;

    attron(COLOR_PAIR(color) | (seleccionado ? A_BOLD : 0));

    // Borde superior
    mvaddch(y, x, '+');
    for(int i = 1; i < ancho-1; i++) mvaddch(y, x+i, '=');
    mvaddch(y, x+ancho-1, '+');

    // Contenido
    for(int i = 1; i < alto-1; i++) {
        mvaddch(y+i, x, '|');
        mvaddch(y+i, x+ancho-1, '|');
    }

    // Borde inferior
    mvaddch(y+alto-1, x, '+');
    for(int i = 1; i < ancho-1; i++) mvaddch(y+alto-1, x+i, '=');
    mvaddch(y+alto-1, x+ancho-1, '+');

    attroff(COLOR_PAIR(color) | (seleccionado ? A_BOLD : 0));

    // Contenido del producto
    if(prod) {
        int ancho_texto = ancho - 4;

        attron(COLOR_PAIR(10) | A_BOLD);
        mvprintw(y+1, x+2, "ID: %d", prod->id);
        mvprintw(y+2, x+2, "%s", prod->nombre);
        attron(COLOR_PAIR(10));

        // Mostrar descripción truncada si es muy larga
        char desc_ajustada[100];
        strncpy(desc_ajustada, prod->descripcion, ancho_texto-1);
        desc_ajustada[ancho_texto-1] = '\0'; // Asegurar terminación
        mvprintw(y+3, x+2, "%s", desc_ajustada);

        attron(A_BOLD);
        mvprintw(y+4, x+2, "$ %.2f", prod->precio);
        mvprintw(y+5, x+2, "Ingredientes: %d", prod->num_ingredientes);
        attroff(A_BOLD | COLOR_PAIR(10));
    } else {
        // Si es NULL, mostramos la tarjeta para nuevo producto
        attron(COLOR_PAIR(10) | A_BOLD);
        mvprintw(y+1, x+2, "NUEVO PRODUCTO");
        mvprintw(y+2, x+2, "Presione Enter para agregar");
        mvprintw(y+3, x+2, "Nombre: ---");
        mvprintw(y+4, x+2, "Descripcion: ---");
        mvprintw(y+5, x+2, "Precio: ---");
        attroff(COLOR_PAIR(10) | A_BOLD);
    }
}

// Dibujar recuadro de ingrediente para administración
void dibujar_recuadro_ingrediente_admin(int x, int y, int ancho, int alto, Ingrediente *ing, int seleccionado) {
    int color = seleccionado ? 5 : 10;

    attron(COLOR_PAIR(color) | (seleccionado ? A_BOLD : 0));

    // Borde superior
    mvaddch(y, x, '+');
    for(int i = 1; i < ancho-1; i++) mvaddch(y, x+i, '=');
    mvaddch(y, x+ancho-1, '+');

    // Contenido
    for(int i = 1; i < alto-1; i++) {
        mvaddch(y+i, x, '|');
        mvaddch(y+i, x+ancho-1, '|');
    }

    // Borde inferior
    mvaddch(y+alto-1, x, '+');
    for(int i = 1; i < ancho-1; i++) mvaddch(y+alto-1, x+i, '=');
    mvaddch(y+alto-1, x+ancho-1, '+');

    attroff(COLOR_PAIR(color) | (seleccionado ? A_BOLD : 0));

    // Contenido del ingrediente
    if(ing) {
        attron(COLOR_PAIR(10) | A_BOLD);
        mvprintw(y+1, x+2, "ID: %d", ing->id);
        mvprintw(y+2, x+2, "%s", ing->nombre);
        attron(COLOR_PAIR(10));

        mvprintw(y+3, x+2, "Cantidad: %d", ing->cantidad);
        mvprintw(y+4, x+2, "           ");
        mvprintw(y+5, x+2, "           ");
        attroff(COLOR_PAIR(10));
    } else {
        // Si es NULL, mostramos la tarjeta para nuevo ingrediente
        attron(COLOR_PAIR(10) | A_BOLD);
        mvprintw(y+1, x+2, "NUEVO INGREDIENTE");
        mvprintw(y+2, x+2, "Presione Enter para agregar");
        mvprintw(y+3, x+2, "Nombre: ---");
        mvprintw(y+4, x+2, "Cantidad: ---");
        mvprintw(y+5, x+2, "           ");
        attroff(COLOR_PAIR(10) | A_BOLD);
    }
}

void mostrar_menu_productos() {
    productos_cargar(); // Cargar productos desde archivo
    inventario_cargar(); // Cargar inventario para ver ingredientes

    int seleccionado = 0;
    int tecla;

    while(1) {
        // Actualizar el número de productos en cada iteración del bucle
        int num_productos = productos_get_total();
        ui_clear_screen();
        ui_print_title("ADMINISTRAR PRODUCTOS");

        // Calcular layout
        int columnas, filas, ancho_rec, alto_rec;
        calcular_layout_productos(num_productos, &columnas, &filas, &ancho_rec, &alto_rec);

        int margen_x = 2;
        int margen_y = 6;
        int espacio_x = 2;
        int espacio_y = 1;

        // Calcular desplazamiento para mostrar productos
        int filas_visibles = (LINES - 12) / (alto_rec + espacio_y);
        if(filas_visibles < 1) filas_visibles = 1;

        // Mostrar productos en forma de rejilla
        for(int i = 0; i < num_productos + 1; i++) { // +1 para la tarjeta de nuevo producto
            int fila = i / columnas;
            int columna = i % columnas;
            int x = margen_x + columna * (ancho_rec + espacio_x);
            int y = margen_y + fila * (alto_rec + espacio_y);

            if(i == 0) {
                // Primera tarjeta: "Agregar nuevo producto"
                dibujar_recuadro_producto_admin(x, y, ancho_rec, alto_rec, NULL, i == seleccionado);
            } else {
                // Productos existentes
                PRODUCTO *prod = productos_get_by_index(i - 1); // -1 porque el indice 0 es para nuevo producto
                if(prod != NULL) {
                    dibujar_recuadro_producto_admin(x, y, ancho_rec, alto_rec, prod, i == seleccionado);
                }
            }
        }

        ui_print_footer("Use flechas para navegar, ENTER para seleccionar, ESC para salir");
        refresh();

        tecla = getch();

        switch(tecla) {
            case KEY_UP:
                if(seleccionado >= columnas)
                    seleccionado -= columnas;
                break;
            case KEY_DOWN:
                if(seleccionado + columnas < num_productos + 1)
                    seleccionado += columnas;
                break;
            case KEY_LEFT:
                if(seleccionado > 0)
                    seleccionado--;
                break;
            case KEY_RIGHT:
                if(seleccionado < num_productos)
                    seleccionado++;
                break;
            case 10: // Enter
                if(seleccionado == 0) {
                    // Agregar nuevo producto
                    char nombre[MAX_NOMBRE_PRODUCTO];
                    char descripcion[MAX_DESCRIPCION];
                    char precio_str[20];
                    float precio;

                    ui_clear_screen();
                    ui_print_title("Agregar Nuevo Producto");

                    attron(COLOR_PAIR(10) | A_BOLD);
                    mvprintw(6, 5, "Nombre:");
                    mvprintw(8, 5, "Descripcion:");
                    mvprintw(10, 5, "Precio:");
                    attroff(COLOR_PAIR(10) | A_BOLD);

                    ui_read_input(6, 14, nombre, MAX_NOMBRE_PRODUCTO, 0);
                    if(strlen(nombre) == 0) break;

                    ui_read_input(8, 18, descripcion, MAX_DESCRIPCION, 0);
                    if(strlen(descripcion) == 0) break;

                    ui_read_input(10, 14, precio_str, sizeof(precio_str), 0);
                    precio = atof(precio_str);

                    // Agregar producto
                    if(productos_agregar(nombre, descripcion, precio) == 0) {
                        productos_guardar(); // Guardar en archivo
                        ui_show_success("Producto agregado correctamente.");
                        // Continuar al siguiente ciclo del bucle para recargar la lista
                    } else {
                        ui_show_error("Error al agregar producto.");
                    }
                } else {
                    // Editar producto existente
                    PRODUCTO *prod = productos_get_by_index(seleccionado - 1); // -1 porque el 0 es nuevo producto
                    if(prod != NULL) {
                        // Mostrar menú para editar producto
                        int opcion_seleccionada = 0;
                        int num_opciones = 4; // Editar producto, Editar ingredientes, Eliminar producto, Volver

                        while(1) {
                            clear();
                            attron(A_BOLD);
                            mvprintw(2, (COLS - 30) / 2, "OPCIONES PRODUCTO: %s", prod->nombre);
                            attroff(A_BOLD);

                            char *opciones[] = {
                                "Editar producto",      // Cambia nombre, descripción y precio
                                "Editar ingredientes",  // Editar ingredientes
                                "Eliminar producto",    // Eliminar producto
                                "Volver"                // Volver al menú principal
                            };

                            for(int i = 0; i < num_opciones; i++) {
                                if(i == opcion_seleccionada) {
                                    attron(A_REVERSE);
                                    mvprintw(6 + i * 2, (COLS - strlen(opciones[i])) / 2, "%s", opciones[i]);
                                    attroff(A_REVERSE);
                                } else {
                                    mvprintw(6 + i * 2, (COLS - strlen(opciones[i])) / 2, "%s", opciones[i]);
                                }
                            }

                            mvprintw(LINES - 3, 1, "Use las flechas para navegar, Enter para seleccionar, Escape para volver");

                            refresh();

                            tecla = getch();

                            switch(tecla) {
                                case KEY_UP:
                                    opcion_seleccionada = (opcion_seleccionada - 1 + num_opciones) % num_opciones;
                                    break;
                                case KEY_DOWN:
                                    opcion_seleccionada = (opcion_seleccionada + 1) % num_opciones;
                                    break;
                                case 10: // Enter
                                    if(opcion_seleccionada == 0) { // Editar producto
                                        char nombre[MAX_NOMBRE_PRODUCTO];
                                        char descripcion[MAX_DESCRIPCION];
                                        char precio_str[20]; // Para manejar el precio como string
                                        float precio;

                                        ui_clear_screen();
                                        ui_print_title("Editar Producto");

                                        attron(COLOR_PAIR(10) | A_BOLD);
                                        mvprintw(6, 5, "Nombre:");
                                        mvprintw(8, 5, "Descripcion:");
                                        mvprintw(10, 5, "Precio:");
                                        attroff(COLOR_PAIR(10) | A_BOLD);

                                        ui_print_footer("Presione ESC para cancelar");
                                        refresh();

                                        // Leer datos usando placeholders para mostrar valores actuales
                                        ui_read_input_with_placeholder(6, 14, nombre, MAX_NOMBRE_PRODUCTO, 0, prod->nombre);
                                        if(strlen(nombre) == 0) strcpy(nombre, prod->nombre); // Mantener el valor actual si está vacío

                                        ui_read_input_with_placeholder(8, 18, descripcion, MAX_DESCRIPCION, 0, prod->descripcion);
                                        if(strlen(descripcion) == 0) strcpy(descripcion, prod->descripcion); // Mantener el valor actual si esta vacio

                                        // Convertir el precio a string para usarlo como placeholder
                                        snprintf(precio_str, sizeof(precio_str), "%.2f", prod->precio);
                                        ui_read_input_with_placeholder(10, 13, precio_str, sizeof(precio_str), 0, precio_str);

                                        // Convertir el string de precio de nuevo a float
                                        precio = atof(precio_str);

                                        // Actualizar datos del producto
                                        strncpy(prod->nombre, nombre, MAX_NOMBRE_PRODUCTO-1);
                                        strncpy(prod->descripcion, descripcion, MAX_DESCRIPCION-1);
                                        prod->precio = precio;

                                        productos_guardar(); // Guardar en archivo

                                        ui_show_success("Producto actualizado correctamente.");
                                        goto salir_submenu_producto; // Regresar a la lista de productos
                                    }
                                    else if(opcion_seleccionada == 1) { // Editar ingredientes
                                        // Cargar todos los ingredientes disponibles
                                        int num_ingredientes_totales = inventario_get_total();
                                        Ingrediente ingredientes_totales[100];
                                        for(int i = 0; i < num_ingredientes_totales; i++) {
                                            Ingrediente *temp = inventario_get_by_index(i);
                                            if(temp != NULL) {
                                                ingredientes_totales[i] = *temp;
                                            }
                                        }

                                        int seleccion_ing = 0;
                                        int tecla_ing;

                                        // Calcular layout para ingredientes
                                        int columnas_ing = 4;
                                        int ancho_rec_ing = (COLS - 10) / columnas_ing;
                                        int alto_rec_ing = 6;

                                        while(1) {
                                            clear();
                                            attron(A_BOLD);
                                            mvprintw(2, (COLS - 40) / 2, "EDITAR INGREDIENTES: %s", prod->nombre);
                                            attroff(A_BOLD);

                                            // Mostrar instrucciones
                                            mvprintw(4, 2, "Teclas: [+] Aumentar, [-] Disminuir, [I] Editar Cantidad, [R] Resetear Todo, [ESC] Volver");

                                            // Calcular total de ingredientes a mostrar (todos los disponibles)
                                            int total_a_mostrar = num_ingredientes_totales;

                                            // Mostrar ingredientes como tarjetas
                                            for(int i = 0; i < total_a_mostrar; i++) {
                                                int fila = i / columnas_ing;
                                                int columna = i % columnas_ing;
                                                int x = 2 + columna * (ancho_rec_ing + 1);
                                                int y = 6 + fila * (alto_rec_ing + 1);

                                                // Mostrar ingrediente como tarjeta
                                                Ingrediente *ing_disponible = &ingredientes_totales[i];

                                                // Obtener la cantidad actual del ingrediente en el producto (si existe)
                                                int cantidad_producto = 0;
                                                for(int j = 0; j < prod->num_ingredientes; j++) {
                                                    if(prod->ingredientes[j].id_ingrediente == ing_disponible->id) {
                                                        cantidad_producto = prod->ingredientes[j].cantidad_necesaria;
                                                        break;
                                                    }
                                                }

                                                // Dibujar tarjeta de ingrediente
                                                int color = (i == seleccion_ing) ? 5 : (cantidad_producto > 0 ? 6 : 10); // Color diferente si el ingrediente está en el producto
                                                attron(COLOR_PAIR(color) | (i == seleccion_ing ? A_BOLD : 0));

                                                // Borde superior
                                                mvaddch(y, x, '+');
                                                for(int k = 1; k < ancho_rec_ing-1; k++) mvaddch(y, x+k, '=');
                                                mvaddch(y, x+ancho_rec_ing-1, '+');

                                                // Lados
                                                for(int j = 1; j < alto_rec_ing-1; j++) {
                                                    mvaddch(y+j, x, '|');
                                                    mvaddch(y+j, x+ancho_rec_ing-1, '|');
                                                }

                                                // Borde inferior
                                                mvaddch(y+alto_rec_ing-1, x, '+');
                                                for(int k = 1; k < ancho_rec_ing-1; k++) mvaddch(y+alto_rec_ing-1, x+k, '=');
                                                mvaddch(y+alto_rec_ing-1, x+ancho_rec_ing-1, '+');

                                                attroff(COLOR_PAIR(color) | (i == seleccion_ing ? A_BOLD : 0));

                                                // Contenido de la tarjeta
                                                attron(COLOR_PAIR(10));
                                                mvprintw(y+1, x+2, "%s", ing_disponible->nombre);
                                                // Si el ingrediente está en el producto, mostramos la cantidad, sino mostramos "No"
                                                if(cantidad_producto > 0) {
                                                    mvprintw(y+2, x+2, "Cantidad: %d", cantidad_producto);
                                                } else {
                                                    mvprintw(y+2, x+2, "Cantidad: No");
                                                }
                                                mvprintw(y+3, x+2, "Disp: %d", ing_disponible->cantidad);
                                                attroff(COLOR_PAIR(10));
                                            }

                                            refresh();

                                            tecla_ing = getch();

                                            // Actualizar layout si cambia el tamaño de la terminal
                                            columnas_ing = 4;
                                            ancho_rec_ing = (COLS - 10) / columnas_ing;
                                            alto_rec_ing = 6;

                                            switch(tecla_ing) {
                                                case KEY_UP:
                                                    if(seleccion_ing >= columnas_ing)
                                                        seleccion_ing -= columnas_ing;
                                                    break;
                                                case KEY_DOWN:
                                                    if(seleccion_ing + columnas_ing < total_a_mostrar)
                                                        seleccion_ing += columnas_ing;
                                                    break;
                                                case KEY_LEFT:
                                                    if(seleccion_ing > 0)
                                                        seleccion_ing--;
                                                    break;
                                                case KEY_RIGHT:
                                                    if(seleccion_ing < total_a_mostrar - 1)
                                                        seleccion_ing++;
                                                    break;
                                                case '+': // Aumentar cantidad
                                                    {
                                                        // Verificar si ya existe en el producto
                                                        int existe_ingrediente = 0;
                                                        for(int j = 0; j < prod->num_ingredientes; j++) {
                                                            if(prod->ingredientes[j].id_ingrediente == ingredientes_totales[seleccion_ing].id) {
                                                                prod->ingredientes[j].cantidad_necesaria++;
                                                                existe_ingrediente = 1;
                                                                break;
                                                            }
                                                        }
                                                        // Si no existe, agregarlo con cantidad 1
                                                        if(!existe_ingrediente && prod->num_ingredientes < MAX_INGREDIENTES_PRODUCTO) {
                                                            prod->ingredientes[prod->num_ingredientes].id = prod->num_ingredientes + 1;
                                                            prod->ingredientes[prod->num_ingredientes].id_ingrediente = ingredientes_totales[seleccion_ing].id;
                                                            prod->ingredientes[prod->num_ingredientes].cantidad_necesaria = 1;
                                                            prod->num_ingredientes++;
                                                        }
                                                        productos_guardar(); // Guardar en archivo inmediatamente
                                                    }
                                                    break;
                                                case '-': // Disminuir cantidad
                                                    {
                                                        // Encontrar el ingrediente en el producto
                                                        int ing_producto_index = -1;
                                                        for(int j = 0; j < prod->num_ingredientes; j++) {
                                                            if(prod->ingredientes[j].id_ingrediente == ingredientes_totales[seleccion_ing].id) {
                                                                ing_producto_index = j;
                                                                break;
                                                            }
                                                        }

                                                        if(ing_producto_index != -1) {
                                                            prod->ingredientes[ing_producto_index].cantidad_necesaria--;
                                                            if(prod->ingredientes[ing_producto_index].cantidad_necesaria <= 0) {
                                                                // Si es 0, lo quitamos del producto
                                                                for(int j = ing_producto_index; j < prod->num_ingredientes - 1; j++) {
                                                                    prod->ingredientes[j] = prod->ingredientes[j+1];
                                                                }
                                                                prod->num_ingredientes--;
                                                            }
                                                            productos_guardar(); // Guardar en archivo inmediatamente
                                                        }
                                                    }
                                                    break;
                                                case 'i': // Editar cantidad directamente
                                                case 'I':
                                                    {
                                                        // Verificar si ya existe en el producto
                                                        int ing_producto_index = -1;
                                                        for(int j = 0; j < prod->num_ingredientes; j++) {
                                                            if(prod->ingredientes[j].id_ingrediente == ingredientes_totales[seleccion_ing].id) {
                                                                ing_producto_index = j;
                                                                break;
                                                            }
                                                        }

                                                        // Mostrar recuadro emergente simple para editar cantidad
                                                        int rec_x = (COLS - 30) / 2;  // Centrado horizontalmente
                                                        int rec_y = (LINES - 5) / 2;  // Centrado verticalmente
                                                        int rec_ancho = 30;
                                                        int rec_alto = 5;

                                                        // Dibujar recuadro emergente
                                                        attron(COLOR_PAIR(5) | A_BOLD);

                                                        // Borde superior
                                                        mvaddch(rec_y, rec_x, '+');
                                                        for(int k = 1; k < rec_ancho-1; k++) mvaddch(rec_y, rec_x+k, '-');
                                                        mvaddch(rec_y, rec_x+rec_ancho-1, '+');

                                                        // Lados
                                                        for(int j = 1; j < rec_alto-1; j++) {
                                                            mvaddch(rec_y+j, rec_x, '|');
                                                            mvaddch(rec_y+j, rec_x+rec_ancho-1, '|');
                                                        }

                                                        // Borde inferior
                                                        mvaddch(rec_y+rec_alto-1, rec_x, '+');
                                                        for(int k = 1; k < rec_ancho-1; k++) mvaddch(rec_y+rec_alto-1, rec_x+k, '-');
                                                        mvaddch(rec_y+rec_alto-1, rec_x+rec_ancho-1, '+');

                                                        attroff(COLOR_PAIR(5) | A_BOLD);

                                                        // Contenido del recuadro
                                                        attron(COLOR_PAIR(10) | A_BOLD);
                                                        mvprintw(rec_y+1, rec_x+2, "Ingrese nueva cantidad:");
                                                        attroff(COLOR_PAIR(10) | A_BOLD);

                                                        // Mostrar el cuadro de entrada
                                                        char cantidad_str[10];
                                                        mvchgat(rec_y+3, rec_x+2, 10, A_NORMAL, 7, NULL); // Resaltar el campo de entrada
                                                        refresh();

                                                        // Leer la entrada directamente en el recuadro
                                                        int pos_x = rec_x + 2;
                                                        int pos_y = rec_y + 3;
                                                        int input_len = 0;
                                                        int ch;

                                                        // Limpiar buffer de entrada
                                                        flushinp();

                                                        // Mostrar valor actual como placeholder
                                                        snprintf(cantidad_str, sizeof(cantidad_str), "%d", (ing_producto_index != -1) ? prod->ingredientes[ing_producto_index].cantidad_necesaria : 0);
                                                        mvprintw(pos_y, pos_x, "%-10s", cantidad_str);
                                                        move(pos_y, pos_x + strlen(cantidad_str));
                                                        refresh();

                                                        while(1) {
                                                            ch = getch();

                                                            if(ch == 10 || ch == '\n') { // Enter
                                                                break;
                                                            } else if(ch == 27) { // Escape
                                                                cantidad_str[0] = '\0'; // Cancelar
                                                                break;
                                                            } else if(ch == KEY_BACKSPACE || ch == 127 || ch == 8) { // Backspace
                                                                if(input_len > 0) {
                                                                    input_len--;
                                                                    cantidad_str[input_len] = '\0';
                                                                    mvaddch(pos_y, pos_x + input_len, ' ');
                                                                    move(pos_y, pos_x + input_len);
                                                                }
                                                            } else if(isdigit(ch) && input_len < 9) { // Solo dígitos
                                                                cantidad_str[input_len] = ch;
                                                                input_len++;
                                                                cantidad_str[input_len] = '\0';
                                                                mvprintw(pos_y, pos_x, "%-10s", cantidad_str);
                                                                move(pos_y, pos_x + input_len);
                                                            }
                                                            refresh();
                                                        }

                                                        // Procesar la cantidad ingresada
                                                        if(strlen(cantidad_str) > 0) {
                                                            int nueva_cantidad = atoi(cantidad_str);

                                                            if(nueva_cantidad > 0) {
                                                                if(ing_producto_index != -1) {
                                                                    // Actualizar cantidad si ya existe
                                                                    prod->ingredientes[ing_producto_index].cantidad_necesaria = nueva_cantidad;
                                                                } else {
                                                                    // Agregar nuevo ingrediente si no existe
                                                                    if(prod->num_ingredientes < MAX_INGREDIENTES_PRODUCTO) {
                                                                        prod->ingredientes[prod->num_ingredientes].id = prod->num_ingredientes + 1;
                                                                        prod->ingredientes[prod->num_ingredientes].id_ingrediente = ingredientes_totales[seleccion_ing].id;
                                                                        prod->ingredientes[prod->num_ingredientes].cantidad_necesaria = nueva_cantidad;
                                                                        prod->num_ingredientes++;
                                                                    }
                                                                }
                                                                productos_guardar(); // Guardar en archivo
                                                                ui_show_success("Cantidad actualizada correctamente.");
                                                            } else if(nueva_cantidad == 0) {
                                                                // Si la nueva cantidad es 0, eliminar el ingrediente del producto
                                                                if(ing_producto_index != -1) {
                                                                    for(int j = ing_producto_index; j < prod->num_ingredientes - 1; j++) {
                                                                        prod->ingredientes[j] = prod->ingredientes[j+1];
                                                                    }
                                                                    prod->num_ingredientes--;
                                                                    productos_guardar(); // Guardar en archivo
                                                                    ui_show_success("Ingrediente eliminado del producto.");
                                                                }
                                                            } else {
                                                                ui_show_error("Cantidad inválida.");
                                                            }
                                                        }
                                                    }
                                                    break;
                                                case 'r': // Resetear todos los valores a cero
                                                case 'R':
                                                    {
                                                        int seleccion_confirmacion = 0;
                                                        int tecla_confirmacion;
                                                        int num_opciones_conf = 2; // Sí, No
                                                        // char *opciones_conf[] = {
                                                        //     "CONFIRMAR",
                                                        //     "CANCELAR"
                                                        // };

                                                        while(1) {
                                                            clear();
                                                            attron(A_BOLD);
                                                            mvprintw(2, (COLS - 45) / 2, "¿RESETEAR TODOS LOS INGREDIENTES A 0?");
                                                            mvprintw(3, (COLS - strlen(prod->nombre)) / 2, "%s", prod->nombre);
                                                            attroff(A_BOLD);

                                                            // Dibujar botones usando la función centralizada
                                                            int centro_x = (COLS - 40) / 2; // Centrado para dos botones
                                                            int boton_y = 8;
                                                            int boton_ancho = 15;
                                                            int boton_alto = 4;

                                                            dibujar_botones_confirmacion(centro_x, boton_y, seleccion_confirmacion, boton_ancho, boton_alto, "CONFIRMAR", "CANCELAR");

                                                            mvprintw(LINES - 3, 1, "Use flechas para navegar, Enter para seleccionar, ESC para cancelar");
                                                            refresh();

                                                            tecla_confirmacion = getch();

                                                            switch(tecla_confirmacion) {
                                                                case KEY_LEFT:
                                                                    seleccion_confirmacion = (seleccion_confirmacion - 1 + num_opciones_conf) % num_opciones_conf;
                                                                    break;
                                                                case KEY_RIGHT:
                                                                    seleccion_confirmacion = (seleccion_confirmacion + 1) % num_opciones_conf;
                                                                    break;
                                                                case 10: // Enter
                                                                    if(seleccion_confirmacion == 0) { // Confirmar
                                                                        // Resetear todas las cantidades a 0 (eliminar todos los ingredientes del producto)
                                                                        prod->num_ingredientes = 0; // Simplemente reseteamos el contador
                                                                        productos_guardar(); // Guardar en archivo
                                                                        ui_show_success("Todas las cantidades de ingredientes reseteadas a 0.");
                                                                        goto salir_confirmacion_reset; // Salir del bucle de confirmación
                                                                    } else { // Cancelar
                                                                        ui_show_info("Reset cancelado.");
                                                                        goto salir_confirmacion_reset; // Salir del bucle de confirmación
                                                                    }
                                                                    break;
                                                                case 27: // Escape
                                                                    ui_show_info("Reset cancelado.");
                                                                    goto salir_confirmacion_reset; // Salir del bucle de confirmación
                                                            }
                                                        }
                                                        salir_confirmacion_reset:;
                                                    }
                                                    break;
                                                case 27: // Escape
                                                    goto salir_submenu_producto; // Regresar a la lista de productos
                                                    break;
                                            }
                                        }
                                    }
                                    else if(opcion_seleccionada == 2) { // Eliminar producto
                                        // Confirmar eliminación con interfaz de selección
                                        int seleccion_confirmacion = 0;
                                        int tecla_confirmacion;
                                        int num_opciones_conf = 2; // Sí, No
                                        // char *opciones_conf[] = {
                                        //     "CONFIRMAR",
                                        //     "CANCELAR"
                                        // };

                                        while(1) {
                                            clear();
                                            attron(A_BOLD);
                                            mvprintw(2, (COLS - 40) / 2, "ESTA SEGURO DE ELIMINAR EL PRODUCTO?");
                                            mvprintw(3, (COLS - strlen(prod->nombre)) / 2, "%s", prod->nombre);
                                            attroff(A_BOLD);

                                            // Dibujar botones usando la función centralizada
                                            int centro_x = (COLS - 40) / 2; // Centrado para dos botones
                                            int boton_y = 8;
                                            int boton_ancho = 15;
                                            int boton_alto = 4;

                                            dibujar_botones_confirmacion(centro_x, boton_y, seleccion_confirmacion, boton_ancho, boton_alto, "CONFIRMAR", "CANCELAR");

                                            mvprintw(LINES - 3, 1, "Use flechas para navegar, Enter para seleccionar, ESC para cancelar");
                                            refresh();

                                            tecla_confirmacion = getch();

                                            switch(tecla_confirmacion) {
                                                case KEY_LEFT:
                                                    seleccion_confirmacion = (seleccion_confirmacion - 1 + num_opciones_conf) % num_opciones_conf;
                                                    break;
                                                case KEY_RIGHT:
                                                    seleccion_confirmacion = (seleccion_confirmacion + 1) % num_opciones_conf;
                                                    break;
                                                case 10: // Enter
                                                    if(seleccion_confirmacion == 0) { // Confirmar
                                                        // Eliminar el producto
                                                        if(producto_eliminar(prod->id) == 0) {
                                                            productos_guardar(); // Guardar en archivo
                                                            ui_show_success("Producto eliminado correctamente.");
                                                            goto salir_submenu_producto; // Salir completamente y regresar a la lista de productos
                                                        } else {
                                                            ui_show_error("Error al eliminar producto.");
                                                        }
                                                    } else { // Cancelar
                                                        ui_show_info("Eliminación cancelada.");
                                                    }
                                                    break;
                                                case 27: // Escape
                                                    ui_show_info("Eliminación cancelada.");
                                                    break;
                                            }
                                            break; // Romper el bucle de confirmación y regresar al menú de opciones de producto
                                        }
                                    }
                                    else if(opcion_seleccionada == 3) { // Volver
                                        goto salir_submenu_producto; // Regresar a la lista de productos
                                    }
                                    break;
                                case 27: // Escape
                                    goto salir_submenu_producto; // Regresar a la lista de productos
                            }
                        }
                    }
                salir_submenu_producto:;
                }
                break;
            case 27: // Escape
                return; // Salir del menú de productos
        }
    }
}

void mostrar_menu_ingredientes() {
    inventario_cargar(); // Cargar inventario

    // Calcular total de ingredientes + 1 para la tarjeta de nuevo ingrediente
    int num_ingredientes = inventario_get_total();
    int total_items = num_ingredientes + 1; // +1 para la tarjeta de nuevo ingrediente

    int seleccion_ing = 0;
    int tecla_ing;

    // Calcular layout para ingredientes
    int columnas_ing = 4;
    int ancho_rec_ing = (COLS - 10) / columnas_ing;
    int alto_rec_ing = 6;

    while(1) {
        num_ingredientes = inventario_get_total();
        total_items = num_ingredientes + 1; // Actualizar total_items

        clear();
        attron(A_BOLD);
        mvprintw(2, (COLS - 40) / 2, "ADMINISTRAR INGREDIENTES");
        attroff(A_BOLD);

        // Mostrar instrucciones
        mvprintw(4, 2, "Teclas: [+] Aumentar, [-] Disminuir, [I] Editar Cantidad, [E] Eliminar, [R] Resetear Todo, [ESC] Volver");

        // Mostrar ingredientes como tarjetas
        for(int i = 0; i < total_items; i++) {
            int fila = i / columnas_ing;
            int columna = i % columnas_ing;
            int x = 2 + columna * (ancho_rec_ing + 1);
            int y = 6 + fila * (alto_rec_ing + 1);

            Ingrediente *ing_disponible = NULL;
            int color;

            if(i == 0) { // Primera posición: "Agregar nuevo ingrediente"
                color = (i == seleccion_ing) ? 5 : 10;
                attron(COLOR_PAIR(color) | (i == seleccion_ing ? A_BOLD : 0));

                // Dibujar tarjeta de ingrediente
                // Borde superior
                mvaddch(y, x, '+');
                for(int k = 1; k < ancho_rec_ing-1; k++) mvaddch(y, x+k, '=');
                mvaddch(y, x+ancho_rec_ing-1, '+');

                // Lados
                for(int j = 1; j < alto_rec_ing-1; j++) {
                    mvaddch(y+j, x, '|');
                    mvaddch(y+j, x+ancho_rec_ing-1, '|');
                }

                // Borde inferior
                mvaddch(y+alto_rec_ing-1, x, '+');
                for(int k = 1; k < ancho_rec_ing-1; k++) mvaddch(y+alto_rec_ing-1, x+k, '=');
                mvaddch(y+alto_rec_ing-1, x+ancho_rec_ing-1, '+');

                attroff(COLOR_PAIR(color) | (i == seleccion_ing ? A_BOLD : 0));

                // Contenido de la tarjeta
                attron(COLOR_PAIR(10) | A_BOLD);
                mvprintw(y+1, x+2, "NUEVO INGREDIENTE");
                mvprintw(y+2, x+2, "Presione Enter para");
                mvprintw(y+3, x+2, "crear nuevo ingrediente");
                attroff(COLOR_PAIR(10) | A_BOLD);
            } else {
                // Obtener ingrediente existente (i-1 porque la posición 0 es el nuevo ingrediente)
                ing_disponible = inventario_get_by_index(i - 1);
                if(ing_disponible != NULL) {
                    // Dibujar tarjeta de ingrediente
                    color = (i == seleccion_ing) ? 5 : 10; // Color diferente si está seleccionado
                    attron(COLOR_PAIR(color) | (i == seleccion_ing ? A_BOLD : 0));

                    // Borde superior
                    mvaddch(y, x, '+');
                    for(int k = 1; k < ancho_rec_ing-1; k++) mvaddch(y, x+k, '=');
                    mvaddch(y, x+ancho_rec_ing-1, '+');

                    // Lados
                    for(int j = 1; j < alto_rec_ing-1; j++) {
                        mvaddch(y+j, x, '|');
                        mvaddch(y+j, x+ancho_rec_ing-1, '|');
                    }

                    // Borde inferior
                    mvaddch(y+alto_rec_ing-1, x, '+');
                    for(int k = 1; k < ancho_rec_ing-1; k++) mvaddch(y+alto_rec_ing-1, x+k, '=');
                    mvaddch(y+alto_rec_ing-1, x+ancho_rec_ing-1, '+');

                    attroff(COLOR_PAIR(color) | (i == seleccion_ing ? A_BOLD : 0));

                    // Contenido de la tarjeta
                    attron(COLOR_PAIR(10));
                    mvprintw(y+1, x+2, "%s", ing_disponible->nombre);
                    mvprintw(y+2, x+2, "Cantidad: %d", ing_disponible->cantidad);
                    mvprintw(y+3, x+2, "ID: %d", ing_disponible->id);
                    attroff(COLOR_PAIR(10));
                }
            }
        }

        refresh();

        tecla_ing = getch();

        // Actualizar layout si cambia el tamaño de la terminal
        columnas_ing = 4;
        ancho_rec_ing = (COLS - 10) / columnas_ing;
        alto_rec_ing = 6;

        switch(tecla_ing) {
            case KEY_UP:
                if(seleccion_ing >= columnas_ing)
                    seleccion_ing -= columnas_ing;
                if(seleccion_ing < 0) seleccion_ing = 0; // Asegurar que no sea negativo
                break;
            case KEY_DOWN:
                if(seleccion_ing + columnas_ing < total_items)
                    seleccion_ing += columnas_ing;
                break;
            case KEY_LEFT:
                if(seleccion_ing > 0)
                    seleccion_ing--;
                break;
            case KEY_RIGHT:
                if(seleccion_ing < total_items - 1)
                    seleccion_ing++;
                break;
            case 10: // Enter - Crear nuevo ingrediente o editar existente
                if(seleccion_ing == 0) { // Crear nuevo ingrediente
                    char nombre[MAX_NOMBRE_INGREDIENTE];
                    char cantidad_str[20];
                    int cantidad;

                    ui_clear_screen();
                    ui_print_title("Agregar Nuevo Ingrediente");

                    attron(COLOR_PAIR(10) | A_BOLD);
                    mvprintw(6, 5, "Nombre:");
                    mvprintw(8, 5, "Cantidad inicial:");
                    attroff(COLOR_PAIR(10) | A_BOLD);

                    ui_read_input(6, 14, nombre, MAX_NOMBRE_INGREDIENTE, 0);
                    if(strlen(nombre) == 0) break;

                    ui_read_input(8, 23, cantidad_str, sizeof(cantidad_str), 0);
                    cantidad = atoi(cantidad_str);
                    if(cantidad < 0) cantidad = 0;

                    // Agregar ingrediente
                    if(inventario_agregar(nombre, cantidad) == 0) {
                        inventario_guardar(); // Guardar en archivo
                        ui_show_success("Ingrediente agregado correctamente.");
                    } else {
                        ui_show_error("Error al agregar ingrediente.");
                    }
                } else { // Editar ingrediente existente
                    Ingrediente *ing = inventario_get_by_index(seleccion_ing - 1); // -1 porque la posición 0 es nuevo ingrediente
                    if(ing != NULL) {
                        char nombre[MAX_NOMBRE_INGREDIENTE];
                        char cantidad_str[20]; // Para manejar la cantidad como string
                        int cantidad;

                        ui_clear_screen();
                        ui_print_title("Editar Ingrediente");

                        attron(COLOR_PAIR(10) | A_BOLD);
                        mvprintw(6, 5, "Nombre:");
                        mvprintw(8, 5, "Cantidad:");
                        attroff(COLOR_PAIR(10) | A_BOLD);

                        ui_print_footer("Presione ESC para cancelar");
                        refresh();

                        // Leer datos usando placeholders para mostrar valores actuales
                        ui_read_input_with_placeholder(6, 14, nombre, MAX_NOMBRE_INGREDIENTE, 0, ing->nombre);
                        if(strlen(nombre) == 0) strcpy(nombre, ing->nombre); // Mantener el valor actual si está vacío

                        // Convertir la cantidad a string para usarlo como placeholder
                        snprintf(cantidad_str, sizeof(cantidad_str), "%d", ing->cantidad);
                        ui_read_input_with_placeholder(8, 15, cantidad_str, sizeof(cantidad_str), 0, cantidad_str);

                        // Convertir el string de cantidad de nuevo a int
                        cantidad = atoi(cantidad_str);
                        if(cantidad < 0) cantidad = 0;

                        // Actualizar datos del ingrediente - solo si hay cambio
                        if(strcmp(nombre, ing->nombre) != 0 || cantidad != ing->cantidad) {
                            if(strcmp(nombre, ing->nombre) != 0) {
                                // Si cambio el nombre, debemos remover el actual y agregar el nuevo
                                inventario_agregar(ing->nombre, 0); // Remover el antiguo
                                if(inventario_agregar(nombre, cantidad) == 0) {
                                    inventario_guardar(); // Guardar en archivo
                                    ui_show_success("Ingrediente actualizado correctamente.");
                                } else {
                                    ui_show_error("Error al actualizar ingrediente.");
                                }
                            } else {
                                // Si solo cambio la cantidad, usamos la función específica para modificar cantidad
                                if(inventario_modificar_cantidad(ing->id, cantidad) == 0) {
                                    ui_show_success("Cantidad actualizada correctamente.");
                                } else {
                                    ui_show_error("Error al actualizar cantidad.");
                                }
                            }
                        }
                    }
                }
                break;
            case '+': // Aumentar cantidad (solo para ingredientes existentes)
                if(seleccion_ing > 0) { // No en la tarjeta de nuevo ingrediente
                    Ingrediente *ing = inventario_get_by_index(seleccion_ing - 1); // -1 porque la posición 0 es nuevo ingrediente
                    if(ing != NULL) {
                        ing->cantidad++; // Aumentar cantidad en la copia local
                        // Actualizar en el archivo usando la función específica para modificar cantidad
                        inventario_modificar_cantidad(ing->id, ing->cantidad);
                    }
                }
                break;
            case '-': // Disminuir cantidad (solo para ingredientes existentes)
                if(seleccion_ing > 0) { // No en la tarjeta de nuevo ingrediente
                    Ingrediente *ing = inventario_get_by_index(seleccion_ing - 1); // -1 porque la posición 0 es nuevo ingrediente
                    if(ing != NULL && ing->cantidad > 0) {
                        ing->cantidad--; // Disminuir cantidad en la copia local
                        // Actualizar en el archivo usando la función específica para modificar cantidad
                        inventario_modificar_cantidad(ing->id, ing->cantidad);
                    }
                }
                break;
            case 'i': // Editar cantidad directamente (solo para ingredientes existentes)
            case 'I':
                if(seleccion_ing > 0) { // No en la tarjeta de nuevo ingrediente
                    Ingrediente *ing = inventario_get_by_index(seleccion_ing - 1); // -1 porque la posición 0 es nuevo ingrediente
                    if(ing != NULL) {
                        // Mostrar recuadro emergente simple para editar cantidad
                        int rec_x = (COLS - 30) / 2;  // Centrado horizontalmente
                        int rec_y = (LINES - 5) / 2;  // Centrado verticalmente
                        int rec_ancho = 30;
                        int rec_alto = 5;

                        // Dibujar recuadro emergente
                        attron(COLOR_PAIR(5) | A_BOLD);

                        // Borde superior
                        mvaddch(rec_y, rec_x, '+');
                        for(int k = 1; k < rec_ancho-1; k++) mvaddch(rec_y, rec_x+k, '-');
                        mvaddch(rec_y, rec_x+rec_ancho-1, '+');

                        // Lados
                        for(int j = 1; j < rec_alto-1; j++) {
                            mvaddch(rec_y+j, rec_x, '|');
                            mvaddch(rec_y+j, rec_x+rec_ancho-1, '|');
                        }

                        // Borde inferior
                        mvaddch(rec_y+rec_alto-1, rec_x, '+');
                        for(int k = 1; k < rec_ancho-1; k++) mvaddch(rec_y+rec_alto-1, rec_x+k, '-');
                        mvaddch(rec_y+rec_alto-1, rec_x+rec_ancho-1, '+');

                        attroff(COLOR_PAIR(5) | A_BOLD);

                        // Contenido del recuadro
                        attron(COLOR_PAIR(10) | A_BOLD);
                        mvprintw(rec_y+1, rec_x+2, "Ingrese nueva cantidad:");
                        attroff(COLOR_PAIR(10) | A_BOLD);

                        // Mostrar el cuadro de entrada
                        char cantidad_str[10];
                        mvchgat(rec_y+3, rec_x+2, 10, A_NORMAL, 7, NULL); // Resaltar el campo de entrada
                        refresh();

                        // Leer la entrada directamente en el recuadro
                        int pos_x = rec_x + 2;
                        int pos_y = rec_y + 3;
                        int input_len = 0;
                        int ch;

                        // Limpiar buffer de entrada
                        flushinp();

                        // Mostrar valor actual como placeholder
                        snprintf(cantidad_str, sizeof(cantidad_str), "%d", ing->cantidad);
                        mvprintw(pos_y, pos_x, "%-10s", cantidad_str);
                        move(pos_y, pos_x + strlen(cantidad_str));
                        refresh();

                        while(1) {
                            ch = getch();

                            if(ch == 10 || ch == '\n') { // Enter
                                break;
                            } else if(ch == 27) { // Escape
                                cantidad_str[0] = '\0'; // Cancelar
                                break;
                            } else if(ch == KEY_BACKSPACE || ch == 127 || ch == 8) { // Backspace
                                if(input_len > 0) {
                                    input_len--;
                                    cantidad_str[input_len] = '\0';
                                    mvaddch(pos_y, pos_x + input_len, ' ');
                                    move(pos_y, pos_x + input_len);
                                }
                            } else if(isdigit(ch) && input_len < 9) { // Solo dígitos
                                cantidad_str[input_len] = ch;
                                input_len++;
                                cantidad_str[input_len] = '\0';
                                mvprintw(pos_y, pos_x, "%-10s", cantidad_str);
                                move(pos_y, pos_x + input_len);
                            }
                            refresh();
                        }

                        // Procesar la cantidad ingresada
                        if(strlen(cantidad_str) > 0) {
                            int nueva_cantidad = atoi(cantidad_str);

                            if(nueva_cantidad >= 0) {
                                // Actualizar cantidad en el stock usando la función específica
                                if(inventario_modificar_cantidad(ing->id, nueva_cantidad) == 0) {
                                    ui_show_success("Cantidad actualizada correctamente.");
                                } else {
                                    ui_show_error("Error al actualizar cantidad.");
                                }
                            } else {
                                ui_show_error("Cantidad inválida.");
                            }
                        }
                    }
                }
                break;
            case 'e': // Eliminar ingrediente seleccionado
            case 'E':
                if(seleccion_ing > 0) { // No en la tarjeta de nuevo ingrediente
                    Ingrediente *ing = inventario_get_by_index(seleccion_ing - 1); // -1 porque la posición 0 es nuevo ingrediente
                    if(ing != NULL) {
                        int seleccion_confirmacion = 0;
                        int tecla_confirmacion;
                        int num_opciones_conf = 2; // Confirmar, Cancelar
                        // char *opciones_conf[] = {
                        //     "CONFIRMAR",
                        //     "CANCELAR"
                        // };

                        while(1) {
                            clear();
                            attron(A_BOLD);
                            mvprintw(2, (COLS - 40) / 2, "¿ELIMINAR INGREDIENTE?");
                            mvprintw(3, (COLS - strlen(ing->nombre)) / 2, "%s", ing->nombre);
                            attroff(A_BOLD);

                            // Dibujar botones usando la función centralizada
                            int centro_x = (COLS - 40) / 2; // Centrado para dos botones
                            int boton_y = 8;
                            int boton_ancho = 15;
                            int boton_alto = 4;

                            dibujar_botones_confirmacion(centro_x, boton_y, seleccion_confirmacion, boton_ancho, boton_alto, "CONFIRMAR", "CANCELAR");

                            mvprintw(LINES - 3, 1, "Use flechas para navegar, Enter para seleccionar, ESC para cancelar");
                            refresh();

                            tecla_confirmacion = getch();

                            switch(tecla_confirmacion) {
                                case KEY_LEFT:
                                    seleccion_confirmacion = (seleccion_confirmacion - 1 + num_opciones_conf) % num_opciones_conf;
                                    break;
                                case KEY_RIGHT:
                                    seleccion_confirmacion = (seleccion_confirmacion + 1) % num_opciones_conf;
                                    break;
                                case 10: // Enter
                                    if(seleccion_confirmacion == 0) { // Confirmar
                                        // Eliminar el ingrediente usando la función específica
                                        if(inventario_eliminar_ingrediente(ing->id) == 0) {
                                            ui_show_success("Ingrediente eliminado correctamente.");
                                        } else {
                                            ui_show_error("Error al eliminar ingrediente.");
                                        }
                                        goto salir_confirmacion_eliminar; // Salir del bucle de confirmación
                                    } else { // Cancelar
                                        ui_show_info("Eliminación cancelada.");
                                        goto salir_confirmacion_eliminar; // Salir del bucle de confirmación
                                    }
                                    break;
                                case 27: // Escape
                                    ui_show_info("Eliminación cancelada.");
                                    goto salir_confirmacion_eliminar; // Salir del bucle de confirmación
                            }
                        }
                        salir_confirmacion_eliminar:;
                    }
                }
                break;
            case 'r': // Resetear todos los valores a cero
            case 'R':
                {
                    int seleccion_confirmacion = 0;
                    int tecla_confirmacion;
                    int num_opciones_conf = 2; // Sí, No
                    // char *opciones_conf[] = {
                    //     "CONFIRMAR",
                    //     "CANCELAR"
                    // };

                    while(1) {
                        clear();
                        attron(A_BOLD);
                        mvprintw(2, (COLS - 45) / 2, "¿RESETEAR TODOS LOS INGREDIENTES A 0?");
                        mvprintw(3, (COLS - 10) / 2, "STOCK ACTUAL");
                        attroff(A_BOLD);

                        // Dibujar botones usando la función centralizada
                        int centro_x = (COLS - 40) / 2; // Centrado para dos botones
                        int boton_y = 8;
                        int boton_ancho = 15;
                        int boton_alto = 4;

                        dibujar_botones_confirmacion(centro_x, boton_y, seleccion_confirmacion, boton_ancho, boton_alto, "CONFIRMAR", "CANCELAR");

                        mvprintw(LINES - 3, 1, "Use flechas para navegar, Enter para seleccionar, ESC para cancelar");
                        refresh();

                        tecla_confirmacion = getch();

                        switch(tecla_confirmacion) {
                            case KEY_LEFT:
                                seleccion_confirmacion = (seleccion_confirmacion - 1 + num_opciones_conf) % num_opciones_conf;
                                break;
                            case KEY_RIGHT:
                                seleccion_confirmacion = (seleccion_confirmacion + 1) % num_opciones_conf;
                                break;
                            case 10: // Enter
                                if(seleccion_confirmacion == 0) { // Confirmar
                                    // Resetear todas las cantidades a 0
                                    for(int i = 0; i < num_ingredientes; i++) {
                                        Ingrediente *temp_ing = inventario_get_by_index(i);
                                        if(temp_ing != NULL) {
                                            // Usar la función específica para modificar la cantidad en lugar de crear nuevos ingredientes
                                            inventario_modificar_cantidad(temp_ing->id, 0);
                                        }
                                    }
                                    ui_show_success("Todas las cantidades de ingredientes reseteadas a 0.");
                                    goto salir_confirmacion_reset; // Salir del bucle de confirmación
                                } else { // Cancelar
                                    ui_show_info("Reset cancelado.");
                                    goto salir_confirmacion_reset; // Salir del bucle de confirmación
                                }
                                break;
                            case 27: // Escape
                                ui_show_info("Reset cancelado.");
                                goto salir_confirmacion_reset; // Salir del bucle de confirmación
                        }
                    }
                    salir_confirmacion_reset:;
                }
                break;
            case 27: // Escape
                return; // Salir del menú de ingredientes
                break;
        }
    }
}

void mostrar_menu_logs() {
    int seleccion = 0;
    int tecla;
    int num_opciones = 3;

    char *opciones[] = {
        "Logs del sistema",    // Últimas 20 l’neas de autentificacion.log
        "Logs de usuarios",    // Últimas 20 l’neas de cocina.log
        "Volver"
    };

    while(1) {
        ui_clear_screen();
        ui_print_title("LOGS DEL SISTEMA");

        // Mostrar opciones
        for(int i = 0; i < num_opciones; i++) {
            if(i == seleccion) {
                attron(A_REVERSE);
                mvprintw(6 + i * 2, (COLS - strlen(opciones[i])) / 2, "%s", opciones[i]);
                attroff(A_REVERSE);
            } else {
                mvprintw(6 + i * 2, (COLS - strlen(opciones[i])) / 2, "%s", opciones[i]);
            }
        }

        mvprintw(LINES - 3, 1, "Use las flechas para navegar y Enter para seleccionar");

        refresh();

        tecla = getch();

        switch(tecla) {
            case KEY_UP:
                seleccion = (seleccion - 1 + num_opciones) % num_opciones;
                break;
            case KEY_DOWN:
                seleccion = (seleccion + 1) % num_opciones;
                break;
            case 10: // Enter
                switch(seleccion) {
                    case 0: // Logs del sistema
                        {
                            ui_clear_screen();
                            ui_print_title("LOGS DEL SISTEMA (autenticacion.log)");

                            // Cargar y mostrar las últimas 20 líneas de autenticacion.log
                            FILE *archivo = fopen("autenticacion.log", "r");
                            if(archivo != NULL) {
                                // Leer todas las líneas y almacenar las últimas 20
                                char lineas[20][512]; // Array para almacenar las últimas 20 líneas
                                int total_lineas = 0;

                                char linea_completa[512];
                                while(fgets(linea_completa, sizeof(linea_completa), archivo)) {
                                    if(total_lineas < 20) {
                                        strcpy(lineas[total_lineas], linea_completa);
                                        total_lineas++;
                                    } else {
                                        // Desplazar las líneas para mantener solo las últimas 20
                                        for(int i = 0; i < 19; i++) {
                                            strcpy(lineas[i], lineas[i+1]);
                                        }
                                        strcpy(lineas[19], linea_completa);
                                    }
                                }
                                fclose(archivo);

                                int linea_inicio = (total_lineas > 20) ? total_lineas - 20 : 0;
                                int linea_y = 6;
                                for(int i = linea_inicio; i < total_lineas && linea_y < LINES - 2; i++) {
                                    // Eliminar salto de línea
                                    lineas[i][strcspn(lineas[i], "\n")] = 0;
                                    mvprintw(linea_y++, 2, "%s", lineas[i]);
                                }

                                if(total_lineas == 0) {
                                    mvprintw(6, 2, "No hay registros en el archivo de logs.");
                                }
                            } else {
                                mvprintw(6, 2, "No se pudo abrir el archivo autentificacion.log.");
                            }

                            mvprintw(LINES - 3, 2, "Presione cualquier tecla para continuar...");
                            refresh();
                            ui_wait_key();
                        }
                        break;
                    case 1: // Logs de usuarios
                        {
                            ui_clear_screen();
                            ui_print_title("LOGS DE USUARIOS (cocina.log)");

                            // Cargar y mostrar las últimas 20 líneas de cocina.log
                            FILE *archivo = fopen("cocina.log", "r");
                            if(archivo != NULL) {
                                // Leer todas las líneas y almacenar las últimas 20
                                char lineas[20][512]; // Array para almacenar las últimas 20 líneas
                                int total_lineas = 0;

                                char linea_completa[512];
                                while(fgets(linea_completa, sizeof(linea_completa), archivo)) {
                                    if(total_lineas < 20) {
                                        strcpy(lineas[total_lineas], linea_completa);
                                        total_lineas++;
                                    } else {
                                        // Desplazar las líneas para mantener solo las últimas 20
                                        for(int i = 0; i < 19; i++) {
                                            strcpy(lineas[i], lineas[i+1]);
                                        }
                                        strcpy(lineas[19], linea_completa);
                                    }
                                }
                                fclose(archivo);

                                int linea_inicio = (total_lineas > 20) ? total_lineas - 20 : 0;
                                int linea_y = 6;
                                for(int i = linea_inicio; i < total_lineas && linea_y < LINES - 2; i++) {
                                    // Eliminar salto de línea
                                    lineas[i][strcspn(lineas[i], "\n")] = 0;
                                    mvprintw(linea_y++, 2, "%s", lineas[i]);
                                }

                                if(total_lineas == 0) {
                                    mvprintw(6, 2, "No hay registros en el archivo de logs.");
                                }
                            } else {
                                mvprintw(6, 2, "No se pudo abrir el archivo cocina.log.");
                            }

                            mvprintw(LINES - 3, 2, "Presione cualquier tecla para continuar...");
                            refresh();
                            ui_wait_key();
                        }
                        break;
                    case 2: // Volver
                        return; // Regresar al menú de administrador
                }
                break;
            case 27: // Tecla Escape
                return; // Regresar al menú de administrador
        }
    }
}

// Funciones de la interfaz de cocina

// Enviar petición
int enviar_peticion_cocina(Peticion *pet, Respuesta *resp) {
    if (mi_slot == -1) return -1;

    down(semid_servidor);
    datos_servidor->clientes[mi_slot].peticion = *pet;
    datos_servidor->clientes[mi_slot].peticion_lista = 1;
    up(semid_servidor);

    int timeout = 0;
    while (timeout < 100) {
        down(semid_servidor);
        if (datos_servidor->clientes[mi_slot].respuesta_lista == 1) {
            *resp = datos_servidor->clientes[mi_slot].respuesta;
            datos_servidor->clientes[mi_slot].respuesta_lista = 0;
            up(semid_servidor);
            return 0;
        }
        up(semid_servidor);
        usleep(100000);
        timeout++;
    }

    return -1;
}

// Dibujar recuadro horizontal de pedido
void dibujar_recuadro_pedido_horizontal(int x, int y, int ancho, PedidoMsg *pedido, int seleccionado) {
    const char* estados[] = {"PENDIENTE", "EN PROGRESO", "LISTO"};
    int color_estado[] = {3, 5, 2}; // Rojo, Amarillo, Verde

    int color_borde = seleccionado ? 5 : 10;

    attron(COLOR_PAIR(color_borde) | (seleccionado ? A_BOLD : 0));

    // Borde superior
    mvaddch(y, x, '+');
    for(int i = 1; i < ancho-1; i++) mvaddch(y, x+i, '=');
    mvaddch(y, x+ancho-1, '+');

    // Primera línea de contenido
    mvaddch(y+1, x, '|');
    mvaddch(y+1, x+ancho-1, '|');

    // Segunda línea de contenido
    mvaddch(y+2, x, '|');
    mvaddch(y+2, x+ancho-1, '|');

    // Tercera línea de contenido
    mvaddch(y+3, x, '|');
    mvaddch(y+3, x+ancho-1, '|');

    // Borde inferior
    mvaddch(y+4, x, '+');
    for(int i = 1; i < ancho-1; i++) mvaddch(y+4, x+i, '=');
    mvaddch(y+4, x+ancho-1, '+');

    attroff(COLOR_PAIR(color_borde) | (seleccionado ? A_BOLD : 0));

    if(pedido) {
        // Línea 1: ID y Estado
        attron(COLOR_PAIR(color_estado[pedido->estado]) | A_BOLD);
        mvprintw(y+1, x+2, "PEDIDO #%d", pedido->id);

        char estado_text[30];
        snprintf(estado_text, 30, "[%s]", estados[pedido->estado]);
        mvprintw(y+1, x+ancho-strlen(estado_text)-2, "%s", estado_text);
        attroff(COLOR_PAIR(color_estado[pedido->estado]) | A_BOLD);

        // Línea 2: Mesa y Mesero
        attron(COLOR_PAIR(10));
        char info_line[256];
        snprintf(info_line, 256, "Mesa: %s | Mesero: %s", pedido->mesa, pedido->mesero);

        if(strlen(info_line) > ancho-4) {
            info_line[ancho-4] = '\0';
        }
        mvprintw(y+2, x+2, "%s", info_line);

        // Línea 3: Items
        char items_line[512] = "Items: ";
        int offset = 7;

        for(int i = 0; i < pedido->num_items && offset < 500; i++) {
            char item_text[150];
            if(i > 0) {
                snprintf(item_text, 150, " | %dx %s",
                        pedido->items[i].cantidad,
                        pedido->items[i].nombre_producto);
            } else {
                snprintf(item_text, 150, "%dx %s",
                        pedido->items[i].cantidad,
                        pedido->items[i].nombre_producto);
            }

            if(offset + strlen(item_text) < 500) {
                strcat(items_line, item_text);
                offset += strlen(item_text);
            }
        }

        if(strlen(items_line) > ancho-4) {
            items_line[ancho-4] = '\0';
        }
        mvprintw(y+3, x+2, "%s", items_line);
        attroff(COLOR_PAIR(10));
    }
}

void interfaz_cocina_ejecutar(USUARIO* usuario) {
    int salir = 0;

    while(!salir) {
        // Obtener pedidos del servidor
        Peticion pet;
        Respuesta resp;
        memset(&pet, 0, sizeof(Peticion));
        memset(&resp, 0, sizeof(Respuesta));

        pet.operacion = OP_LISTAR_PEDIDOS;

        if(enviar_peticion_cocina(&pet, &resp) != 0 || resp.codigo != RESP_OK) {
            ui_show_error("Error al cargar pedidos");
            sleep(2);
            continue;
        }

        ui_clear_screen();

        // Encabezado
        attron(COLOR_PAIR(5) | A_BOLD);
        ui_print_centered(2, "=== SISTEMA DE COCINA ===");
        attroff(COLOR_PAIR(5) | A_BOLD);

        attron(COLOR_PAIR(10));
        char info[256];
        snprintf(info, sizeof(info), "Cocinero: %s", usuario->name);
        ui_print_centered(3, info);
        attroff(COLOR_PAIR(10));

        // Contador de pedidos
        int pendientes = 0, en_progreso = 0;
        for(int i = 0; i < resp.num_pedidos; i++) {
            if(resp.pedidos[i].estado == 0) pendientes++;
            else if(resp.pedidos[i].estado == 1) en_progreso++;
        }

        attron(COLOR_PAIR(3) | A_BOLD);
        mvprintw(4, 5, "%d PENDIENTES", pendientes);
        attroff(COLOR_PAIR(3) | A_BOLD);

        attron(COLOR_PAIR(5) | A_BOLD);
        mvprintw(4, 25, "%d EN PROGRESO", en_progreso);
        attroff(COLOR_PAIR(5) | A_BOLD);

        mvprintw(5, 0, " ");
        for(int i = 0; i < COLS; i++) addch('=');

        if(resp.num_pedidos == 0) {
            attron(COLOR_PAIR(10));
            ui_print_centered(LINES/2, "No hay pedidos activos");
            attroff(COLOR_PAIR(10));

            attron(COLOR_PAIR(10));
            mvprintw(LINES-3, 2, "Presione R para actualizar | ESC para salir");
            attroff(COLOR_PAIR(10));

            refresh();

            int key = getch();
            if(key == 27) { // ESC
                ui_show_info("Cerrando sesion...");
                salir = 1;
            }
            continue;
        }

        //
        int ancho_recuadro = COLS - 6; // Usar casi todo el ancho
        int alto_recuadro = 5; // Altura fija para recuadros horizontales
        int y_inicio = 7;
        int espacio_y = 1;

        // Calcular cuántos pedidos caben en pantalla
        int espacio_disponible = LINES - y_inicio - 4; // Espacio para pedidos
        int pedidos_por_pantalla = espacio_disponible / (alto_recuadro + espacio_y);
        if(pedidos_por_pantalla < 1) pedidos_por_pantalla = 1;

        int seleccionado = 0;
        int offset_scroll = 0;
        int key;
        int actualizar = 0;

        while(!actualizar && !salir) {
            // Limpiar área de pedidos
            for(int i = y_inicio; i < LINES-4; i++) {
                move(i, 0);
                clrtoeol();
            }

            // Calcular rango visible
            if(seleccionado < offset_scroll) {
                offset_scroll = seleccionado;
            }
            if(seleccionado >= offset_scroll + pedidos_por_pantalla) {
                offset_scroll = seleccionado - pedidos_por_pantalla + 1;
            }

            int inicio = offset_scroll;
            int fin = inicio + pedidos_por_pantalla;
            if(fin > resp.num_pedidos) fin = resp.num_pedidos;

            // Dibujar pedidos visibles
            for(int i = inicio; i < fin; i++) {
                int y_pos = y_inicio + (i - inicio) * (alto_recuadro + espacio_y);
                dibujar_recuadro_pedido_horizontal(3, y_pos, ancho_recuadro, &resp.pedidos[i], i == seleccionado);
            }

            // Instrucciones
            attron(COLOR_PAIR(10));
            mvprintw(LINES-3, 2, "Flechas ARRIBA/ABAJO: navegar | 1: En progreso | 2: Listo | R: Actualizar | ESC: Salir");
            attroff(COLOR_PAIR(10));

            // Indicador de posición y scroll
            attron(COLOR_PAIR(10));
            mvprintw(LINES-2, 2, "Pedido %d de %d", seleccionado + 1, resp.num_pedidos);

            if(resp.num_pedidos > pedidos_por_pantalla) {
                mvprintw(LINES-2, COLS-30, "Mostrando %d-%d de %d", inicio+1, fin, resp.num_pedidos);
            }
            attroff(COLOR_PAIR(10));

            refresh();
            key = getch();

            switch(key) {
                case KEY_UP:
                    if(seleccionado > 0) {
                        seleccionado--;
                    }
                    break;

                case KEY_DOWN:
                    if(seleccionado < resp.num_pedidos - 1) {
                        seleccionado++;
                    }
                    break;

                case KEY_PPAGE: // Page Up
                    seleccionado -= pedidos_por_pantalla;
                    if(seleccionado < 0) seleccionado = 0;
                    break;

                case KEY_NPAGE: // Page Down
                    seleccionado += pedidos_por_pantalla;
                    if(seleccionado >= resp.num_pedidos) seleccionado = resp.num_pedidos - 1;
                    break;

                case KEY_HOME:
                    seleccionado = 0;
                    break;

                case KEY_END:
                    seleccionado = resp.num_pedidos - 1;
                    break;

                case '1': // Marcar en progreso
                    if(resp.pedidos[seleccionado].estado == 0) {
                        Peticion pet_estado;
                        Respuesta resp_estado;
                        memset(&pet_estado, 0, sizeof(Peticion));
                        memset(&resp_estado, 0, sizeof(Respuesta));

                        pet_estado.operacion = OP_CAMBIAR_ESTADO_PEDIDO;
                        pet_estado.pedido_id = resp.pedidos[seleccionado].id;
                        pet_estado.nuevo_estado = 1; // En progreso

                        if(enviar_peticion_cocina(&pet_estado, &resp_estado) == 0) {
                            actualizar = 1;
                        }
                    }
                    break;

                case '2': // Marcar listo
                    if(resp.pedidos[seleccionado].estado != 2) {
                        Peticion pet_estado;
                        Respuesta resp_estado;
                        memset(&pet_estado, 0, sizeof(Peticion));
                        memset(&resp_estado, 0, sizeof(Respuesta));

                        pet_estado.operacion = OP_CAMBIAR_ESTADO_PEDIDO;
                        pet_estado.pedido_id = resp.pedidos[seleccionado].id;
                        pet_estado.nuevo_estado = 2; // Listo

                        if(enviar_peticion_cocina(&pet_estado, &resp_estado) == 0) {
                            actualizar = 1;
                        }
                    }
                    break;

                case 'r':
                case 'R':
                    actualizar = 1;
                    break;

                case 27: // ESC
                    ui_show_info("Cerrando sesion...");
                    salir = 1;
                    break;
            }
        }
    }
}

// Funciones de la interfaz de mesero

// Enviar petición y esperar respuesta
int enviar_peticion_mesero(Peticion *pet, Respuesta *resp) {
    if (mi_slot == -1) return -1;

    down(semid_servidor);
    datos_servidor->clientes[mi_slot].peticion = *pet;
    datos_servidor->clientes[mi_slot].peticion_lista = 1;
    up(semid_servidor);

    int timeout = 0;
    while (timeout < 100) {
        down(semid_servidor);
        if (datos_servidor->clientes[mi_slot].respuesta_lista == 1) {
            *resp = datos_servidor->clientes[mi_slot].respuesta;
            datos_servidor->clientes[mi_slot].respuesta_lista = 0;
            up(semid_servidor);
            return 0;
        }
        up(semid_servidor);
        usleep(100000);
        timeout++;
    }

    return -1;
}

// Calcular layout dinámico
void calcular_layout(int num_productos, int *cols, int *filas, int *ancho, int *alto) {
    int espacio_util_x = COLS - 4;

    // Dimensiones minimas y maximas para los recuadros
    int ancho_min = 30;
    int ancho_max = 50;
    int alto_fijo = 8;

    // Calcular cuántas columnas caben
    int max_cols = espacio_util_x / (ancho_min + 2);
    if(max_cols < 1) max_cols = 1;
    if(max_cols > 4) max_cols = 4;

    // Calcular ancho optimo para las columnas
    int ancho_calculado = (espacio_util_x - (max_cols - 1) * 2) / max_cols;
    if(ancho_calculado > ancho_max) {
        ancho_calculado = ancho_max;
        // Recalcular columnas con ancho maximo
        max_cols = espacio_util_x / (ancho_max + 2);
        if(max_cols < 1) max_cols = 1;
    }

    // Calcular filas necesarias
    int filas_necesarias = (num_productos + max_cols - 1) / max_cols;

    *cols = max_cols;
    *filas = filas_necesarias;
    *ancho = ancho_calculado;
    *alto = alto_fijo;
}

// Verificar disponibilidad de un producto
int verificar_disponibilidad_producto(int id_producto) {
    Peticion pet;
    Respuesta resp;
    memset(&pet, 0, sizeof(Peticion));
    memset(&resp, 0, sizeof(Respuesta));

    pet.operacion = OP_VERIFICAR_DISPONIBILIDAD_PRODUCTO;
    pet.producto_id = id_producto;
    pet.cantidad = 1; // Verificar disponibilidad para al menos 1 unidad

    if(enviar_peticion_mesero(&pet, &resp) == 0) {
        return resp.codigo == RESP_OK; // Si la respuesta es OK (0), está disponible (1); si es error, no está disponible (0)
    }
    return 1; // Si hay error en la comunicación, asumimos que está disponible
}

// Dibujar recuadro de producto con texto ajustado
void dibujar_recuadro_producto(int x, int y, int ancho, int alto, ProductoMsg *prod, int seleccionado, int disponible) {
    int color;
    if(!disponible) {
        color = 8; // Color para productos no disponibles (gris/apagado)
    } else {
        color = seleccionado ? 5 : 10;
    }

    attron(COLOR_PAIR(color) | (seleccionado ? A_BOLD : (!disponible ? A_DIM : 0)));

    // Borde superior
    mvaddch(y, x, '+');
    for(int i = 1; i < ancho-1; i++) mvaddch(y, x+i, '-');
    mvaddch(y, x+ancho-1, '+');

    // Contenido
    for(int i = 1; i < alto-1; i++) {
        mvaddch(y+i, x, '|');
        mvaddch(y+i, x+ancho-1, '|');
    }

    // Borde inferior
    mvaddch(y+alto-1, x, '+');
    for(int i = 1; i < ancho-1; i++) mvaddch(y+alto-1, x+i, '-');
    mvaddch(y+alto-1, x+ancho-1, '+');

    attroff(COLOR_PAIR(color) | (seleccionado ? A_BOLD : (!disponible ? A_DIM : 0)));

    // Contenido del producto
    if(prod) {
        int ancho_texto = ancho - 4;

        attron(COLOR_PAIR(color) | (seleccionado ? A_BOLD : (!disponible ? A_DIM : 0)));
        mvprintw(y+1, x+2, "ID: %d", prod->id);
        attroff(COLOR_PAIR(color) | (seleccionado ? A_BOLD : (!disponible ? A_DIM : 0)));

        attron(COLOR_PAIR(color) | (!disponible ? A_DIM : 0));

        // Nombre
        char nombre_ajustado[100];
        strncpy(nombre_ajustado, prod->nombre, ancho_texto);
        nombre_ajustado[ancho_texto] = '\0';
        mvprintw(y+2, x+2, "%s", nombre_ajustado);

        // Descripción
        char desc[200];
        strncpy(desc, prod->descripcion, 199);
        desc[199] = '\0';

        int linea_actual = y+3;
        int max_lineas = 3;
        int pos = 0;
        int len_desc = strlen(desc);

        for(int i = 0; i < max_lineas && pos < len_desc && linea_actual < y+alto-2; i++) {
            char linea[100];
            int chars_a_copiar = ancho_texto;
            if(pos + chars_a_copiar > len_desc) {
                chars_a_copiar = len_desc - pos;
            }

            // Intentar cortar en un espacio si es posible
            if(pos + chars_a_copiar < len_desc && i < max_lineas - 1) {
                int ultimo_espacio = -1;
                for(int j = 0; j < chars_a_copiar; j++) {
                    if(desc[pos + j] == ' ') {
                        ultimo_espacio = j;
                    }
                }
                if(ultimo_espacio > chars_a_copiar / 2) {
                    chars_a_copiar = ultimo_espacio + 1;
                }
            }

            strncpy(linea, desc + pos, chars_a_copiar);
            linea[chars_a_copiar] = '\0';

            mvprintw(linea_actual, x+2, "%s", linea);
            pos += chars_a_copiar;
            linea_actual++;
        }

        // Precio
        attron(A_BOLD | (!disponible ? A_DIM : 0));
        mvprintw(y+alto-2, x+2, "$ %.2f", prod->precio);
        attroff(A_BOLD | (!disponible ? A_DIM : 0));

        attroff(COLOR_PAIR(color) | (!disponible ? A_DIM : 0));
    }
}

void interfaz_mesero_ejecutar(USUARIO* usuario) {
    int salir = 0;

    while(!salir) {
        ui_clear_screen();

        // Encabezado
        attron(COLOR_PAIR(1) | A_BOLD);
        ui_print_centered(2, "=== SISTEMA DE MESEROS ===");
        attroff(COLOR_PAIR(1) | A_BOLD);

        attron(COLOR_PAIR(10));
        char info[256];
        snprintf(info, sizeof(info), "Mesero: %s", usuario->name);
        ui_print_centered(3, info);
        attroff(COLOR_PAIR(10));

        mvprintw(4, 0, " ");
        for(int i = 0; i < COLS; i++) addch('-');

        // Menú
        char *opciones[] = {
            "Tomar Nueva Orden",
            "Ver Mis Ordenes",
            "Cerrar Sesion"
        };

        for(int i = 0; i < 3; i++) {
            attron(COLOR_PAIR(10));
            ui_print_centered(7 + i*2, opciones[i]);
            attroff(COLOR_PAIR(10));
        }

        ui_print_footer("Use flechas para navegar, ENTER para seleccionar");
        refresh();

        int opcion = ui_show_menu("", opciones, 3);

        switch(opcion) {
            case 0: { // Tomar Nueva Orden
                // Obtener productos del servidor
                Peticion pet;
                Respuesta resp;
                memset(&pet, 0, sizeof(Peticion));
                memset(&resp, 0, sizeof(Respuesta));

                pet.operacion = OP_LISTAR_PRODUCTOS;

                if(enviar_peticion_mesero(&pet, &resp) != 0 || resp.codigo != RESP_OK) {
                    ui_show_error("Error al cargar productos");
                    break;
                }

                if(resp.num_productos == 0) {
                    ui_show_info("No hay productos disponibles");
                    break;
                }

                // Calcular layout
                int columnas, filas, ancho_rec, alto_rec;
                calcular_layout(resp.num_productos, &columnas, &filas, &ancho_rec, &alto_rec);

                int margen_x = 2;
                int margen_y = 6;
                int espacio_x = 2;
                int espacio_y = 1;

                int espacio_disponible = LINES - margen_y - 6;
                int filas_por_pantalla = espacio_disponible / (alto_rec + espacio_y);
                if(filas_por_pantalla < 1) filas_por_pantalla = 1;
                int productos_por_pantalla = filas_por_pantalla * columnas;

                int seleccionado = 0;
                int offset_scroll = 0;
                int cantidad_productos[MAX_PRODUCTOS_RESPUESTA];
                memset(cantidad_productos, 0, sizeof(cantidad_productos));
                int key;
                int tomar_orden = 0;

                while(!tomar_orden) {
                    ui_clear_screen();
                    ui_print_title("Seleccionar Productos");

                    attron(COLOR_PAIR(10));
                    mvprintw(3, 2, "Flechas: navegar | ENTER: +1 | -: -1 | O: ordenar | ESC: cancelar");
                    attroff(COLOR_PAIR(10));

                    // Calcular rango visible
                    int fila_seleccionada = seleccionado / columnas;
                    int fila_inicio = offset_scroll;
                    int fila_fin = fila_inicio + filas_por_pantalla;

                    if(fila_seleccionada < fila_inicio) {
                        offset_scroll = fila_seleccionada;
                        fila_inicio = offset_scroll;
                        fila_fin = fila_inicio + filas_por_pantalla;
                    }
                    if(fila_seleccionada >= fila_fin) {
                        offset_scroll = fila_seleccionada - filas_por_pantalla + 1;
                        fila_inicio = offset_scroll;
                        fila_fin = fila_inicio + filas_por_pantalla;
                    }

                    // Dibujar productos visibles
                    for(int i = 0; i < resp.num_productos; i++) {
                        int fila = i / columnas;
                        int columna = i % columnas;

                        // Solo dibujar si está en rango visible
                        if(fila >= fila_inicio && fila < fila_fin) {
                            int x = margen_x + columna * (ancho_rec + espacio_x);
                            int y = margen_y + (fila - fila_inicio) * (alto_rec + espacio_y);

                            // Verificar disponibilidad del producto
                            int disponible = verificar_disponibilidad_producto(resp.productos[i].id);

                            dibujar_recuadro_producto(x, y, ancho_rec, alto_rec, &resp.productos[i], i == seleccionado, disponible);

                            // Mostrar cantidad si es mayor a 0
                            if(cantidad_productos[i] > 0) {
                                attron(COLOR_PAIR(2) | A_BOLD);
                                mvprintw(y+alto_rec-1, x+ancho_rec-8, "x%d", cantidad_productos[i]);
                                attroff(COLOR_PAIR(2) | A_BOLD);
                            }
                        }
                    }

                    // Mostrar resumen
                    int total_items = 0;
                    float total_precio = 0.0;
                    for(int i = 0; i < resp.num_productos; i++) {
                        if(cantidad_productos[i] > 0) {
                            total_items += cantidad_productos[i];
                            total_precio += resp.productos[i].precio * cantidad_productos[i];
                        }
                    }

                    attron(COLOR_PAIR(9) | A_BOLD);
                    mvprintw(LINES-5, 2, "Items totales: %d", total_items);
                    mvprintw(LINES-4, 2, "Total: $%.2f", total_precio);
                    attroff(COLOR_PAIR(9) | A_BOLD);

                    // Indicador de scroll
                    if(filas > filas_por_pantalla) {
                        attron(COLOR_PAIR(10));
                        mvprintw(LINES-3, 2, "Producto %d de %d | Fila %d de %d",
                                seleccionado + 1, resp.num_productos, fila_seleccionada + 1, filas);
                        attroff(COLOR_PAIR(10));
                    }

                    refresh();
                    key = getch();

                    switch(key) {
                        case KEY_UP:
                            if(seleccionado >= columnas)
                                seleccionado -= columnas;
                            break;
                        case KEY_DOWN:
                            if(seleccionado + columnas < resp.num_productos)
                                seleccionado += columnas;
                            break;
                        case KEY_LEFT:
                            if(seleccionado > 0)
                                seleccionado--;
                            break;
                        case KEY_RIGHT:
                            if(seleccionado < resp.num_productos - 1)
                                seleccionado++;
                            break;
                        case KEY_PPAGE: // Page Up
                            seleccionado -= productos_por_pantalla;
                            if(seleccionado < 0) seleccionado = 0;
                            break;
                        case KEY_NPAGE: // Page Down
                            seleccionado += productos_por_pantalla;
                            if(seleccionado >= resp.num_productos)
                                seleccionado = resp.num_productos - 1;
                            break;
                        case KEY_HOME:
                            seleccionado = 0;
                            break;
                        case KEY_END:
                            seleccionado = resp.num_productos - 1;
                            break;
                        case 10: // ENTER
                            cantidad_productos[seleccionado]++;
                            break;
                        case '-': // Quitar
                            if(cantidad_productos[seleccionado] > 0)
                                cantidad_productos[seleccionado]--;
                            break;
                        case '+':
                        case '=':
                            cantidad_productos[seleccionado] += 5;
                            break;
                        case 'o':
                        case 'O': // Ordenar
                            if(total_items > 0) {
                                tomar_orden = 1;
                            }
                            break;
                        case 27: // ESC
                            tomar_orden = -1;
                            break;
                    }
                }

                if(tomar_orden == 1) {
                    // Pedir número de mesa
                    ui_clear_screen();
                    ui_print_title("Confirmar Orden");

                    attron(COLOR_PAIR(10) | A_BOLD);
                    mvprintw(6, 5, "Numero de mesa:");
                    attroff(COLOR_PAIR(10) | A_BOLD);

                    char mesa[50];
                    ui_read_input(6, 21, mesa, 50, 0);

                    if(strlen(mesa) > 0) {
                        // Crear pedido
                        Peticion pet_orden;
                        Respuesta resp_orden;
                        memset(&pet_orden, 0, sizeof(Peticion));
                        memset(&resp_orden, 0, sizeof(Respuesta));

                        pet_orden.operacion = OP_CREAR_PEDIDO;
                        strncpy(pet_orden.mesa, mesa, 49);
                        strncpy(pet_orden.name, usuario->name, MAX_CHAIN_SIZE-1);

                        int item_count = 0;
                        for(int i = 0; i < resp.num_productos; i++) {
                            if(cantidad_productos[i] > 0) {
                                pet_orden.items[item_count].producto_id = resp.productos[i].id;
                                strncpy(pet_orden.items[item_count].nombre_producto, resp.productos[i].nombre, 99);
                                pet_orden.items[item_count].cantidad = cantidad_productos[i];
                                item_count++;
                            }
                        }
                        pet_orden.num_items = item_count;

                        if(enviar_peticion_mesero(&pet_orden, &resp_orden) == 0 && resp_orden.codigo == RESP_OK) {
                            char msg[200];
                            snprintf(msg, 200, "Pedido #%d creado exitosamente!", resp_orden.pedido_id_creado);
                            ui_show_success(msg);
                        } else {
                            ui_show_error("Error al crear pedido");
                        }
                    }
                }
                break;
            }

            case 1: { // Ver Mis Ordenes
                Peticion pet;
                Respuesta resp;
                memset(&pet, 0, sizeof(Peticion));
                memset(&resp, 0, sizeof(Respuesta));

                pet.operacion = OP_LISTAR_PEDIDOS;

                if(enviar_peticion_mesero(&pet, &resp) != 0 || resp.codigo != RESP_OK) {
                    ui_show_error("Error al cargar pedidos");
                    break;
                }

                ui_clear_screen();
                ui_print_title("Mis Ordenes Activas");

                if(resp.num_pedidos == 0) {
                    attron(COLOR_PAIR(10));
                    mvprintw(8, 5, "No hay ordenes activas");
                    attroff(COLOR_PAIR(10));
                } else {
                    int y = 6;
                    for(int i = 0; i < resp.num_pedidos; i++) {
                        // Filtrar por mesero
                        if(strcmp(resp.pedidos[i].mesero, usuario->name) == 0) {
                            const char* estados[] = {"PENDIENTE", "EN PROGRESO", "LISTO"};
                            int color = (resp.pedidos[i].estado == 0) ? 3 : (resp.pedidos[i].estado == 1) ? 5 : 2;

                            attron(COLOR_PAIR(color) | A_BOLD);
                            mvprintw(y, 5, "Pedido #%d - Mesa %s - %s",
                                    resp.pedidos[i].id,
                                    resp.pedidos[i].mesa,
                                    estados[resp.pedidos[i].estado]);
                            attroff(COLOR_PAIR(color) | A_BOLD);
                            y++;

                            attron(COLOR_PAIR(10));
                            for(int j = 0; j < resp.pedidos[i].num_items; j++) {
                                mvprintw(y, 7, "- %dx %s",
                                        resp.pedidos[i].items[j].cantidad,
                                        resp.pedidos[i].items[j].nombre_producto);
                                y++;
                            }
                            attroff(COLOR_PAIR(10));
                            y++;
                        }
                    }
                }

                ui_print_footer("Presione cualquier tecla para continuar...");
                refresh();
                ui_wait_key();
                break;
            }

            case 2: // Cerrar Sesión
                ui_show_info("Cerrando sesion...");
                salir = 1;
                break;
        }
    }
}

// Funciones para administración de usuarios que faltaban
// Calcular layout dinámico para usuarios
void calcular_layout_usuarios(int num_usuarios, int *cols, int *filas, int *ancho, int *alto) {
    int espacio_util_x = COLS - 4;

    // Dimensiones minimas y maximas para los recuadros
    int ancho_min = 35;
    int ancho_max = 55;
    int alto_fijo = 6; // Reducido de 8 a 6 para usar mejor el espacio vertical

    // Calcular cuántas columnas caben
    int max_cols = espacio_util_x / (ancho_min + 2);
    if(max_cols < 1) max_cols = 1;
    if(max_cols > 4) max_cols = 4;

    // Calcular ancho optimo para las columnas
    int ancho_calculado = (espacio_util_x - (max_cols - 1) * 2) / max_cols;
    if(ancho_calculado > ancho_max) {
        ancho_calculado = ancho_max;
        // Recalcular columnas con ancho maximo
        max_cols = espacio_util_x / (ancho_max + 2);
        if(max_cols < 1) max_cols = 1;
    }

    // Calcular filas necesarias (añadir 1 para la tarjeta "Agregar nuevo usuario")
    int filas_necesarias = (num_usuarios + 1 + max_cols - 1) / max_cols;

    *cols = max_cols;
    *filas = filas_necesarias;
    *ancho = ancho_calculado;
    *alto = alto_fijo;
}

void mostrar_menu_usuarios() {
    usuario_cargar(); // Cargar usuarios desde archivo

    int seleccionado = 0;
    int tecla;

    int offset = 0; // Offset para la paginación
    int total_usuarios = usuario_get_total() + 1; // Incluyendo la tarjeta de "nuevo usuario"

    while(1) {
        // Actualizar el número de usuarios en cada iteración del bucle
        total_usuarios = usuario_get_total() + 1; // Actualizar para incluir la tarjeta de nuevo usuario
        ui_clear_screen();
        ui_print_title("ADMINISTRAR USUARIOS");

        // Calcular layout
        int columnas, filas, ancho_rec, alto_rec;
        calcular_layout_usuarios(usuario_get_total(), &columnas, &filas, &ancho_rec, &alto_rec);

        // Reducir la separación vertical para aprovechar mejor el espacio
        int espacio_y = 0; // Reducir espacio vertical
        int espacio_x = 2;
        int margen_x = 2;
        int margen_y = 6;

        // Calcular cuántos usuarios se pueden mostrar en pantalla
        int filas_visibles = (LINES - 12) / alto_rec; // Ajustar para reducir espacio vertical
        if(filas_visibles < 1) filas_visibles = 1;
        int usuarios_por_pagina = filas_visibles * columnas;

        // Ajustar la selección para que esté dentro del rango
        if(seleccionado >= total_usuarios) {
            seleccionado = total_usuarios - 1;
        }

        // Calcular el offset basado en la selección
        int fila_seleccionada = seleccionado / columnas;
        int fila_inicio = offset / columnas;
        int fila_fin = fila_inicio + filas_visibles;

        if(fila_seleccionada < fila_inicio) {
            offset = (fila_seleccionada * columnas);
        }
        if(fila_seleccionada >= fila_fin) {
            offset = ((fila_seleccionada - filas_visibles + 1) * columnas);
        }

        // Asegurar que offset esté dentro de los límites
        if(offset > total_usuarios - usuarios_por_pagina) {
            offset = (total_usuarios > usuarios_por_pagina) ? total_usuarios - usuarios_por_pagina : 0;
        }
        if(offset < 0) offset = 0;

        // Mostrar usuarios en forma de rejilla
        int mostrados = 0;
        int usuarios_mostrados = 0;
        for(int i = offset; i < total_usuarios && usuarios_mostrados < usuarios_por_pagina; i++, usuarios_mostrados++) {
            int fila = mostrados / columnas;
            int columna = mostrados % columnas;
            int x = margen_x + columna * (ancho_rec + espacio_x);
            int y = margen_y + fila * (alto_rec + espacio_y);

            if(i == 0) {
                // Primera tarjeta: "Agregar nuevo usuario"
                dibujar_recuadro_usuario_horizontal(x, y, ancho_rec, NULL, -1, i == seleccionado);
            } else {
                // Usuarios existentes
                USUARIO *usr = usuario_get_by_index(i - 1); // -1 porque el 0 es la tarjeta de nuevo usuario
                if(usr != NULL) {
                    dibujar_recuadro_usuario_horizontal(x, y, ancho_rec, usr, i - 1, i == seleccionado);
                }
            }
            mostrados++;
        }

        // Mostrar información de navegación
        if(total_usuarios > usuarios_por_pagina) {
            int pagina_actual = (offset / usuarios_por_pagina) + 1;
            int total_paginas = (total_usuarios + usuarios_por_pagina - 1) / usuarios_por_pagina;
            attron(COLOR_PAIR(10));
            mvprintw(LINES - 4, 2, "Usuario %d/%d | Pagina %d/%d", seleccionado + 1, total_usuarios, pagina_actual, total_paginas);
            attroff(COLOR_PAIR(10));
        }

        ui_print_footer("Use flechas para navegar, ENTER para seleccionar, ESC para salir");
        refresh();

        tecla = getch();

        switch(tecla) {
            case KEY_UP:
                if(seleccionado >= columnas) {
                    seleccionado -= columnas;
                } else if(seleccionado > 0) {
                    // Si está en la primera fila y puede moverse hacia arriba, mover al inicio de la lista
                    seleccionado = 0;
                }
                break;
            case KEY_DOWN:
                if(seleccionado + columnas < total_usuarios) {
                    seleccionado += columnas;
                } else if(seleccionado < total_usuarios - 1) {
                    // Si está en la última fila y puede moverse hacia abajo
                    seleccionado = total_usuarios - 1;
                }
                break;
            case KEY_LEFT:
                if(seleccionado > 0) {
                    seleccionado--;
                }
                break;
            case KEY_RIGHT:
                if(seleccionado < total_usuarios - 1) {
                    seleccionado++;
                }
                break;
            case 10: // Enter
                if(seleccionado == 0) {
                    // Bucle principal para manejar reintentos
                    int reintentar = 1;
                    while(reintentar) {
                        reintentar = 0; // Resetear para este intento

                        // Agregar nuevo usuario
                        char name[MAX_CHAIN_SIZE];
                        char user[MAX_CHAIN_SIZE];
                        char pass[32];
                        char mail[MAX_CHAIN_SIZE];
                        char telf[MAX_CHAIN_SIZE];
                        // char tipo_str[10];
                        int tipo;
                        char mensaje_error[256];

                        ui_clear_screen();
                        ui_print_title("Agregar Nuevo Usuario");

                        attron(COLOR_PAIR(10) | A_BOLD);
                        mvprintw(6, 5, "Nombre completo:");
                        mvprintw(8, 5, "Usuario:");
                        mvprintw(10, 5, "Contrasena:");
                        mvprintw(12, 5, "Email:");
                        mvprintw(14, 5, "Telefono:");
                        mvprintw(16, 5, "Tipo:");
                        attroff(COLOR_PAIR(10) | A_BOLD);

                        ui_print_footer("Presione ESC para cancelar");
                        refresh();

                        // Leer nombre
                        ui_read_input(6, 22, name, MAX_CHAIN_SIZE, 0);
                        if(strlen(name) == 0) break;

                        // Leer usuario
                        ui_read_input(8, 14, user, MAX_CHAIN_SIZE, 0);
                        if(strlen(user) == 0) break;

                        // Verificar si existe
                        Peticion pet_ver;
                        Respuesta resp_ver;
                        memset(&pet_ver, 0, sizeof(Peticion));
                        memset(&resp_ver, 0, sizeof(Respuesta));

                        pet_ver.operacion = OP_VERIFICAR_USUARIO;
                        strncpy(pet_ver.user, user, MAX_CHAIN_SIZE-1);

                        int error_encontrado = 0;

                        if (enviar_peticion(&pet_ver, &resp_ver) == 0 && resp_ver.codigo == RESP_USUARIO_EXISTE) {
                            strcpy(mensaje_error, "El usuario ya existe");
                            error_encontrado = 1;
                        }

                        if(!error_encontrado) {
                            // Mostrar requisitos de contraseña - cálculo preciso para colocarla a la derecha
                            // int req_width = 26; // Ancho de la caja
                            int req_x = COLS - 29; // Ajustar la posición para que no esté tan a la derecha
                            if(req_x < 0) req_x = 0; // Asegurar que no se coloque fuera de la pantalla
                            attron(COLOR_PAIR(6) | A_BOLD);
                            mvprintw(1, req_x, "+==========================+");
                            mvprintw(2, req_x, "| REQUISITOS DE CONTRASENA |");
                            mvprintw(3, req_x, "+==========================+");
                            attroff(COLOR_PAIR(6) | A_BOLD);
                            attron(COLOR_PAIR(6));
                            mvprintw(4, req_x, "| > Minimo 8 caracteres    |");
                            mvprintw(5, req_x, "| > 1 mayuscula (A-Z)      |");
                            mvprintw(6, req_x, "| > 1 minuscula (a-z)      |");
                            mvprintw(7, req_x, "| > 1 numero (0-9)         |");
                            mvprintw(8, req_x, "| > 2 simbolos (*/_-+.)    |");
                            attroff(COLOR_PAIR(6));
                            attron(COLOR_PAIR(6) | A_BOLD);
                            mvprintw(9, req_x, "+==========================+");
                            attroff(COLOR_PAIR(6) | A_BOLD);
                            refresh();

                            // Leer contraseña
                            ui_read_input(10, 17, pass, sizeof(pass), 1);
                            if(strlen(pass) == 0) break;

                            if(validar_password(pass) != 0) {
                                strcpy(mensaje_error, "La contrasena es invalida");

                                ui_clear_screen();
                                
                                ui_print_colored(LINES/2 - 3, (COLS-strlen(mensaje_error))/2, mensaje_error, 3);

                                // Mensaje de reintento
                                mvprintw(LINES/2 + 1, (COLS-strlen("¿Intentar otra vez?"))/2, "¿Intentar otra vez?");

                                int centro_x = (COLS - 32) / 2; // Centrado para dos botones de 12 caracteres con separación de 20
                                int boton_y = LINES/2 + 3;
                                int boton_ancho = 12;
                                int boton_alto = 3;

                                int seleccion = 0; // 0 para Sí, 1 para No
                                int salir_seleccion = 0;

                                while(!salir_seleccion) {
                                    // Dibujar botones usando la función centralizada
                                    dibujar_botones_si_no(centro_x, boton_y, seleccion, boton_ancho, boton_alto);

                                    ui_print_footer("Use flechas para navegar y Enter para seleccionar");
                                    refresh();

                                    int tecla = getch();
                                    if(tecla == KEY_LEFT || tecla == KEY_RIGHT) {
                                        seleccion = 1 - seleccion; // Alternar entre 0 y 1
                                    } else if(tecla == 10) { // Enter
                                        if(seleccion == 0) { // Sí
                                            salir_seleccion = 1;
                                            reintentar = 1; // Volver a intentar
                                        } else { // No
                                            salir_seleccion = 1;
                                            reintentar = 0; // No volver a intentar
                                        }
                                    } else if(tecla == 27) { // ESC
                                        salir_seleccion = 1;
                                        reintentar = 0; // No volver a intentar
                                    }
                                }
                                if(reintentar) continue; // Reiniciar el proceso de creación
                                else break; // Salir del proceso
                            }
                        }

                        if(!error_encontrado) {
                            // Limpiar requisitos anteriores y mostrar requisitos de email
                            for(int i = 1; i <= 9; i++) {
                                move(i, COLS - 29);
                                clrtoeol();
                            }

                            attron(COLOR_PAIR(6) | A_BOLD);
                            mvprintw(1, COLS - 29, "+==========================+");
                            mvprintw(2, COLS - 29, "|   REQUISITOS DE EMAIL    |");
                            mvprintw(3, COLS - 29, "+==========================+");
                            attroff(COLOR_PAIR(6) | A_BOLD);
                            attron(COLOR_PAIR(6));
                            mvprintw(4, COLS - 29, "| > 1 arroba (@)           |");
                            mvprintw(5, COLS - 29, "| > Al menos 1 punto (.)   |");
                            mvprintw(6, COLS - 29, "|                          |");
                            mvprintw(7, COLS - 29, "| Ejemplo:                 |");
                            mvprintw(8, COLS - 29, "| usuario@correo.com       |");
                            attroff(COLOR_PAIR(6));
                            attron(COLOR_PAIR(6) | A_BOLD);
                            mvprintw(9, COLS - 29, "+==========================+");
                            attroff(COLOR_PAIR(6) | A_BOLD);
                            refresh();

                            // Leer email
                            ui_read_input(12, 12, mail, MAX_CHAIN_SIZE, 0);
                            if(strlen(mail) == 0) break;

                            if(validar_email(mail) != 0) {
                                strcpy(mensaje_error, "El correo es invalido");

                                ui_clear_screen();
                                ui_print_colored(LINES/2 - 1, (COLS-strlen(mensaje_error))/2, mensaje_error, 3);

                                // Mensaje de reintento
                                mvprintw(LINES/2 + 1, (COLS-strlen("¿Intentar otra vez?"))/2, "¿Intentar otra vez?");

                                int centro_x = (COLS - 32) / 2; // Centrado para dos botones de 12 caracteres con separación de 20
                                int boton_y = LINES/2 + 3;
                                int boton_ancho = 12;
                                int boton_alto = 3;

                                int seleccion = 0; // 0 para Sí, 1 para No
                                int salir_seleccion = 0;

                                while(!salir_seleccion) {
                                    // Dibujar botones usando la función centralizada
                                    dibujar_botones_si_no(centro_x, boton_y, seleccion, boton_ancho, boton_alto);

                                    ui_print_footer("Use flechas para navegar y Enter para seleccionar");
                                    refresh();

                                    int tecla = getch();
                                    if(tecla == KEY_LEFT || tecla == KEY_RIGHT) {
                                        seleccion = 1 - seleccion; // Alternar entre 0 y 1
                                    } else if(tecla == 10) { // Enter
                                        if(seleccion == 0) { // Sí
                                            salir_seleccion = 1;
                                            reintentar = 1; // Volver a intentar
                                        } else { // No
                                            salir_seleccion = 1;
                                            reintentar = 0; // No volver a intentar
                                        }
                                    } else if(tecla == 27) { // ESC
                                        salir_seleccion = 1;
                                        reintentar = 0; // No volver a intentar
                                    }
                                }
                                if(reintentar) continue; // Reiniciar el proceso de creación
                                else break; // Salir del proceso
                            }
                        }

                        if(!error_encontrado) {
                            // Limpiar requisitos anteriores y mostrar requisitos de teléfono
                            for(int i = 1; i <= 9; i++) {
                                move(i, COLS - 29);
                                clrtoeol();
                            }

                            attron(COLOR_PAIR(6) | A_BOLD);
                            mvprintw(1, COLS - 29, "+==========================+");
                            mvprintw(2, COLS - 29, "|  REQUISITOS DE TELEFONO  |");
                            mvprintw(3, COLS - 29, "+==========================+");
                            attroff(COLOR_PAIR(6) | A_BOLD);
                            attron(COLOR_PAIR(6));
                            mvprintw(4, COLS - 29, "| > Minimo 8 digitos       |");
                            mvprintw(5, COLS - 29, "| > Solo numeros (0-9)     |");
                            mvprintw(6, COLS - 29, "|                          |");
                            mvprintw(7, COLS - 29, "| Ejemplo:                 |");
                            mvprintw(8, COLS - 29, "| 5512345678               |");
                            attroff(COLOR_PAIR(6));
                            attron(COLOR_PAIR(6) | A_BOLD);
                            mvprintw(9, COLS - 29, "+==========================+");
                            attroff(COLOR_PAIR(6) | A_BOLD);
                            refresh();

                            // Leer teléfono
                            ui_read_input(14, 15, telf, MAX_CHAIN_SIZE, 0);
                            if(strlen(telf) == 0) break;

                            if(validar_telefono(telf) != 0) {
                                strcpy(mensaje_error, "El telefono es invalido");

                                ui_clear_screen();
                                
                                ui_print_colored(LINES/2 - 1, (COLS-strlen(mensaje_error))/2, mensaje_error, 3);

                                // Mensaje de reintento
                                mvprintw(LINES/2 + 2, (COLS-strlen("¿Intentar otra vez?"))/2, "¿Intentar otra vez?");

                                int centro_x = (COLS - 32) / 2; // Centrado para dos botones de 12 caracteres con separación de 20
                                int boton_y = LINES/2 + 4;
                                int boton_ancho = 12;
                                int boton_alto = 3;

                                int seleccion = 0; // 0 para Sí, 1 para No
                                int salir_seleccion = 0;

                                while(!salir_seleccion) {
                                    // Dibujar botones usando la función centralizada
                                    dibujar_botones_si_no(centro_x, boton_y, seleccion, boton_ancho, boton_alto);

                                    ui_print_footer("Use flechas para navegar y Enter para seleccionar");
                                    refresh();

                                    int tecla = getch();
                                    if(tecla == KEY_LEFT || tecla == KEY_RIGHT) {
                                        seleccion = 1 - seleccion; // Alternar entre 0 y 1
                                    } else if(tecla == 10) { // Enter
                                        if(seleccion == 0) { // Sí
                                            salir_seleccion = 1;
                                            reintentar = 1; // Volver a intentar
                                        } else { // No
                                            salir_seleccion = 1;
                                            reintentar = 0; // No volver a intentar
                                        }
                                    } else if(tecla == 27) { // ESC
                                        salir_seleccion = 1;
                                        reintentar = 0; // No volver a intentar
                                    }
                                }
                                if(reintentar) continue; // Reiniciar el proceso de creación
                                else break; // Salir del proceso
                            }
                        }

                        if(!error_encontrado) {
                            // Limpiar requisitos anteriores y mostrar requisitos de tipo
                            for(int i = 1; i <= 9; i++) {
                                move(i, COLS - 29);
                                clrtoeol();
                            }

                            attron(COLOR_PAIR(6) | A_BOLD);
                            mvprintw(1, COLS - 29, "+==========================+");
                            mvprintw(2, COLS - 29, "|     TIPO DE USUARIO      |");
                            mvprintw(3, COLS - 29, "+==========================+");
                            attroff(COLOR_PAIR(6) | A_BOLD);
                            attron(COLOR_PAIR(6));
                            mvprintw(4, COLS - 29, "| > M: Mesero              |");
                            mvprintw(5, COLS - 29, "| > C: Cocina              |");
                            mvprintw(6, COLS - 29, "| > A: Administrador       |");
                            mvprintw(7, COLS - 29, "|                          |");
                            mvprintw(8, COLS - 29, "| Ingrese M, C o A    |");
                            attroff(COLOR_PAIR(6));
                            attron(COLOR_PAIR(6) | A_BOLD);
                            mvprintw(9, COLS - 29, "+==========================+");
                            attroff(COLOR_PAIR(6) | A_BOLD);
                            refresh();

                            // Leer tipo
                            char tipo_char;
                            move(16, 51);
                            refresh();
                            tipo_char = getch();
                            if(tipo_char == 'C' || tipo_char == 'c') tipo = 1;
                            else if(tipo_char == 'A' || tipo_char == 'a') tipo = 2;
                            else tipo = 0;

                            // Preparar petición
                            Peticion pet;
                            Respuesta resp;
                            memset(&pet, 0, sizeof(Peticion));
                            memset(&resp, 0, sizeof(Respuesta));

                            pet.operacion = OP_CREAR_USUARIO;
                            strncpy(pet.name, name, MAX_CHAIN_SIZE-1);
                            strncpy(pet.user, user, MAX_CHAIN_SIZE-1);
                            strncpy(pet.pass, pass, MAX_CHAIN_SIZE-1);
                            strncpy(pet.mail, mail, MAX_CHAIN_SIZE-1);
                            strncpy(pet.telf, telf, MAX_CHAIN_SIZE-1);
                            pet.tipo = tipo;

                            // Enviar al servidor
                            if (enviar_peticion(&pet, &resp) != 0) {
                                ui_show_error("Error de comunicacion con el servidor");
                                break; // Salir del proceso de creación
                            }

                            if (resp.codigo == RESP_OK) {
                                ui_show_success(resp.mensaje);
                                usuario_cargar(); // Recargar lista de usuarios para que se muestre el nuevo usuario
                            } else {
                                char error_mensaje[256]; // New variable to avoid redeclaration
                                strcpy(error_mensaje, resp.mensaje);

                                ui_clear_screen();
                                ui_print_colored(LINES/2 - 3, (COLS-strlen(error_mensaje))/2, error_mensaje, 3);

                                // Mensaje de reintento
                                mvprintw(LINES/2 + 2, (COLS-strlen("¿Intentar otra vez?"))/2, "¿Intentar otra vez?");

                                int centro_x = (COLS - 32) / 2; // Centrado para dos botones de 12 caracteres con separación de 20
                                int boton_y = LINES/2 + 4;
                                int boton_ancho = 12;
                                int boton_alto = 3;

                                int seleccion = 0; // 0 para Sí, 1 para No
                                int salir_seleccion = 0;

                                while(!salir_seleccion) {
                                    // Dibujar botones usando la función centralizada
                                    dibujar_botones_si_no(centro_x, boton_y, seleccion, boton_ancho, boton_alto);

                                    ui_print_footer("Use flechas para navegar y Enter para seleccionar");
                                    refresh();

                                    int tecla = getch();
                                    if(tecla == KEY_LEFT || tecla == KEY_RIGHT) {
                                        seleccion = 1 - seleccion; // Alternar entre 0 y 1
                                    } else if(tecla == 10) { // Enter
                                        if(seleccion == 0) { // Sí
                                            salir_seleccion = 1;
                                            reintentar = 1; // Volver a intentar
                                        } else { // No
                                            salir_seleccion = 1;
                                            reintentar = 0; // No volver a intentar
                                        }
                                    } else if(tecla == 27) { // ESC
                                        salir_seleccion = 1;
                                        reintentar = 0; // No volver a intentar
                                    }
                                }
                                if(reintentar) continue; // Reiniciar el proceso de creación
                                else break; // Salir del proceso
                            }
                        }
                    } // Fin del bucle while(reintentar)
                }

                if(seleccionado != 0) { // Si no es la tarjeta de nuevo usuario, entonces es un usuario existente
                    USUARIO *usr = usuario_get_by_index(seleccionado - 1); // -1 porque el 0 es nuevo usuario
                    if(usr != NULL) {
                        // Mostrar menú para editar usuario
                        int opcion_seleccionada = 0;
                        int num_opciones = 3; // Editar usuario, Eliminar usuario, Volver

                        while(1) {
                            clear();
                            attron(A_BOLD);
                            mvprintw(2, (COLS - 30) / 2, "OPCIONES USUARIO: %s", usr->name);
                            attroff(A_BOLD);

                            char *opciones[] = {
                                "Editar usuario",       // Cambia nombre, usuario, correo, teléfono, tipo
                                "Eliminar usuario",     // Eliminar usuario
                                "Volver"                // Volver al menú principal
                            };

                            for(int i = 0; i < num_opciones; i++) {
                                if(i == opcion_seleccionada) {
                                    attron(A_REVERSE);
                                    mvprintw(6 + i * 2, (COLS - strlen(opciones[i])) / 2, "%s", opciones[i]);
                                    attroff(A_REVERSE);
                                } else {
                                    mvprintw(6 + i * 2, (COLS - strlen(opciones[i])) / 2, "%s", opciones[i]);
                                }
                            }

                            mvprintw(LINES - 3, 1, "Use las flechas para navegar, Enter para seleccionar, Escape para volver");

                            refresh();

                            tecla = getch();

                            switch(tecla) {
                                case KEY_UP:
                                    opcion_seleccionada = (opcion_seleccionada - 1 + num_opciones) % num_opciones;
                                    break;
                                case KEY_DOWN:
                                    opcion_seleccionada = (opcion_seleccionada + 1) % num_opciones;
                                    break;
                                case 10: // Enter
                                    if(opcion_seleccionada == 0) { // Editar usuario
                                        char name[MAX_CHAIN_SIZE];
                                        char user[MAX_CHAIN_SIZE];
                                        char pass[32];
                                        char mail[MAX_CHAIN_SIZE];
                                        char telf[MAX_CHAIN_SIZE];
                                        char tipo_str[10]; // Para manejar el tipo como string
                                        int tipo;

                                        ui_clear_screen();
                                        ui_print_title("Editar Usuario");

                                        attron(COLOR_PAIR(10) | A_BOLD);
                                        mvprintw(6, 5, "Nombre completo:");
                                        mvprintw(8, 5, "Usuario:");
                                        mvprintw(10, 5, "Contrasena:");
                                        mvprintw(12, 5, "Email:");
                                        mvprintw(14, 5, "Telefono:");
                                        mvprintw(16, 5, "Tipo:");
                                        attroff(COLOR_PAIR(10) | A_BOLD);

                                        ui_print_footer("Presione ESC para cancelar");
                                        refresh();

                                        // Inicializar todos los campos con los valores actuales
                                        strcpy(name, usr->name);
                                        strcpy(user, usr->user);
                                        strcpy(mail, usr->mail);
                                        strcpy(telf, usr->telf);
                                        snprintf(tipo_str, sizeof(tipo_str), "%d", usr->tipo);
                                        strcpy(pass, ""); // Contraseña en blanco para mantener la actual

                                        // Mostrar valores actuales en color diferente
                                        attron(COLOR_PAIR(6)); // Diferente color para valores actuales
                                        mvprintw(6, 22, "%s", usr->name);
                                        mvprintw(8, 14, "%s", usr->user);
                                        mvprintw(10, 17, "%*s", (int)strlen(usr->pass), "*"); // Ocultar contraseña
                                        mvprintw(12, 12, "%s", usr->mail);
                                        mvprintw(14, 15, "%s", usr->telf);
                                        mvprintw(16, 11, "%d", usr->tipo); // Mostrar tipo actual
                                        attroff(COLOR_PAIR(6)); // Restaurar color normal
                                        refresh();

                                        int campo_actual = 0; // 0: nombre, 1: usuario, 2: contraseña, 3: correo, 4: teléfono, 5: tipo
                                        int num_campos = 6;
                                        // int campos_validos[6] = {1, 1, 1, 1, 1, 1}; // Todos los campos inicialmente válidos

                                        while(1) {
                                            // Mostrar campos con resaltado en el campo actual
                                            ui_clear_screen();
                                            ui_print_title("Editar Usuario");

                                            attron(COLOR_PAIR(10) | A_BOLD);
                                            if(campo_actual == 0) attron(A_REVERSE); // Resaltar campo actual
                                            mvprintw(6, 5, "Nombre completo:");
                                            mvprintw(6, 22, "%s", name);
                                            if(campo_actual == 0) attroff(A_REVERSE);

                                            if(campo_actual == 1) attron(A_REVERSE);
                                            mvprintw(8, 5, "Usuario:");
                                            mvprintw(8, 14, "%s", user);
                                            if(campo_actual == 1) attroff(A_REVERSE);

                                            if(campo_actual == 2) attron(A_REVERSE);
                                            mvprintw(10, 5, "Contrasena:");
                                            mvprintw(10, 17, "%*s", (int)strlen(pass), "*");
                                            if(campo_actual == 2) attroff(A_REVERSE);

                                            if(campo_actual == 3) attron(A_REVERSE);
                                            mvprintw(12, 5, "Email:");
                                            mvprintw(12, 12, "%s", mail);
                                            if(campo_actual == 3) attroff(A_REVERSE);

                                            if(campo_actual == 4) attron(A_REVERSE);
                                            mvprintw(14, 5, "Telefono:");
                                            mvprintw(14, 15, "%s", telf);
                                            if(campo_actual == 4) attroff(A_REVERSE);

                                            if(campo_actual == 5) attron(A_REVERSE);
                                            mvprintw(16, 5, "Tipo:");
                                            mvprintw(16, 11, "%s", tipo_str);
                                            if(campo_actual == 5) attroff(A_REVERSE);

                                            attroff(COLOR_PAIR(10) | A_BOLD);

                                            // Mostrar caja de requisitos según el campo actual
                                            for(int i = 1; i <= 9; i++) {
                                                move(i, COLS - 29);
                                                clrtoeol();
                                            }

                                            switch(campo_actual) {
                                                case 0: // Nombre
                                                    attron(COLOR_PAIR(6) | A_BOLD);
                                                    mvprintw(1, COLS - 29, "+==========================+");
                                                    mvprintw(2, COLS - 29, "|     REQUISITOS NOMBRE    |");
                                                    mvprintw(3, COLS - 29, "+==========================+");
                                                    attroff(COLOR_PAIR(6) | A_BOLD);
                                                    attron(COLOR_PAIR(6));
                                                    mvprintw(4, COLS - 29, "| > Nombre completo valido |");
                                                    mvprintw(5, COLS - 29, "| > No campos vacios       |");
                                                    mvprintw(6, COLS - 29, "|                          |");
                                                    mvprintw(7, COLS - 29, "| Ejemplo: Juan Perez      |");
                                                    mvprintw(8, COLS - 29, "|                          |");
                                                    attroff(COLOR_PAIR(6));
                                                    attron(COLOR_PAIR(6) | A_BOLD);
                                                    mvprintw(9, COLS - 29, "+==========================+");
                                                    attroff(COLOR_PAIR(6) | A_BOLD);
                                                    break;
                                                case 1: // Usuario
                                                    attron(COLOR_PAIR(6) | A_BOLD);
                                                    mvprintw(1, COLS - 29, "+==========================+");
                                                    mvprintw(2, COLS - 29, "|    REQUISITOS USUARIO    |");
                                                    mvprintw(3, COLS - 29, "+==========================+");
                                                    attroff(COLOR_PAIR(6) | A_BOLD);
                                                    attron(COLOR_PAIR(6));
                                                    mvprintw(4, COLS - 29, "| > Minimo 3 caracteres    |");
                                                    mvprintw(5, COLS - 29, "| > Solo letras y numeros  |");
                                                    mvprintw(6, COLS - 29, "| > No espacios            |");
                                                    mvprintw(7, COLS - 29, "|                          |");
                                                    mvprintw(8, COLS - 29, "| Ejemplo: juanperez       |");
                                                    attroff(COLOR_PAIR(6));
                                                    attron(COLOR_PAIR(6) | A_BOLD);
                                                    mvprintw(9, COLS - 29, "+==========================+");
                                                    attroff(COLOR_PAIR(6) | A_BOLD);
                                                    break;
                                                case 2: // Contraseña
                                                    attron(COLOR_PAIR(6) | A_BOLD);
                                                    mvprintw(1, COLS - 29, "+==========================+");
                                                    mvprintw(2, COLS - 29, "| REQUISITOS DE CONTRASENA |");
                                                    mvprintw(3, COLS - 29, "+==========================+");
                                                    attroff(COLOR_PAIR(6) | A_BOLD);
                                                    attron(COLOR_PAIR(6));
                                                    mvprintw(4, COLS - 29, "| > Minimo 8 caracteres    |");
                                                    mvprintw(5, COLS - 29, "| > 1 mayuscula (A-Z)      |");
                                                    mvprintw(6, COLS - 29, "| > 1 minuscula (a-z)      |");
                                                    mvprintw(7, COLS - 29, "| > 1 numero (0-9)         |");
                                                    mvprintw(8, COLS - 29, "| > 2 simbolos (*/_-+.)    |");
                                                    attroff(COLOR_PAIR(6));
                                                    attron(COLOR_PAIR(6) | A_BOLD);
                                                    mvprintw(9, COLS - 29, "+==========================+");
                                                    attroff(COLOR_PAIR(6) | A_BOLD);
                                                    break;
                                                case 3: // Correo
                                                    attron(COLOR_PAIR(6) | A_BOLD);
                                                    mvprintw(1, COLS - 29, "+==========================+");
                                                    mvprintw(2, COLS - 29, "|   REQUISITOS DE EMAIL    |");
                                                    mvprintw(3, COLS - 29, "+==========================+");
                                                    attroff(COLOR_PAIR(6) | A_BOLD);
                                                    attron(COLOR_PAIR(6));
                                                    mvprintw(4, COLS - 29, "| > 1 arroba (@)           |");
                                                    mvprintw(5, COLS - 29, "| > Al menos 1 punto (.)   |");
                                                    mvprintw(6, COLS - 29, "|                          |");
                                                    mvprintw(7, COLS - 29, "| Ejemplo:                 |");
                                                    mvprintw(8, COLS - 29, "| usuario@correo.com       |");
                                                    attroff(COLOR_PAIR(6));
                                                    attron(COLOR_PAIR(6) | A_BOLD);
                                                    mvprintw(9, COLS - 29, "+==========================+");
                                                    attroff(COLOR_PAIR(6) | A_BOLD);
                                                    break;
                                                case 4: // Teléfono
                                                    attron(COLOR_PAIR(6) | A_BOLD);
                                                    mvprintw(1, COLS - 29, "+==========================+");
                                                    mvprintw(2, COLS - 29, "|  REQUISITOS DE TELEFONO  |");
                                                    mvprintw(3, COLS - 29, "+==========================+");
                                                    attroff(COLOR_PAIR(6) | A_BOLD);
                                                    attron(COLOR_PAIR(6));
                                                    mvprintw(4, COLS - 29, "| > Minimo 8 digitos       |");
                                                    mvprintw(5, COLS - 29, "| > Solo numeros (0-9)     |");
                                                    mvprintw(6, COLS - 29, "|                          |");
                                                    mvprintw(7, COLS - 29, "| Ejemplo:                 |");
                                                    mvprintw(8, COLS - 29, "| 5512345678               |");
                                                    attroff(COLOR_PAIR(6));
                                                    attron(COLOR_PAIR(6) | A_BOLD);
                                                    mvprintw(9, COLS - 29, "+==========================+");
                                                    attroff(COLOR_PAIR(6) | A_BOLD);
                                                    break;
                                                case 5: // Tipo
                                                    attron(COLOR_PAIR(6) | A_BOLD);
                                                    mvprintw(1, COLS - 29, "+==========================+");
                                                    mvprintw(2, COLS - 29, "|     TIPO DE USUARIO      |");
                                                    mvprintw(3, COLS - 29, "+==========================+");
                                                    attroff(COLOR_PAIR(6) | A_BOLD);
                                                    attron(COLOR_PAIR(6));
                                                    mvprintw(4, COLS - 29, "| > 0: Mesero              |");
                                                    mvprintw(5, COLS - 29, "| > 1: Cocina              |");
                                                    mvprintw(6, COLS - 29, "| > 2: Administrador       |");
                                                    mvprintw(7, COLS - 29, "|                          |");
                                                    mvprintw(8, COLS - 29, "| Ingrese 0, 1 o 2         |");
                                                    attroff(COLOR_PAIR(6));
                                                    attron(COLOR_PAIR(6) | A_BOLD);
                                                    mvprintw(9, COLS - 29, "+==========================+");
                                                    attroff(COLOR_PAIR(6) | A_BOLD);
                                                    break;
                                            }

                                            ui_print_footer("Use flechas para navegar, ENTER para editar, ESC para salir");
                                            refresh();

                                            int tecla = getch();
                                            if(tecla == 27) { // ESC
                                                break; // Salir sin guardar
                                            } else if(tecla == KEY_UP) {
                                                campo_actual = (campo_actual - 1 + num_campos) % num_campos;
                                            } else if(tecla == KEY_DOWN) {
                                                campo_actual = (campo_actual + 1) % num_campos;
                                            } else if(tecla == 10) { // Enter
                                                // Editar el campo actual
                                                char temp_input[MAX_CHAIN_SIZE];
                                                int input_y, input_x;
                                                int is_password = 0;

                                                switch(campo_actual) {
                                                    case 0: // Nombre
                                                        input_y = 6; input_x = 22;
                                                        is_password = 0;
                                                        break;
                                                    case 1: // Usuario
                                                        input_y = 8; input_x = 14;
                                                        is_password = 0;
                                                        break;
                                                    case 2: // Contraseña
                                                        input_y = 10; input_x = 17;
                                                        is_password = 1;
                                                        break;
                                                    case 3: // Correo
                                                        input_y = 12; input_x = 12;
                                                        is_password = 0;
                                                        break;
                                                    case 4: // Teléfono
                                                        input_y = 14; input_x = 15;
                                                        is_password = 0;
                                                        break;
                                                    case 5: // Tipo
                                                        input_y = 16; input_x = 11;
                                                        is_password = 0;
                                                        break;
                                                }

                                                // Limpiar el campo antes de leer entrada
                                                mvchgat(input_y, input_x, COLS - input_x, A_NORMAL, 0, NULL);
                                                clrtoeol(); // Limpiar hasta el final de la línea

                                                // Leer entrada
                                                ui_read_input(input_y, input_x, temp_input, MAX_CHAIN_SIZE, is_password);

                                                // Procesar la entrada según el campo
                                                switch(campo_actual) {
                                                    case 0: // Nombre
                                                        if(strlen(temp_input) > 0) {
                                                            strcpy(name, temp_input);
                                                        } else {
                                                            strcpy(name, usr->name); // Mantener valor original
                                                        }
                                                        break;
                                                    case 1: // Usuario
                                                        if(strlen(temp_input) > 0) {
                                                            strcpy(user, temp_input);
                                                        } else {
                                                            strcpy(user, usr->user); // Mantener valor original
                                                        }
                                                        break;
                                                    case 2: // Contraseña
                                                        if(strlen(temp_input) > 0) {
                                                            strcpy(pass, temp_input);
                                                            if(validar_password(temp_input) != 0) {
                                                                ui_clear_screen();
                                                                ui_print_colored(LINES/2 - 3, (COLS-strlen("La contrasena es invalida"))/2, "La contrasena es invalida", 3);

                                                                // Mensaje de reintento
                                                                mvprintw(LINES/2 + 1, (COLS-strlen("Presione cualquier tecla para continuar..."))/2, "Presione cualquier tecla para continuar...");
                                                                refresh();
                                                                ui_wait_key();
                                                            }
                                                        } else {
                                                            strcpy(pass, ""); // Contraseña vacía significa mantener la actual
                                                        }
                                                        break;
                                                    case 3: // Correo
                                                        if(strlen(temp_input) > 0) {
                                                            if(validar_email(temp_input) == 0) {
                                                                strcpy(mail, temp_input);
                                                            } else {
                                                                ui_clear_screen();
                                                                ui_print_colored(LINES/2 - 1, (COLS-strlen("El correo es invalido"))/2, "El correo es invalido", 3);

                                                                // Mensaje de reintento
                                                                mvprintw(LINES/2 + 1, (COLS-strlen("Presione cualquier tecla para continuar..."))/2, "Presione cualquier tecla para continuar...");
                                                                refresh();
                                                                ui_wait_key();
                                                                strcpy(mail, usr->mail); // Restaurar valor original
                                                            }
                                                        } else {
                                                            strcpy(mail, usr->mail); // Mantener valor original
                                                        }
                                                        break;
                                                    case 4: // Teléfono
                                                        if(strlen(temp_input) > 0) {
                                                            if(validar_telefono(temp_input) == 0) {
                                                                strcpy(telf, temp_input);
                                                            } else {
                                                                ui_clear_screen();
                                                                ui_print_colored(LINES/2 - 1, (COLS-strlen("El telefono es invalido"))/2, "El telefono es invalido", 3);

                                                                // Mensaje de reintento
                                                                mvprintw(LINES/2 + 2, (COLS-strlen("Presione cualquier tecla para continuar..."))/2, "Presione cualquier tecla para continuar...");
                                                                refresh();
                                                                ui_wait_key();
                                                                strcpy(telf, usr->telf); // Restaurar valor original
                                                            }
                                                        } else {
                                                            strcpy(telf, usr->telf); // Mantener valor original
                                                        }
                                                        break;
                                                    case 5: // Tipo
                                                        if(strlen(temp_input) > 0) {
                                                            int temp_tipo = atoi(temp_input);
                                                            if(temp_tipo >= 0 && temp_tipo <= 2) {
                                                                snprintf(tipo_str, sizeof(tipo_str), "%d", temp_tipo);
                                                            } else {
                                                                ui_clear_screen();
                                                                ui_print_colored(LINES/2 - 1, (COLS-strlen("Tipo invalido"))/2, "Tipo invalido", 3);

                                                                // Mensaje de reintento
                                                                mvprintw(LINES/2 + 1, (COLS-strlen("Presione cualquier tecla para continuar..."))/2, "Presione cualquier tecla para continuar...");
                                                                refresh();
                                                                ui_wait_key();
                                                                snprintf(tipo_str, sizeof(tipo_str), "%d", usr->tipo); // Restaurar valor original
                                                            }
                                                        } else {
                                                            snprintf(tipo_str, sizeof(tipo_str), "%d", usr->tipo); // Mantener valor original
                                                        }
                                                        break;
                                                }
                                            }
                                        }

                                        // Si salió sin guardar (por ejemplo, con ESC), regresar sin cambiar nada
                                        if(tecla == 27) {
                                            goto salir_submenu_usuario; // Regresar a la lista de usuarios sin guardar cambios
                                        }

                                        // Convertir el string de tipo de nuevo a int
                                        tipo = atoi(tipo_str);
                                        if(tipo < 0 || tipo > 2) tipo = usr->tipo; // Si es inválido, mantener el valor actual

                                        // Actualizar datos del usuario finalmente
                                        strncpy(usr->name, name, MAX_CHAIN_SIZE-1);
                                        strncpy(usr->user, user, MAX_CHAIN_SIZE-1);
                                        if(strlen(pass) > 0) { // Solo actualizar contraseña si se introdujo una nueva
                                            strncpy(usr->pass, pass, 31);
                                        }
                                        strncpy(usr->mail, mail, MAX_CHAIN_SIZE-1);
                                        strncpy(usr->telf, telf, MAX_CHAIN_SIZE-1);
                                        usr->tipo = tipo;

                                        usuario_guardar(); // Guardar en archivo
                                        usuario_cargar(); // Recargar lista de usuarios para reflejar el cambio

                                        ui_show_success("Usuario actualizado correctamente.");
                                        goto salir_submenu_usuario; // Regresar a la lista de usuarios
                                    }
                                    else if(opcion_seleccionada == 1) { // Eliminar usuario
                                        // Confirmar eliminación con interfaz de selección
                                        int seleccion_confirmacion = 0;
                                        int tecla_confirmacion;
                                        int num_opciones_conf = 2; // Sí, No
                                        // char *opciones_conf[] = {
                                        //     "CONFIRMAR",
                                        //     "CANCELAR"
                                        // };

                                        while(1) {
                                            clear();
                                            attron(A_BOLD);
                                            mvprintw(2, (COLS - 40) / 2, "ESTA SEGURO DE ELIMINAR EL USUARIO?");
                                            mvprintw(3, (COLS - strlen(usr->name)) / 2, "%s", usr->name);
                                            attroff(A_BOLD);

                                            // Dibujar botones usando la función centralizada
                                            int centro_x = (COLS - 40) / 2; // Centrado para dos botones
                                            int boton_y = 8;
                                            int boton_ancho = 15;
                                            int boton_alto = 4;

                                            dibujar_botones_confirmacion(centro_x, boton_y, seleccion_confirmacion, boton_ancho, boton_alto, "CONFIRMAR", "CANCELAR");

                                            mvprintw(LINES - 3, 1, "Use flechas para navegar, Enter para seleccionar, ESC para cancelar");
                                            refresh();

                                            tecla_confirmacion = getch();

                                            switch(tecla_confirmacion) {
                                                case KEY_LEFT:
                                                    seleccion_confirmacion = (seleccion_confirmacion - 1 + num_opciones_conf) % num_opciones_conf;
                                                    break;
                                                case KEY_RIGHT:
                                                    seleccion_confirmacion = (seleccion_confirmacion + 1) % num_opciones_conf;
                                                    break;
                                                case 10: // Enter
                                                    if(seleccion_confirmacion == 0) { // Confirmar
                                                        // Eliminar el usuario
                                                        if(usuario_eliminar_por_nombre(usr->user) == 0) {
                                                            usuario_guardar(); // Guardar en archivo
                                                            usuario_cargar(); // Recargar lista de usuarios para reflejar el cambio
                                                            ui_show_success("Usuario eliminado correctamente.");
                                                            goto salir_submenu_usuario; // Salir completamente y regresar a la lista de usuarios
                                                        } else {
                                                            ui_show_error("Error al eliminar usuario.");
                                                        }
                                                        goto salir_confirmacion_usuario; // Salir del bucle de confirmación
                                                    } else { // Cancelar
                                                        ui_show_info("Eliminación cancelada.");
                                                        goto salir_confirmacion_usuario; // Salir del bucle de confirmación y regresar a la lista de usuarios
                                                    }
                                                    break;
                                                case 27: // Escape
                                                    ui_show_info("Eliminación cancelada.");
                                                    goto salir_confirmacion_usuario; // Salir del bucle de confirmación y regresar a la lista de usuarios
                                                    break;
                                            }
                                        }
                                        salir_confirmacion_usuario:;
                                    }
                                    else if(opcion_seleccionada == 2) { // Volver
                                        goto salir_submenu_usuario; // Regresar a la lista de usuarios
                                    }
                                    break;
                                case 27: // Escape
                                    goto salir_submenu_usuario; // Regresar a la lista de usuarios
                            }
                        }
                    salir_submenu_usuario:;
                    }
                }
                break;
            case 27: // Escape
                return; // Salir del menú de usuarios
        }
    }
}

// Función principal de la interfaz de administrador
void interfaz_admin_ejecutar(USUARIO *usuario_logueado) {
    // Saltar el mensaje de bienvenida y entrar directamente al menú
    // Mostrar menú principal de administrador
    mostrar_menu_principal();
}
