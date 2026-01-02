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

void down(int semid);
void up(int semid);

// Funciones de la interfaz de administrador

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
        mvprintw(y+1, x+2, "NUEVO USUARIO");
        mvprintw(y+1, x+20, "Presione Enter para agregar");
        attroff(COLOR_PAIR(10) | A_BOLD);

        mvprintw(y+2, x+2, "Nombre: ---");
        mvprintw(y+3, x+2, "Usuario: ---");
        mvprintw(y+4, x+2, "Correo: ---");
        mvprintw(y+5, x+2, "Telefono: ---");
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
                                        int seleccion_ing = 0;
                                        int tecla_ing;
                                        int num_opciones_ing = 2; // Agregar ingrediente, Volver
                                        char *opciones_ing[] = {
                                            "Agregar ingrediente",
                                            "Volver"
                                        };

                                        while(1) {
                                            clear();
                                            attron(A_BOLD);
                                            mvprintw(2, (COLS - 30) / 2, "INGREDIENTES DE: %s", prod->nombre);
                                            attroff(A_BOLD);

                                            // Mostrar ingredientes actuales
                                            int y = 5;
                                            for(int i = 0; i < prod->num_ingredientes && y < LINES - 6; i++) {
                                                IngredienteProducto *ing = &prod->ingredientes[i];
                                                mvprintw(y, 5, "%d. Ingrediente ID: %d (cantidad: %d)", i+1, ing->id_ingrediente, ing->cantidad_necesaria);
                                                y++;
                                            }

                                            // Mostrar opciones
                                            for(int i = 0; i < num_opciones_ing; i++) {
                                                if(i == seleccion_ing) {
                                                    attron(A_REVERSE);
                                                    mvprintw(y + i * 2, (COLS - strlen(opciones_ing[i])) / 2, "%s", opciones_ing[i]);
                                                    attroff(A_REVERSE);
                                                } else {
                                                    mvprintw(y + i * 2, (COLS - strlen(opciones_ing[i])) / 2, "%s", opciones_ing[i]);
                                                }
                                            }

                                            mvprintw(LINES - 3, 1, "Use flechas para navegar, Enter para seleccionar, Escape para volver");

                                            refresh();

                                            tecla_ing = getch();

                                            switch(tecla_ing) {
                                                case KEY_UP:
                                                    seleccion_ing = (seleccion_ing - 1 + num_opciones_ing) % num_opciones_ing;
                                                    break;
                                                case KEY_DOWN:
                                                    seleccion_ing = (seleccion_ing + 1) % num_opciones_ing;
                                                    break;
                                                case 10: // Enter
                                                    if(seleccion_ing == 0) { // Agregar ingrediente
                                                        // Mostrar menú para seleccionar ingrediente del almacén
                                                        int seleccion_almacen = 0;
                                                        int tecla_almacen;
                                                        int num_ingredientes = inventario_get_total();
                                                        Ingrediente ingredientes_almacen[100];

                                                        // Cargar ingredientes del almacén
                                                        for(int i = 0; i < num_ingredientes; i++) {
                                                            Ingrediente *temp = inventario_get_by_index(i);
                                                            if(temp != NULL) {
                                                                ingredientes_almacen[i] = *temp;
                                                            }
                                                        }

                                                        while(1) {
                                                            clear();
                                                            attron(A_BOLD);
                                                            mvprintw(2, (COLS - 30) / 2, "SELECCIONAR INGREDIENTE");
                                                            attroff(A_BOLD);

                                                            // Mostrar ingredientes en el almacén
                                                            for(int i = 0; i < num_ingredientes && i < 10; i++) { // Mostrar máximo 10
                                                                if(i == seleccion_almacen) {
                                                                    attron(A_REVERSE);
                                                                    mvprintw(6 + i * 2, 5, "%s (disp: %d)", ingredientes_almacen[i].nombre, ingredientes_almacen[i].cantidad);
                                                                    attroff(A_REVERSE);
                                                                } else {
                                                                    mvprintw(6 + i * 2, 5, "%s (disp: %d)", ingredientes_almacen[i].nombre, ingredientes_almacen[i].cantidad);
                                                                }
                                                            }

                                                            mvprintw(LINES - 3, 1, "Use flechas para navegar, Enter para seleccionar, Escape para volver");

                                                            refresh();

                                                            tecla_almacen = getch();

                                                            switch(tecla_almacen) {
                                                                case KEY_UP:
                                                                    if(seleccion_almacen > 0) seleccion_almacen--;
                                                                    break;
                                                                case KEY_DOWN:
                                                                    if(seleccion_almacen < num_ingredientes - 1) seleccion_almacen++;
                                                                    break;
                                                                case 10: // Enter
                                                                    // Preguntar por la cantidad necesaria
                                                                    char cantidad_str[10];
                                                                    ui_clear_screen();
                                                                    ui_print_title("Cantidad Necesaria");

                                                                    attron(COLOR_PAIR(10) | A_BOLD);
                                                                    mvprintw(6, 5, "Ingrediente: %s", ingredientes_almacen[seleccion_almacen].nombre);
                                                                    mvprintw(8, 5, "Cantidad necesaria:");
                                                                    attroff(COLOR_PAIR(10) | A_BOLD);

                                                                    ui_read_input(8, 24, cantidad_str, sizeof(cantidad_str), 0);
                                                                    int cantidad = atoi(cantidad_str);

                                                                    // Verificar que no esté ya en la lista de ingredientes del producto
                                                                    int ya_existe = 0;
                                                                    for(int i = 0; i < prod->num_ingredientes; i++) {
                                                                        if(prod->ingredientes[i].id_ingrediente == ingredientes_almacen[seleccion_almacen].id) {
                                                                            ya_existe = 1;
                                                                            // Actualizar cantidad
                                                                            prod->ingredientes[i].cantidad_necesaria = cantidad;
                                                                            break;
                                                                        }
                                                                    }

                                                                    if(!ya_existe && prod->num_ingredientes < MAX_INGREDIENTES_PRODUCTO) {
                                                                        // Agregar nuevo ingrediente
                                                                        prod->ingredientes[prod->num_ingredientes].id = prod->num_ingredientes + 1; // ID del ingrediente en la relación
                                                                        prod->ingredientes[prod->num_ingredientes].id_ingrediente = ingredientes_almacen[seleccion_almacen].id;
                                                                        prod->ingredientes[prod->num_ingredientes].cantidad_necesaria = cantidad;
                                                                        prod->num_ingredientes++;
                                                                    }

                                                                    productos_guardar(); // Guardar en archivo
                                                                    ui_show_success("Ingrediente actualizado en el producto");
                                                                    break;
                                                                case 27: // Escape
                                                                    goto salir_seleccion_ingrediente;
                                                            }
                                                        }
                                                        salir_seleccion_ingrediente:;
                                                    } else {
                                                        goto salir_submenu_producto; // Regresar a la edición del producto
                                                    }
                                                    break;
                                                case 27: // Escape
                                                    goto salir_submenu_producto; // Regresar a la edición del producto
                                                    break;
                                            }
                                        }
                                    salir_submenu_producto:;
                                    }
                                    else if(opcion_seleccionada == 2) { // Eliminar producto
                                        // Confirmar eliminación con interfaz de selección
                                        int seleccion_confirmacion = 0;
                                        int tecla_confirmacion;
                                        int num_opciones_conf = 2; // Sí, No
                                        char *opciones_conf[] = {
                                            "CONFIRMAR",
                                            "CANCELAR"
                                        };

                                        while(1) {
                                            clear();
                                            attron(A_BOLD);
                                            mvprintw(2, (COLS - 40) / 2, "ESTA SEGURO DE ELIMINAR EL PRODUCTO?");
                                            mvprintw(3, (COLS - strlen(prod->nombre)) / 2, "%s", prod->nombre);
                                            attroff(A_BOLD);

                                            // Dibujar opciones como casillas
                                            int ancho_opcion = 20;
                                            int alto_opcion = 4;
                                            int centro_x = (COLS - (ancho_opcion * 2 + 5)) / 2;

                                            for(int i = 0; i < num_opciones_conf; i++) {
                                                int x = centro_x + i * (ancho_opcion + 5);
                                                int y = 8;

                                                // Dibujar borde de la casilla
                                                int color = (i == seleccion_confirmacion) ? 5 : 10;
                                                attron(COLOR_PAIR(color) | (i == seleccion_confirmacion ? A_BOLD : 0));

                                                // Borde superior
                                                mvaddch(y, x, '+');
                                                for(int k = 1; k < ancho_opcion-1; k++) mvaddch(y, x+k, '=');
                                                mvaddch(y, x+ancho_opcion-1, '+');

                                                // Lados
                                                for(int j = 1; j < alto_opcion-1; j++) {
                                                    mvaddch(y+j, x, '|');
                                                    mvaddch(y+j, x+ancho_opcion-1, '|');
                                                }

                                                // Borde inferior
                                                mvaddch(y+alto_opcion-1, x, '+');
                                                for(int k = 1; k < ancho_opcion-1; k++) mvaddch(y+alto_opcion-1, x+k, '=');
                                                mvaddch(y+alto_opcion-1, x+ancho_opcion-1, '+');

                                                attroff(COLOR_PAIR(color) | (i == seleccion_confirmacion ? A_BOLD : 0));

                                                // Texto en la casilla
                                                attron(COLOR_PAIR(10) | A_BOLD);
                                                mvprintw(y+2, x + (ancho_opcion - strlen(opciones_conf[i])) / 2, "%s", opciones_conf[i]);
                                                attroff(COLOR_PAIR(10) | A_BOLD);
                                            }

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
                                                        } else {
                                                            ui_show_error("Error al eliminar producto.");
                                                        }
                                                        goto salir_confirmacion_producto; // Salir del bucle de confirmación
                                                    } else { // Cancelar
                                                        ui_show_info("Eliminación cancelada.");
                                                        goto salir_confirmacion_producto; // Salir del bucle de confirmación
                                                    }
                                                    break;
                                                case 27: // Escape
                                                    ui_show_info("Eliminación cancelada.");
                                                    goto salir_confirmacion_producto; // Salir del bucle de confirmación
                                            }
                                        }
                                        salir_confirmacion_producto:;
                                    }
                                    else if(opcion_seleccionada == 3) { // Volver
                                        // No hacer nada, continuar con el bucle externo
                                    }
                                    break;
                                case 27: // Escape
                                    goto salir_submenu_producto; // Regresar a la lista de productos
                            }
                        }
                    }
                }
                break;
            case 27: // Escape
                return; // Salir del menú de productos
        }
    }
}

void mostrar_menu_ingredientes() {
    inventario_cargar(); // Cargar inventario

    int seleccionado = 0;
    int tecla;

    while(1) {
        // Actualizar el número de ingredientes en cada iteración del bucle
        int num_ingredientes = inventario_get_total();
        ui_clear_screen();
        ui_print_title("ADMINISTRAR INGREDIENTES");

        // Calcular layout
        int columnas = 3;
        int ancho_rec = (COLS - 10) / columnas;
        int alto_rec = 8;

        int margen_x = 2;
        int margen_y = 6;
        int espacio_x = 2;
        int espacio_y = 1;

        // Calcular desplazamiento para mostrar ingredientes
        int filas_visibles = (LINES - 12) / (alto_rec + espacio_y);
        if(filas_visibles < 1) filas_visibles = 1;
        int ingredientes_por_pantalla = filas_visibles * columnas;

        // Calcular rango visible
        int inicio = (seleccionado / ingredientes_por_pantalla) * ingredientes_por_pantalla;
        int fin = inicio + ingredientes_por_pantalla;
        if(fin > num_ingredientes + 1) fin = num_ingredientes + 1; // +1 para la tarjeta de nuevo ingrediente

        // Mostrar ingredientes en forma de rejilla
        for(int i = inicio; i < fin; i++) {
            int pos_relativa = i - inicio;
            int fila = pos_relativa / columnas;
            int columna = pos_relativa % columnas;
            int x = margen_x + columna * (ancho_rec + espacio_x);
            int y = margen_y + fila * (alto_rec + espacio_y);

            if(i == num_ingredientes) { // Última posición: "Agregar nuevo ingrediente"
                dibujar_recuadro_ingrediente_admin(x, y, ancho_rec, alto_rec, NULL, i == seleccionado);
            } else {
                Ingrediente *ing = inventario_get_by_index(i);
                if(ing != NULL) {
                    dibujar_recuadro_ingrediente_admin(x, y, ancho_rec, alto_rec, ing, i == seleccionado);
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
                if(seleccionado + columnas < num_ingredientes + 1)
                    seleccionado += columnas;
                break;
            case KEY_LEFT:
                if(seleccionado > 0)
                    seleccionado--;
                break;
            case KEY_RIGHT:
                if(seleccionado < num_ingredientes)
                    seleccionado++;
                break;
            case KEY_PPAGE: // Page Up
                seleccionado -= ingredientes_por_pantalla;
                if(seleccionado < 0) seleccionado = 0;
                break;
            case KEY_NPAGE: // Page Down
                seleccionado += ingredientes_por_pantalla;
                if(seleccionado >= num_ingredientes + 1) seleccionado = num_ingredientes;
                break;
            case 10: // Enter
                if(seleccionado == num_ingredientes) {
                    // Agregar nuevo ingrediente
                    char nombre[MAX_NOMBRE_INGREDIENTE];
                    char cantidad_str[20];
                    int cantidad;

                    ui_clear_screen();
                    ui_print_title("Agregar Nuevo Ingrediente");

                    attron(COLOR_PAIR(10) | A_BOLD);
                    mvprintw(6, 5, "Nombre:");
                    mvprintw(8, 5, "Cantidad:");
                    attroff(COLOR_PAIR(10) | A_BOLD);

                    ui_read_input(6, 14, nombre, MAX_NOMBRE_INGREDIENTE, 0);
                    if(strlen(nombre) == 0) break;

                    ui_read_input(8, 15, cantidad_str, sizeof(cantidad_str), 0);
                    cantidad = atoi(cantidad_str);

                    // Agregar ingrediente
                    if(inventario_agregar(nombre, cantidad) == 0) {
                        inventario_guardar(); // Guardar en archivo
                        ui_show_success("Ingrediente agregado correctamente.");
                        // Continuar al siguiente ciclo del bucle para recargar la lista
                    } else {
                        ui_show_error("Error al agregar ingrediente.");
                    }
                } else {
                    // Editar ingrediente existente
                    Ingrediente *ing = inventario_get_by_index(seleccionado);
                    if(ing != NULL) {
                        // Mostrar menú para editar ingrediente
                        int opcion_seleccionada = 0;
                        int num_opciones = 3; // Editar ingrediente, Eliminar ingrediente, Volver

                        while(1) {
                            clear();
                            attron(A_BOLD);
                            mvprintw(2, (COLS - 30) / 2, "OPCIONES INGREDIENTE: %s", ing->nombre);
                            attroff(A_BOLD);

                            char *opciones[] = {
                                "Editar ingrediente",   // Cambia nombre y cantidad
                                "Eliminar ingrediente", // Eliminar ingrediente
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
                                    if(opcion_seleccionada == 0) { // Editar ingrediente
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

                                        // Actualizar datos del ingrediente
                                        // Eliminar y reagregar con nuevos valores
                                        if(inventario_agregar(nombre, cantidad) == 0) {
                                            inventario_guardar(); // Guardar en archivo
                                            ui_show_success("Ingrediente actualizado correctamente.");
                                        } else {
                                            ui_show_error("Error al actualizar ingrediente.");
                                        }
                                        goto salir_submenu_ingrediente; // Regresar a la lista de ingredientes
                                    }
                                    else if(opcion_seleccionada == 1) { // Eliminar ingrediente
                                        // Confirmar eliminación con interfaz de selección
                                        int seleccion_confirmacion = 0;
                                        int tecla_confirmacion;
                                        int num_opciones_conf = 2; // Sí, No
                                        char *opciones_conf[] = {
                                            "CONFIRMAR",
                                            "CANCELAR"
                                        };

                                        while(1) {
                                            clear();
                                            attron(A_BOLD);
                                            mvprintw(2, (COLS - 40) / 2, "ESTA SEGURO DE ELIMINAR EL INGREDIENTE?");
                                            mvprintw(3, (COLS - strlen(ing->nombre)) / 2, "%s", ing->nombre);
                                            attroff(A_BOLD);

                                            // Dibujar opciones como casillas
                                            int ancho_opcion = 20;
                                            int alto_opcion = 4;
                                            int centro_x = (COLS - (ancho_opcion * 2 + 5)) / 2;

                                            for(int i = 0; i < num_opciones_conf; i++) {
                                                int x = centro_x + i * (ancho_opcion + 5);
                                                int y = 8;

                                                // Dibujar borde de la casilla
                                                int color = (i == seleccion_confirmacion) ? 5 : 10;
                                                attron(COLOR_PAIR(color) | (i == seleccion_confirmacion ? A_BOLD : 0));

                                                // Borde superior
                                                mvaddch(y, x, '+');
                                                for(int k = 1; k < ancho_opcion-1; k++) mvaddch(y, x+k, '=');
                                                mvaddch(y, x+ancho_opcion-1, '+');

                                                // Lados
                                                for(int j = 1; j < alto_opcion-1; j++) {
                                                    mvaddch(y+j, x, '|');
                                                    mvaddch(y+j, x+ancho_opcion-1, '|');
                                                }

                                                // Borde inferior
                                                mvaddch(y+alto_opcion-1, x, '+');
                                                for(int k = 1; k < ancho_opcion-1; k++) mvaddch(y+alto_opcion-1, x+k, '=');
                                                mvaddch(y+alto_opcion-1, x+ancho_opcion-1, '+');

                                                attroff(COLOR_PAIR(color) | (i == seleccion_confirmacion ? A_BOLD : 0));

                                                // Texto en la casilla
                                                attron(COLOR_PAIR(10) | A_BOLD);
                                                mvprintw(y+2, x + (ancho_opcion - strlen(opciones_conf[i])) / 2, "%s", opciones_conf[i]);
                                                attroff(COLOR_PAIR(10) | A_BOLD);
                                            }

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
                                                        // Eliminar el ingrediente
                                                        // No hay función directa para eliminar ingredientes, pero podemos marcarlo con cantidad 0
                                                        if(inventario_agregar(ing->nombre, 0) == 0) {
                                                            inventario_guardar(); // Guardar en archivo
                                                            ui_show_success("Ingrediente eliminado correctamente.");
                                                        } else {
                                                            ui_show_error("Error al eliminar ingrediente.");
                                                        }
                                                        goto salir_confirmacion_ingrediente; // Salir del bucle de confirmación
                                                    } else { // Cancelar
                                                        ui_show_info("Eliminación cancelada.");
                                                        goto salir_confirmacion_ingrediente; // Salir del bucle de confirmación
                                                    }
                                                    break;
                                                case 27: // Escape
                                                    ui_show_info("Eliminación cancelada.");
                                                    goto salir_confirmacion_ingrediente; // Salir del bucle de confirmación
                                            }
                                        }
                                        salir_confirmacion_ingrediente:;
                                    }
                                    else if(opcion_seleccionada == 2) { // Volver
                                        // No hacer nada, continuar con el bucle externo
                                    }
                                    break;
                                case 27: // Escape
                                    goto salir_submenu_ingrediente; // Regresar a la lista de ingredientes
                            }
                        }
                        salir_submenu_ingrediente:;
                    }
                }
                break;
            case 27: // Escape
                return; // Salir del menú de ingredientes
        }
    }
}

void mostrar_menu_logs() {
    // Mostrar logs del sistema
    ui_clear_screen();
    ui_print_title("REGISTRO DE ACTIVIDADES");

    // Cargar y mostrar logs
    FILE *archivo = fopen("log.txt", "r");
    if(archivo != NULL) {
        char linea[512];
        int linea_y = 6;

        while(fgets(linea, sizeof(linea), archivo) && linea_y < LINES - 2) {
            // Eliminar salto de línea
            linea[strcspn(linea, "\n")] = 0;
            mvprintw(linea_y++, 2, "%s", linea);
        }
        fclose(archivo);
    } else {
        mvprintw(6, 2, "No se pudo abrir el archivo de logs.");
    }

    mvprintw(LINES - 3, 2, "Presione cualquier tecla para continuar...");
    refresh();
    ui_wait_key();
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
void mostrar_menu_usuarios() {
    // Esta función debe ser implementada si se necesita
    // o se puede eliminar si se va a usar la lógica del archivo original
    ui_clear_screen();
    ui_print_title("ADMINISTRAR USUARIOS");

    mvprintw(6, 2, "Funcionalidad de administración de usuarios no implementada completamente.");
    mvprintw(7, 2, "Presione cualquier tecla para continuar...");
    refresh();
    ui_wait_key();
}

// Función principal de la interfaz de administrador
void interfaz_admin_ejecutar(USUARIO *usuario_logueado) {
    ui_clear_screen();
    ui_print_title("INTERFAZ DE ADMINISTRADOR");

    mvprintw(6, 2, "Bienvenido %s (Administrador)", usuario_logueado->name);
    mvprintw(8, 2, "Presione cualquier tecla para acceder al menu de administracion...");
    refresh();
    ui_wait_key();

    // Mostrar menú principal de administrador
    mostrar_menu_principal();
}