#include "interfaz_cocina.h"
#include "ui.h"
#include "protocolo.h"
#include <string.h>
#include <stdlib.h>
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
        
        // Truncar si es muy largo
        if(strlen(info_line) > ancho-4) {
            info_line[ancho-4] = '\0';
        }
        mvprintw(y+2, x+2, "%s", info_line);
        
        // Línea 3: Items (todos en una línea)
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
        
        // Truncar si es muy largo
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
        
        // Configuración de recuadros horizontales apilados verticalmente
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