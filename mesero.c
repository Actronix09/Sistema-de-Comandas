#include "mesero.h"
#include "ui.h"
#include <string.h>
#include <stdlib.h>

// Mostrar interfaz principal del mesero
void mesero_show_interface(USUARIO* usuario)
{
    int opcion_seleccionada = 0;
    int tecla;
    int salir = 0;
    
    // Opciones del menú del mesero
    char *opciones[] = {
        "Toma de Pedidos",
        "Procesar Pagos",
        "Cerrar Sesion"
    };
    int num_opciones = 3;

    while(!salir)
    {
        ui_clear_screen();
        
        // Dibujar cabecera con información del mesero
        int box_width = 60;
        int start_col = (COLS - box_width) / 2;
        int start_row = 2;
        
        // MARCO SUPERIOR - Información del mesero
        attron(COLOR_PAIR(9) | A_BOLD);  // Color verde
        // Línea superior
        mvprintw(start_row, start_col, "+");
        for(int i = 0; i < box_width - 2; i++) addch('=');
        addch('+');
        
        // Líneas laterales
        for(int i = 1; i <= 3; i++)
        {
            mvprintw(start_row + i, start_col, "|");
            mvprintw(start_row + i, start_col + box_width - 1, "|");
        }
        
        // Línea inferior
        mvprintw(start_row + 4, start_col, "+");
        for(int i = 0; i < box_width - 2; i++) addch('=');
        addch('+');
        attroff(COLOR_PAIR(9) | A_BOLD);
        
        // Contenido de la cabecera
        attron(COLOR_PAIR(10) | A_BOLD);  // Color blanco sobre azul
        // Mensaje de bienvenida - USAR snprintf para evitar overflow
        char mensaje_bienvenida[150];  // Aumentado de 100 a 150
        snprintf(mensaje_bienvenida, sizeof(mensaje_bienvenida), "BIENVENIDO, %s", usuario->name);
        mvprintw(start_row + 1, (COLS - strlen(mensaje_bienvenida)) / 2, "%s", mensaje_bienvenida);
        
        // Tipo de usuario
        char tipo_usuario[50];
        strncpy(tipo_usuario, "TIPO: MESERO", sizeof(tipo_usuario) - 1);
        tipo_usuario[sizeof(tipo_usuario) - 1] = '\0';
        mvprintw(start_row + 2, (COLS - strlen(tipo_usuario)) / 2, "%s", tipo_usuario);
        
        // Usuario - USAR snprintf para evitar overflow
        char usuario_str[150];  // Aumentado de 50 a 150
        snprintf(usuario_str, sizeof(usuario_str), "USUARIO: %s", usuario->user);
        mvprintw(start_row + 3, (COLS - strlen(usuario_str)) / 2, "%s", usuario_str);
        attroff(COLOR_PAIR(10) | A_BOLD);
        
        // MARCO DE OPCIONES
        int menu_start_row = start_row + 7;
        int menu_box_width = 40;
        int menu_start_col = (COLS - menu_box_width) / 2;
        
        attron(COLOR_PAIR(8) | A_BOLD);  // Color para bordes
        // Línea superior del menú
        mvprintw(menu_start_row, menu_start_col, "+");
        for(int i = 0; i < menu_box_width - 2; i++) addch('-');
        addch('+');
        
        // Título del menú
        mvprintw(menu_start_row + 1, menu_start_col, "|");
        char titulo_menu[] = "  MENU PRINCIPAL  ";
        int titulo_pos = (menu_box_width - strlen(titulo_menu)) / 2;
        mvprintw(menu_start_row + 1, menu_start_col + titulo_pos, "%s", titulo_menu);
        mvprintw(menu_start_row + 1, menu_start_col + menu_box_width - 1, "|");
        
        // Separador
        mvprintw(menu_start_row + 2, menu_start_col, "+");
        for(int i = 0; i < menu_box_width - 2; i++) addch('-');
        addch('+');
        
        // Opciones del menú
        for(int i = 0; i < num_opciones; i++)
        {
            mvprintw(menu_start_row + 3 + i*2, menu_start_col, "|");
            mvprintw(menu_start_row + 3 + i*2, menu_start_col + menu_box_width - 1, "|");
            
            // Línea separadora entre opciones
            if(i < num_opciones - 1)
            {
                mvprintw(menu_start_row + 4 + i*2, menu_start_col, "|");
                for(int j = 1; j < menu_box_width - 1; j++) mvprintw(menu_start_row + 4 + i*2, menu_start_col + j, "-");
                mvprintw(menu_start_row + 4 + i*2, menu_start_col + menu_box_width - 1, "|");
            }
        }
        
        // Línea inferior del menú
        mvprintw(menu_start_row + 3 + num_opciones*2, menu_start_col, "+");
        for(int i = 0; i < menu_box_width - 2; i++) addch('-');
        addch('+');
        attroff(COLOR_PAIR(8) | A_BOLD);
        
        // Mostrar opciones con resaltado
        for(int i = 0; i < num_opciones; i++)
        {
            int opcion_row = menu_start_row + 3 + i*2;
            
            // Crear el texto completo (con o sin ">")
            char texto_completo[100];
            if(i == opcion_seleccionada)
                snprintf(texto_completo, sizeof(texto_completo), "> %s", opciones[i]);
            else
                snprintf(texto_completo, sizeof(texto_completo), "  %s", opciones[i]);
            
            // Calcular posición centrada para ESTE texto
            int longitud_total = strlen(texto_completo);
            int opcion_col = menu_start_col + (menu_box_width - longitud_total) / 2;
            
            // Imprimir con atributos
            if(i == opcion_seleccionada)
            {
                attron(A_REVERSE | COLOR_PAIR(5));
                mvprintw(opcion_row, opcion_col, "%s", texto_completo);
                attroff(A_REVERSE | COLOR_PAIR(5));
            }
            else
            {
                attron(COLOR_PAIR(4));
                mvprintw(opcion_row, opcion_col, "%s", texto_completo);
                attroff(COLOR_PAIR(4));
            }
        }
        
        // Pie de página con instrucciones
        ui_print_footer("Use las flechas ARRIBA/ABAJO para navegar, ENTER para seleccionar");
        
        refresh();
        
        // Leer tecla
        tecla = getch();
        
        switch(tecla)
        {
            case KEY_UP:
                opcion_seleccionada = (opcion_seleccionada - 1 + num_opciones) % num_opciones;
                break;
                
            case KEY_DOWN:
                opcion_seleccionada = (opcion_seleccionada + 1) % num_opciones;
                break;
                
            case 10: // ENTER
                switch(opcion_seleccionada)
                {
                    case 0: // Toma de Pedidos
                        mesero_tomar_pedido(usuario);
                        break;
                        
                    case 1: // Procesar Pagos
                        mesero_procesar_pago(usuario);
                        break;
                        
                    case 2: // Cerrar Sesión
                        salir = 1;
                        ui_clear_screen();
                        ui_show_info("Sesion cerrada. Volviendo al menu principal...");
                        break;
                }
                break;
                
            case 27: // ESC - también cierra sesión
                salir = 1;
                ui_clear_screen();
                ui_show_info("Sesion cerrada. Volviendo al menu principal...");
                break;
        }
    }
}

// Función para tomar pedidos (placeholder por ahora)
void mesero_tomar_pedido(USUARIO* mesero)
{
    ui_clear_screen();
    
    // Marco para el mensaje
    int box_width = 50;
    int start_col = (COLS - box_width) / 2;
    int start_row = LINES / 2 - 3;
    
    attron(COLOR_PAIR(1) | A_BOLD);
    // Línea superior
    mvprintw(start_row, start_col, "+");
    for(int i = 0; i < box_width - 2; i++) addch('=');
    addch('+');
    
    // Líneas laterales
    for(int i = 1; i <= 3; i++)
    {
        mvprintw(start_row + i, start_col, "|");
        mvprintw(start_row + i, start_col + box_width - 1, "|");
    }
    
    // Línea inferior
    mvprintw(start_row + 4, start_col, "+");
        for(int i = 0; i < box_width - 2; i++) addch('=');
        addch('+');
        attroff(COLOR_PAIR(1) | A_BOLD);
    
        // Mensaje
        attron(COLOR_PAIR(10) | A_BOLD);
        char mensaje[] = "TOMA DE PEDIDOS";
        mvprintw(start_row + 1, (COLS - strlen(mensaje)) / 2, "%s", mensaje);
    
        char info[] = "Funcionalidad en desarrollo";
        mvprintw(start_row + 2, (COLS - strlen(info)) / 2, "%s", info);
    
        // USAR snprintf para evitar overflow
        char mesero_info[150];  // Aumentado de 100 a 150
        snprintf(mesero_info, sizeof(mesero_info), "Mesero: %s", mesero->name);
        mvprintw(start_row + 3, (COLS - strlen(mesero_info)) / 2, "%s", mesero_info);
        attroff(COLOR_PAIR(10) | A_BOLD);
    
    ui_print_footer("Presione cualquier tecla para continuar...");
    refresh();
    getch();
}

// Función para procesar pagos (placeholder por ahora)
void mesero_procesar_pago(USUARIO* mesero)
{
    ui_clear_screen();
    
    // Marco para el mensaje
    int box_width = 50;
    int start_col = (COLS - box_width) / 2;
    int start_row = LINES / 2 - 3;
    
    attron(COLOR_PAIR(9) | A_BOLD);
    // Línea superior
    mvprintw(start_row, start_col, "+");
    for(int i = 0; i < box_width - 2; i++) addch('=');
    addch('+');
    
    // Líneas laterales
    for(int i = 1; i <= 3; i++)
    {
        mvprintw(start_row + i, start_col, "|");
        mvprintw(start_row + i, start_col + box_width - 1, "|");
    }
    
    // Línea inferior
    mvprintw(start_row + 4, start_col, "+");
    for(int i = 0; i < box_width - 2; i++) addch('=');
    addch('+');
    attroff(COLOR_PAIR(9) | A_BOLD);
    
    // Mensaje
    attron(COLOR_PAIR(10) | A_BOLD);
    char mensaje[] = "PROCESAR PAGOS";
    mvprintw(start_row + 1, (COLS - strlen(mensaje)) / 2, "%s", mensaje);
    
    char info[] = "Funcionalidad en desarrollo";
    mvprintw(start_row + 2, (COLS - strlen(info)) / 2, "%s", info);
    
    // USAR snprintf para evitar overflow
    char mesero_info[150];  // Aumentado de 100 a 150
    snprintf(mesero_info, sizeof(mesero_info), "Mesero: %s", mesero->name);
    mvprintw(start_row + 3, (COLS - strlen(mesero_info)) / 2, "%s", mesero_info);
    attroff(COLOR_PAIR(10) | A_BOLD);
    
    ui_print_footer("Presione cualquier tecla para continuar...");
    refresh();
    getch();
}