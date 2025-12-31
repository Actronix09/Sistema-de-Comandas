#include "interfaz_mesero.h"
#include "ui.h"
#include "protocolo.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include "ticket.h"
#include <math.h>

#define PERMISOS 0644
#define MAX_CLIENTES 10

// Estructura de datos compartidos (debe coincidir con ClientePrincipal.c)
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

// Variables externas del cliente
extern DatosCompartidos *datos_servidor;
extern int semid_servidor;
extern int mi_slot;

// Funciones de semáforo
void down(int semid);
void up(int semid);

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
    
    // Dimensiones mínimas y máximas para los recuadros
    int ancho_min = 30;
    int ancho_max = 50;
    int alto_fijo = 8;
    
    // Calcular cuántas columnas caben
    int max_cols = espacio_util_x / (ancho_min + 2);
    if(max_cols < 1) max_cols = 1;
    if(max_cols > 4) max_cols = 4;
    
    // Calcular ancho óptimo para las columnas
    int ancho_calculado = (espacio_util_x - (max_cols - 1) * 2) / max_cols;
    if(ancho_calculado > ancho_max) {
        ancho_calculado = ancho_max;
        // Recalcular columnas con ancho máximo
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

// Dibujar recuadro de producto con texto ajustado
void dibujar_recuadro_producto(int x, int y, int ancho, int alto, ProductoMsg *prod, int seleccionado) {
    int color = seleccionado ? 5 : 10;
    
    attron(COLOR_PAIR(color) | (seleccionado ? A_BOLD : 0));
    
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
    
    attroff(COLOR_PAIR(color) | (seleccionado ? A_BOLD : 0));
    
    // Contenido del producto
    if(prod) {
        int ancho_texto = ancho - 4;
        
        attron(COLOR_PAIR(color) | A_BOLD);
        mvprintw(y+1, x+2, "ID: %d", prod->id);
        attroff(COLOR_PAIR(color) | A_BOLD);
        
        attron(COLOR_PAIR(color));
        
        // Nombre (ajustar a ancho disponible)
        char nombre_ajustado[100];
        strncpy(nombre_ajustado, prod->nombre, ancho_texto);
        nombre_ajustado[ancho_texto] = '\0';
        mvprintw(y+2, x+2, "%s", nombre_ajustado);
        
        // Descripción (dividir en líneas según ancho)
        char desc[200];
        strncpy(desc, prod->descripcion, 199);
        desc[199] = '\0';
        
        int linea_actual = y+3;
        int max_lineas = 3; // Máximo 3 líneas para descripción
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
        attron(A_BOLD);
        mvprintw(y+alto-2, x+2, "$ %.2f", prod->precio);
        attroff(A_BOLD);
        
        attroff(COLOR_PAIR(color));
    }
}

// Función para mostrar detalles del ticket
void mostrar_detalle_ticket(TicketMsg *ticket) {
    ui_clear_screen();
    ui_print_title("DETALLE DEL TICKET");
    
    attron(COLOR_PAIR(1) | A_BOLD);
    ui_print_centered(5, NOMBRE_RESTAURANTE);
    attroff(COLOR_PAIR(1) | A_BOLD);
    
    attron(COLOR_PAIR(10));
    mvprintw(7, 5, "Ticket #%d", ticket->id);
    mvprintw(8, 5, "Pedido: #%d | Mesa: %s | Mesero: %s", 
            ticket->pedido_id, ticket->mesa, ticket->mesero);
    mvprintw(9, 5, "Fecha: %s | Estado: %s", 
            ticket->fecha, ticket->estado == 0 ? "PENDIENTE PAGO" : "PAGADO");
    
    mvprintw(11, 5, "========================================");
    mvprintw(12, 5, "PRODUCTO");
    mvprintw(12, 30, "CANT.");
    mvprintw(12, 40, "PRECIO U.");
    mvprintw(12, 55, "SUBTOTAL");
    
    int y = 13;
    float total_calculado = 0.0;
    
    // Obtener productos para tener precios reales
    Peticion pet;
    Respuesta resp_prod;
    memset(&pet, 0, sizeof(Peticion));
    memset(&resp_prod, 0, sizeof(Respuesta));
    
    pet.operacion = OP_LISTAR_PRODUCTOS;
    enviar_peticion_mesero(&pet, &resp_prod);
    
    for(int i = 0; i < ticket->num_items; i++) {
        float precio_unitario = 0.0;
        
        // Buscar precio real del producto
        for(int j = 0; j < resp_prod.num_productos; j++) {
            if(resp_prod.productos[j].id == ticket->items[i].producto_id) {
                precio_unitario = resp_prod.productos[j].precio;
                break;
            }
        }
        
        float subtotal = precio_unitario * ticket->items[i].cantidad;
        total_calculado += subtotal;
        
        mvprintw(y, 5, "%s", ticket->items[i].nombre_producto);
        mvprintw(y, 30, "%d", ticket->items[i].cantidad);
        mvprintw(y, 40, "$%.2f", precio_unitario);
        mvprintw(y, 55, "$%.2f", subtotal);
        y++;
    }
    
    mvprintw(y+1, 5, "========================================");
    attron(A_BOLD);
    mvprintw(y+2, 40, "TOTAL A PAGAR:");
    mvprintw(y+2, 55, "$%.2f", total_calculado);
    attroff(A_BOLD);
    
    // Verificar si coincide con el total del servidor
    if(fabs(total_calculado - ticket->total) > 0.01) {
        attron(COLOR_PAIR(3));
        mvprintw(y+3, 5, "NOTA: Total del servidor: $%.2f", ticket->total);
        attroff(COLOR_PAIR(3));
    }
    
    attroff(COLOR_PAIR(10));
    
    if(ticket->estado == 0) {
        attron(COLOR_PAIR(9) | A_BOLD);
        ui_print_centered(y+5, "Presione 'P' para marcar como PAGADO");
        attroff(COLOR_PAIR(9) | A_BOLD);
    }
    
    ui_print_footer("Presione cualquier tecla para volver...");
    refresh();
    getch();
}

// Función para visualizar tickets
void visualizar_tickets(USUARIO* usuario) {
    // Obtener tickets del servidor
    Peticion pet;
    Respuesta resp;
    memset(&pet, 0, sizeof(Peticion));
    memset(&resp, 0, sizeof(Respuesta));
    
    pet.operacion = OP_LISTAR_TICKETS;
    
    if(enviar_peticion_mesero(&pet, &resp) != 0 || resp.codigo != RESP_OK) {
        ui_show_error("Error al cargar tickets");
        return;
    }
    
    if(resp.num_tickets == 0) {
        ui_show_info("No hay tickets pendientes de pago");
        return;
    }
    
    int salir = 0;
    int seleccionado = 0;
    
    while(!salir) {
        ui_clear_screen();
        ui_print_title("TICKETS PENDIENTES DE PAGO");
        
        attron(COLOR_PAIR(10));
        mvprintw(4, 5, "Mesero: %s | Tickets: %d", usuario->name, resp.num_tickets);
        attroff(COLOR_PAIR(10));
        
        mvprintw(5, 0, " ");
        for(int i = 0; i < COLS; i++) addch('-');
        
        // Mostrar lista de tickets
        int y = 7;
        for(int i = 0; i < resp.num_tickets; i++) {
            if(i == seleccionado) attron(A_REVERSE);
            
            mvprintw(y, 5, "Ticket #%d - Mesa: %s - Total: $%.2f", 
                    resp.tickets[i].id,
                    resp.tickets[i].mesa,
                    resp.tickets[i].total);
            
            if(i == seleccionado) attroff(A_REVERSE);
            y++;
        }
        
        // Instrucciones
        attron(COLOR_PAIR(10));
        mvprintw(LINES-4, 2, "Flechas: navegar | ENTER: ver detalle | P: marcar pagado | ESC: salir");
        mvprintw(LINES-3, 2, "Ticket %d de %d", seleccionado + 1, resp.num_tickets);
        attroff(COLOR_PAIR(10));
        
        refresh();
        int key = getch();
        
        switch(key) {
            case KEY_UP:
                if(seleccionado > 0) seleccionado--;
                break;
            case KEY_DOWN:
                if(seleccionado < resp.num_tickets - 1) seleccionado++;
                break;
            case 10: // ENTER
                mostrar_detalle_ticket(&resp.tickets[seleccionado]);
                break;
            case 'p':
            case 'P':
                if(resp.tickets[seleccionado].estado == 0) {
                    // Marcar como pagado
                    Peticion pet_pago;
                    Respuesta resp_pago;
                    memset(&pet_pago, 0, sizeof(Peticion));
                    memset(&resp_pago, 0, sizeof(Respuesta));
                    
                    pet_pago.operacion = OP_MARCAR_TICKET_PAGADO;
                    pet_pago.pedido_id = resp.tickets[seleccionado].id;
                    
                    if(enviar_peticion_mesero(&pet_pago, &resp_pago) == 0 && 
                       resp_pago.codigo == RESP_OK) {
                        ui_show_success("Ticket marcado como pagado");
                        salir = 1; // Salir para refrescar lista
                    } else {
                        ui_show_error("Error al marcar ticket como pagado");
                    }
                }
                break;
            case 27: // ESC
                salir = 1;
                break;
        }
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
            "Visualizar Tickets",
            "Cerrar Sesion"
        };
        
        for(int i = 0; i < 4; i++) {
            attron(COLOR_PAIR(10));
            ui_print_centered(7 + i*2, opciones[i]);
            attroff(COLOR_PAIR(10));
        }
        
        ui_print_footer("Use flechas para navegar, ENTER para seleccionar");
        refresh();
        
        int opcion = ui_show_menu("", opciones, 4);
        
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
                
                // Calcular layout dinámico
                int columnas, filas, ancho_rec, alto_rec;
                calcular_layout(resp.num_productos, &columnas, &filas, &ancho_rec, &alto_rec);
                
                int margen_x = 2;
                int margen_y = 6;
                int espacio_x = 2;
                int espacio_y = 1;
                
                // Calcular cuántos productos caben en pantalla
                int espacio_disponible = LINES - margen_y - 6;
                int filas_por_pantalla = espacio_disponible / (alto_rec + espacio_y);
                if(filas_por_pantalla < 1) filas_por_pantalla = 1;
                int productos_por_pantalla = filas_por_pantalla * columnas;
                
                int seleccionado = 0;
                int offset_scroll = 0;
                int cantidad_productos[MAX_PRODUCTOS_RESPUESTA];
                memset(cantidad_productos, 0, sizeof(cantidad_productos));
                float total_precio = 0.0;
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
                            
                            dibujar_recuadro_producto(x, y, ancho_rec, alto_rec, &resp.productos[i], i == seleccionado);
                            
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
                    total_precio = 0.0;
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

                        pet_orden.total_pedido = total_precio;  // Enviar total calculado
                        
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
            
            case 2: // Visualizar Tickets (NUEVO)
                visualizar_tickets(usuario);
                break;

            case 3: // Cerrar Sesión
                ui_show_info("Cerrando sesion...");
                salir = 1;
                break;
        }
    }
}