#include "ui.h"
#include <string.h>
#include <ctype.h>

// Inicializar ncurses
void ui_init()
{
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    if(has_colors())
    {
        start_color();
        
        // Configurar colores de fondo
        init_pair(1, COLOR_YELLOW, COLOR_BLUE);    // Títulos - amarillo sobre azul
        init_pair(2, COLOR_GREEN, COLOR_BLACK);     // Éxito - verde sobre negro
        init_pair(3, COLOR_WHITE, COLOR_RED);       // Error - blanco sobre rojo
        init_pair(4, COLOR_CYAN, COLOR_BLUE);       // Texto normal - cyan sobre azul
        init_pair(5, COLOR_BLACK, COLOR_YELLOW);    // Resaltado - negro sobre amarillo
        init_pair(6, COLOR_WHITE, COLOR_MAGENTA);   // Requisitos - blanco sobre magenta
        init_pair(7, COLOR_BLACK, COLOR_CYAN);      // Footer - negro sobre cyan
        init_pair(8, COLOR_YELLOW, COLOR_MAGENTA);  // Bordes especiales
        init_pair(9, COLOR_BLACK, COLOR_GREEN);     // Éxito alternativo
        init_pair(10, COLOR_WHITE, COLOR_BLUE);     // Texto importante
        
        // Establecer color de fondo por defecto
        bkgd(COLOR_PAIR(4));
    }
}

// Finalizar ncurses
void ui_cleanup()
{
    endwin();
}

// Dibujar borde
void ui_draw_border()
{
    border('|', '|', '-', '-', '+', '+', '+', '+');
}

// Limpiar pantalla y dibujar borde
void ui_clear_screen()
{
    clear();
    ui_draw_border();
}

// Imprimir título centrado
void ui_print_title(const char* title)
{
    int len = strlen(title);
    attron(COLOR_PAIR(1));
    mvprintw(2, (COLS - len)/2, "%s", title);
    attroff(COLOR_PAIR(1));
    
    // Línea de subrayado
    mvprintw(3, (COLS - len)/2, "%.*s", len, "====================");
}

// Imprimir mensaje en el pie de página
void ui_print_footer(const char* message)
{
    mvprintw(LINES-3, 2, "%s", message);
}

// Imprimir texto centrado en una fila
void ui_print_centered(int row, const char* text)
{
    mvprintw(row, (COLS - strlen(text))/2, "%s", text);
}

// Imprimir texto con color
void ui_print_colored(int row, int col, const char* text, int color_pair)
{
    attron(COLOR_PAIR(color_pair));
    mvprintw(row, col, "%s", text);
    attroff(COLOR_PAIR(color_pair));
}

// Leer entrada del usuario
void ui_read_input(int row, int col, char* buffer, int max_len, int hide)
{
    int i = 0;
    int key;
    buffer[0] = '\0';

    move(row, col);
    clrtoeol();
    curs_set(1);

    while(1)
    {
        key = getch();
        
        if(key == 10) break; // ENTER
        
        if(key == 27) // ESC
        {
            buffer[0] = '\0';
            i = 0;
            break;
        }
        
        if((key == 127 || key == 8 || key == KEY_BACKSPACE) && i > 0) // Backspace
        {
            i--;
            buffer[i] = '\0';
            move(row, col + i);
            addch(' ');
            move(row, col + i);
        }
        else if(key >= 32 && key <= 126 && i < max_len - 1)
        {
            buffer[i] = key;
            i++;
            buffer[i] = '\0';
            addch(hide ? '*' : key);
        }
        
        refresh();
    }
    
    curs_set(0);
}

// Leer un carácter de opción
int ui_read_char_option(int row, int col, const char* valid_chars)
{
    move(row, col);
    curs_set(1);
    
    int key;
    do {
        key = getch();
        key = toupper(key);
        
        if(key == 27) // ESC
        {
            curs_set(0);
            return 27;
        }
        
        // Verificar si es válido
        for(int i = 0; valid_chars[i] != '\0'; i++)
        {
            if(key == toupper(valid_chars[i]))
            {
                curs_set(0);
                return key;
            }
        }
    } while(1);
}

// Mostrar menú interactivo
int ui_show_menu(const char* title, char* options[], int num_options)
{
    int selected = 0;
    int key;

    while(1)
    {
        ui_clear_screen();
        ui_print_title(title);
        ui_print_footer("Use las flechas para navegar, ENTER para seleccionar y ESC para salir");

        // Dibujar opciones
        for(int i = 0; i < num_options; i++)
        {
            if(i == selected)
                attron(A_REVERSE);
            
            ui_print_centered(8 + i*2, options[i]);
            
            if(i == selected)
                attroff(A_REVERSE);
        }

        refresh();
        key = getch();

        switch(key)
        {
            case KEY_UP:
                selected = (selected - 1 + num_options) % num_options;
                break;
            case KEY_DOWN:
                selected = (selected + 1) % num_options;
                break;
            case 10: // ENTER
                return selected;
            case 27: // ESC
                return num_options - 1; // Última opción (generalmente salir)
        }
    }
}

// Mostrar mensaje de éxito
void ui_show_success(const char* message)
{
    ui_clear_screen();
    ui_print_colored(LINES/2, (COLS - strlen(message))/2, message, 2);
    ui_print_footer("Presione cualquier tecla para continuar...");
    refresh();
    getch();
}

// Mostrar mensaje de error
void ui_show_error(const char* message)
{
    ui_clear_screen();
    ui_print_colored(LINES/2, (COLS - strlen(message))/2, message, 3);
    ui_print_footer("Presione cualquier tecla para continuar...");
    refresh();
    getch();
}

// Mostrar mensaje informativo
void ui_show_info(const char* message)
{
    ui_clear_screen();
    ui_print_centered(LINES/2, message);
    ui_print_footer("Presione cualquier tecla para continuar...");
    refresh();
    getch();
}

// Esperar tecla
void ui_wait_key()
{
    getch();
}