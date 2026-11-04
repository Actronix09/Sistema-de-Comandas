#include "recuperar_password.h"
#include "usuario.h"
#include "protocolo.h"
#include "ui.h"
#include "interfaces.h"
#include <stdio.h>
#include <string.h>
#include <ncurses.h>

// Mostrar un error y preguntar si se desea intentar de nuevo (1 = si, 0 = no)
static int preguntar_reintentar(const char* mensaje_error) {
    ui_clear_screen();
    ui_print_colored(LINES/2 - 3, (COLS-strlen(mensaje_error))/2, mensaje_error, 3);
    mvprintw(LINES/2 + 1, (COLS-strlen("Intentar otra vez?"))/2, "Intentar otra vez?");

    int centro_x = (COLS - 32) / 2;
    int boton_y = LINES/2 + 3;
    int boton_ancho = 12;
    int boton_alto = 3;

    int seleccion = 0; // 0 para Si, 1 para No

    while(1) {
        dibujar_botones_si_no(centro_x, boton_y, seleccion, boton_ancho, boton_alto);
        ui_print_footer("Use flechas para navegar y Enter para seleccionar");
        refresh();

        int tecla = getch();
        if(tecla == KEY_LEFT || tecla == KEY_RIGHT) {
            seleccion = 1 - seleccion;
        } else if(tecla == 10) { // Enter
            return (seleccion == 0) ? 1 : 0;
        } else if(tecla == 27) { // ESC
            return 0;
        }
    }
}

// Consultar al servidor si el usuario existe y si no es administrador
int verificar_usuario_recuperable(const char* user, char* motivo, int motivo_len) {
    Peticion pet;
    Respuesta resp;
    memset(&pet, 0, sizeof(Peticion));
    memset(&resp, 0, sizeof(Respuesta));

    pet.operacion = OP_VERIFICAR_USUARIO;
    strncpy(pet.user, user, MAX_CHAIN_SIZE-1);

    if(enviar_peticion(&pet, &resp) != 0) {
        snprintf(motivo, motivo_len, "Error de comunicacion con el servidor");
        return -1;
    }

    if(resp.codigo != RESP_USUARIO_EXISTE) {
        snprintf(motivo, motivo_len, "El usuario no existe");
        return -2;
    }

    if(resp.tipo_usuario == 2) {
        snprintf(motivo, motivo_len, "No se puede recuperar la contrasena de un administrador");
        return -3;
    }

    motivo[0] = '\0';
    return 0;
}

// Enviar la nueva contraseña al servidor
int aplicar_nueva_password(const char* user, const char* new_pass, char* motivo, int motivo_len) {
    Peticion pet;
    Respuesta resp;
    memset(&pet, 0, sizeof(Peticion));
    memset(&resp, 0, sizeof(Respuesta));

    pet.operacion = OP_RECUPERAR_PASS;
    strncpy(pet.user, user, MAX_CHAIN_SIZE-1);
    strncpy(pet.pass, new_pass, MAX_CHAIN_SIZE-1);

    if(enviar_peticion(&pet, &resp) != 0) {
        snprintf(motivo, motivo_len, "Error de comunicacion con el servidor");
        return -1;
    }

    if(resp.codigo != RESP_OK) {
        snprintf(motivo, motivo_len, "%s", resp.mensaje);
        return -1;
    }

    motivo[0] = '\0';
    return 0;
}

// Interfaz de recuperación de contraseña
void recuperar_password() {
    int intentar_nuevamente = 1;

    while(intentar_nuevamente) {
        char user[MAX_CHAIN_SIZE];
        char new_pass[MAX_CHAIN_SIZE];
        char mensaje_error[256];

        // Pedir el usuario y verificarlo
        ui_clear_screen();
        ui_print_title("Recuperar Contrasena");

        attron(COLOR_PAIR(10) | A_BOLD);
        mvprintw(6, 5, "Usuario:");
        attroff(COLOR_PAIR(10) | A_BOLD);

        ui_print_footer("Presione ESC para cancelar");
        refresh();

        ui_read_input(6, 14, user, MAX_CHAIN_SIZE, 0);
        if(strlen(user) == 0) return;

        if(verificar_usuario_recuperable(user, mensaje_error, sizeof(mensaje_error)) != 0) {
            intentar_nuevamente = preguntar_reintentar(mensaje_error);
            continue;
        }

        // UI contraseña
        ui_clear_screen();
        ui_print_title("Recuperar Contrasena");

        attron(COLOR_PAIR(10) | A_BOLD);
        mvprintw(6, 5, "Usuario:");
        mvprintw(8, 5, "Nueva contrasena:");
        attroff(COLOR_PAIR(10) | A_BOLD);

        attron(COLOR_PAIR(9) | A_BOLD);
        mvprintw(6, 14, "%s", user);
        attroff(COLOR_PAIR(9) | A_BOLD);

        // Mostrar requisitos de contraseña
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

        ui_print_footer("Presione ESC para cancelar");
        refresh();

        ui_read_input(8, 24, new_pass, MAX_CHAIN_SIZE, 1);
        if(strlen(new_pass) == 0) return;

        if(validar_password(new_pass) != 0) {
            intentar_nuevamente = preguntar_reintentar("La contrasena es invalida");
            continue;
        }

        // Enviar la nueva contrasena al servidor
        if(aplicar_nueva_password(user, new_pass, mensaje_error, sizeof(mensaje_error)) != 0) {
            intentar_nuevamente = preguntar_reintentar(mensaje_error);
            continue;
        }

        ui_show_success("Contrasena cambiada exitosamente!");
        return;
    }
}
