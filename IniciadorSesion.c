#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <openssl/md5.h>
#include <string.h>

#define MAX_CHAIN_SIZE 100
#define MAX_USUARIOS   100

typedef struct
{
    char name[MAX_CHAIN_SIZE];
    char user[MAX_CHAIN_SIZE];
    char pass[33];
    char mail[MAX_CHAIN_SIZE];
    char telf[MAX_CHAIN_SIZE];
    int  tipo;
}USUARIO;

USUARIO usuarios[MAX_USUARIOS];
int total_usuarios = 0;

// Funcion para encriptar cadena
void encriptar(const char* input, char* output)
{
    unsigned char digest[MD5_DIGEST_LENGTH];
    MD5((unsigned char*)input, strlen(input), digest);
    for(int i = 0; i < MD5_DIGEST_LENGTH; i++){
        sprintf(&output[i*2], "%02x", (unsigned int)digest[i]);
    }
    output[32] = '\0';
}

int main_menu()
{
    int opcion = 0;
    int tecla;
    char *opciones[] = {
        "Iniciar Sesión",
        "Crear Usuario",
        "Salir"
    };
    int num_opciones = 3;

    clear();

    // Dibujar bordes
    border('|', '|', '-', '-', '+', '+', '+', '+');

    // Titulos
    mvprintw(2, (COLS-18)/2, "Sistema de Comandas");
    mvprintw(3, (COLS-16)/2, "=================");

    // Instruciones
    mvprintw(LINES-3, 2, "Use las flechas para navegar, ENTER para selecionar y ESC para regresar");

    while(1) {
        // Dibujar opciones
        for(int i = 0; i < num_opciones; i++) {
            if(i == opcion) {
                attron(A_REVERSE); // Resaltar opción seleccionada
            }
            mvprintw(8+i*2, (COLS - strlen(opciones[i]))/2, "%s", opciones[i]);
            if(i == opcion) {
                attroff(A_REVERSE);
            }
        }
        
        refresh();
        tecla = getch();
        
        switch(tecla) {
            case KEY_UP:
                opcion--;
                if(opcion < 0) opcion = num_opciones - 1;
                break;
            case KEY_DOWN:
                opcion++;
                if(opcion >= num_opciones) opcion = 0;
                break;
            case 10: // ENTER
                return opcion;
            case 27: // ESC
                return 2; // Salir
            default:
                break;
        }
    }
}

void iniciar_sesion(){}

void crear_usuario(){}

int main()
{
    // Iniciar ncurses
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0); // Ocultar cursor

    // Colores
    if(has_colors())
    {
        start_color();
        init_pair(1, COLOR_CYAN, COLOR_BLACK);
        init_pair(2, COLOR_GREEN, COLOR_BLACK);
        init_pair(3, COLOR_RED, COLOR_BLACK);
    }

    int opcion;
    int salir = 0;

    while(!salir)
    {
        opcion = main_menu();

        switch (opcion)
        {
            case 0: // Iniciar Sesión
                iniciar_sesion();
                break;
            case 1: // Crear Usuario
                crear_usuario();
                break;
            case 2: // Salir
                salir = 1;
                break;
        }
    }

    // Finalizar ncurses
    endwin();
    return 0;
}