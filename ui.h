#ifndef UI_H
#define UI_H

#include <ncurses.h>

// Inicialización y finalización de la UI
void ui_init();
void ui_cleanup();

// Funciones de interfaz básicas
void ui_draw_border();
void ui_clear_screen();
void ui_print_title(const char* title);
void ui_print_footer(const char* message);
void ui_print_centered(int row, const char* text);
void ui_print_colored(int row, int col, const char* text, int color_pair);

// Entrada de datos
void ui_read_input(int row, int col, char* buffer, int max_len, int hide);
void ui_read_input_with_placeholder(int row, int col, char* buffer, int max_len, int hide, const char* placeholder);
int ui_read_char_option(int row, int col, const char* valid_chars);

// Menús
int ui_show_menu(const char* title, char* options[], int num_options);

// Mensajes
void ui_show_success(const char* message);
void ui_show_error(const char* message);
void ui_show_info(const char* message);
void ui_wait_key();

#endif