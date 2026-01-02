#include "sesion.h"
#include "usuario.h"
#include "ui.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// Iniciar sesión
void sesion_iniciar()
{
    int intentar_nuevamente = 1;
    
    while(intentar_nuevamente)
    {
        char user[MAX_CHAIN_SIZE];
        char pass[MAX_CHAIN_SIZE];

        ui_clear_screen();
        ui_print_title("Iniciar Sesion");
        
        attron(COLOR_PAIR(10) | A_BOLD);
        mvprintw(6, 5, "Usuario:");
        mvprintw(8, 5, "Contrasena:");
        attroff(COLOR_PAIR(10) | A_BOLD);
        ui_print_footer("Presiona ESC para cancelar");
        refresh();

        // Leer usuario
        ui_read_input(6, 14, user, MAX_CHAIN_SIZE, 0);
        if(strlen(user) == 0) return; // ESC presionado

        // Leer contraseña
        ui_read_input(8, 18, pass, MAX_CHAIN_SIZE, 1);
        if(strlen(pass) == 0) return; // ESC presionado

        // Autenticar
        int index = usuario_autenticar(user, pass);
        
        if(index >= 0)
        {
            USUARIO* u = usuario_get_by_index(index);
            
            ui_clear_screen();
            
            int msg_width = 50;
            int start_col = (COLS - msg_width)/2;
            int start_row = LINES/2 - 5;
            
            // CUADRO VERDE - Mensaje de inicio exitoso
            attron(COLOR_PAIR(9) | A_BOLD);
            mvprintw(start_row, start_col, "+");
            for(int i = 0; i < msg_width - 2; i++) addch('=');
            addch('+');
            
            mvprintw(start_row + 1, start_col, "|");
            mvprintw(start_row + 1, start_col + msg_width - 1, "|");
            
            mvprintw(start_row + 2, start_col, "|");
            int texto_len = 20;
            mvprintw(start_row + 2, (COLS - texto_len)/2, "    Inicio exitoso!  ");
            mvprintw(start_row + 2, start_col + msg_width - 1, "|");
            
            mvprintw(start_row + 3, start_col, "|");
            mvprintw(start_row + 3, start_col + msg_width - 1, "|");
            
            mvprintw(start_row + 4, start_col, "+");
            for(int i = 0; i < msg_width - 2; i++) addch('=');
            addch('+');
            attroff(COLOR_PAIR(9) | A_BOLD);
            
            // Información del usuario
            start_row += 6;
            
            attron(COLOR_PAIR(10) | A_BOLD);
            mvprintw(start_row, start_col, "+");
            for(int i = 0; i < msg_width - 2; i++) addch('=');
            addch('+');
            
            mvprintw(start_row + 1, start_col, "|");
            mvprintw(start_row + 1, start_col + msg_width - 1, "|");
            attroff(COLOR_PAIR(10) | A_BOLD);
            
            attron(COLOR_PAIR(10));
            char msg[256];
            sprintf(msg, "Bienvenido: %s", u->name);
            mvprintw(start_row + 1, (COLS - strlen(msg))/2, "%s", msg);
            
            attron(COLOR_PAIR(10) | A_BOLD);
            mvprintw(start_row + 2, start_col, "|");
            mvprintw(start_row + 2, start_col + msg_width - 1, "|");
            attroff(COLOR_PAIR(10) | A_BOLD);
            
            attron(COLOR_PAIR(10));
            sprintf(msg, "Tipo: %s", u->tipo == 2 ? "Administrador" : (u->tipo == 1 ? "Cocina" : "Mesero"));
            mvprintw(start_row + 2, (COLS - strlen(msg))/2, "%s", msg);
            
            attron(COLOR_PAIR(10) | A_BOLD);
            mvprintw(start_row + 3, start_col, "|");
            mvprintw(start_row + 3, start_col + msg_width - 1, "|");
            
            mvprintw(start_row + 4, start_col, "+");
            for(int i = 0; i < msg_width - 2; i++) addch('=');
            addch('+');
            attroff(COLOR_PAIR(10) | A_BOLD);
            
            ui_print_footer("Presione cualquier tecla para continuar...");
            refresh();
            ui_wait_key();
            
            return;
        }
        else
        {
            ui_clear_screen();
            ui_print_colored(LINES/2 - 1, (COLS-35)/2, "Usuario o contrasena incorrectos", 3);
            mvprintw(LINES/2 + 1, (COLS-40)/2, "Desea intentar nuevamente? (S/N)");
            ui_print_footer("Presione S para reintentar o N para volver al menu");
            refresh();
            
            int respuesta = ui_read_char_option(LINES/2 + 2, COLS/2, "SN");
            
            if(respuesta == 'N' || respuesta == 27) // N o ESC
            {
                intentar_nuevamente = 0;
            }
        }
    }
}

// Crear usuario
void sesion_crear_usuario()
{
    int intentar_nuevamente = 1;
    
    while(intentar_nuevamente)
    {
        if(usuario_get_total() >= MAX_USUARIOS)
        {
            ui_show_error("Limite de usuarios alcanzado");
            return;
        }

        char name[MAX_CHAIN_SIZE];
        char user[MAX_CHAIN_SIZE];
        char pass[MAX_CHAIN_SIZE];
        char mail[MAX_CHAIN_SIZE];
        char telf[MAX_CHAIN_SIZE];
        int tipo;
        int error_encontrado = 0;
        char mensaje_error[256];

        ui_clear_screen();
        ui_print_title("Crear Usuario");
        
        attron(COLOR_PAIR(10) | A_BOLD);
        mvprintw(6, 5, "Nombre completo:");
        mvprintw(8, 5, "Usuario:");
        mvprintw(10, 5, "Contrasena:");
        mvprintw(12, 5, "Email:");
        mvprintw(14, 5, "Telefono:");
        mvprintw(16, 5, "Tipo (A=Administrador, C=Cocina, M=Mesero):");
        attroff(COLOR_PAIR(10) | A_BOLD);
        
        ui_print_footer("Presione ESC para cancelar");
        refresh();

        // Leer nombre
        ui_read_input(6, 22, name, MAX_CHAIN_SIZE, 0);
        if(strlen(name) == 0) return; // ESC presionado

        // Leer usuario
        ui_read_input(8, 14, user, MAX_CHAIN_SIZE, 0);
        if(strlen(user) == 0) return; // ESC presionado

        // Verificar si existe
        if(usuario_existe(user))
        {
            strcpy(mensaje_error, "El usuario ya existe");
            error_encontrado = 1;
        }

        if(!error_encontrado)
        {
            // Mostrar requisitos de contraseña
            attron(COLOR_PAIR(6) | A_BOLD);
            mvprintw(1, COLS - 45, "+==========================+");
            mvprintw(2, COLS - 45, "| REQUISITOS DE CONTRASENA |");
            mvprintw(3, COLS - 45, "+==========================+");
            attroff(COLOR_PAIR(6) | A_BOLD);
            attron(COLOR_PAIR(6));
            mvprintw(4, COLS - 45, "| > Minimo 8 caracteres    |");
            mvprintw(5, COLS - 45, "| > 1 mayuscula (A-Z)      |");
            mvprintw(6, COLS - 45, "| > 1 minuscula (a-z)      |");
            mvprintw(7, COLS - 45, "| > 1 numero (0-9)         |");
            mvprintw(8, COLS - 45, "| > 2 simbolos (*/_-+.)    |");
            attroff(COLOR_PAIR(6));
            attron(COLOR_PAIR(6) | A_BOLD);
            mvprintw(9, COLS - 45, "+==========================+");
            attroff(COLOR_PAIR(6) | A_BOLD);
            refresh();
            
            // Leer contraseña
            ui_read_input(10, 17, pass, MAX_CHAIN_SIZE, 1);
            if(strlen(pass) == 0) return; // ESC presionado

            if(validar_password(pass) != 0)
            {
                strcpy(mensaje_error, "La contrasena es invalida");
                error_encontrado = 2; // Tipo 2 para mostrar requisitos
            }
        }

        if(!error_encontrado)
        {
            // Limpiar requisitos anteriores y mostrar requisitos de email
            for(int i = 1; i <= 9; i++)
            {
                move(i, COLS - 45);
                clrtoeol();
            }
            
            attron(COLOR_PAIR(6) | A_BOLD);
            mvprintw(1, COLS - 45, "+==========================+");
            mvprintw(2, COLS - 45, "|   REQUISITOS DE EMAIL    |");
            mvprintw(3, COLS - 45, "+==========================+");
            attroff(COLOR_PAIR(6) | A_BOLD);
            attron(COLOR_PAIR(6));
            mvprintw(4, COLS - 45, "| > 1 arroba (@)           |");
            mvprintw(5, COLS - 45, "| > Al menos 1 punto (.)   |");
            mvprintw(6, COLS - 45, "|                          |");
            mvprintw(7, COLS - 45, "| Ejemplo:                 |");
            mvprintw(8, COLS - 45, "| usuario@correo.com       |");
            attroff(COLOR_PAIR(6));
            attron(COLOR_PAIR(6) | A_BOLD);
            mvprintw(9, COLS - 45, "+==========================+");
            attroff(COLOR_PAIR(6) | A_BOLD);
            refresh();
            
            // Leer email
            ui_read_input(12, 12, mail, MAX_CHAIN_SIZE, 0);
            if(strlen(mail) == 0) return; // ESC presionado

            if(validar_email(mail) != 0)
            {
                strcpy(mensaje_error, "El correo es invalido");
                error_encontrado = 1;
            }
        }

        if(!error_encontrado)
        {
            // Limpiar requisitos anteriores y mostrar requisitos de teléfono
            for(int i = 1; i <= 9; i++)
            {
                move(i, COLS - 45);
                clrtoeol();
            }
            
            attron(COLOR_PAIR(6) | A_BOLD);
            mvprintw(1, COLS - 45, "+==========================+");
            mvprintw(2, COLS - 45, "|  REQUISITOS DE TELEFONO  |");
            mvprintw(3, COLS - 45, "+==========================+");
            attroff(COLOR_PAIR(6) | A_BOLD);
            attron(COLOR_PAIR(6));
            mvprintw(4, COLS - 45, "| > Minimo 8 digitos       |");
            mvprintw(5, COLS - 45, "| > Solo numeros (0-9)     |");
            mvprintw(6, COLS - 45, "|                          |");
            mvprintw(7, COLS - 45, "| Ejemplo:                 |");
            mvprintw(8, COLS - 45, "| 5512345678               |");
            attroff(COLOR_PAIR(6));
            attron(COLOR_PAIR(6) | A_BOLD);
            mvprintw(9, COLS - 45, "+==========================+");
            attroff(COLOR_PAIR(6) | A_BOLD);
            refresh();
            
            // Leer teléfono
            ui_read_input(14, 15, telf, MAX_CHAIN_SIZE, 0);
            if(strlen(telf) == 0) return; // ESC presionado

            if(validar_telefono(telf) != 0)
            {
                strcpy(mensaje_error, "El telefono es invalido");
                error_encontrado = 3; // Tipo 3 para mostrar requisitos de teléfono
            }
        }

        if(!error_encontrado)
        {
            // Leer tipo
            int opcion = ui_read_char_option(16, 38, "ACM");
            if(opcion == 27) return; // ESC

            if(opcion == 'A') tipo = 2;
            else if(opcion == 'C') tipo = 1;
            else tipo = 0;

            // Crear usuario
            int resultado = usuario_crear(name, user, pass, mail, telf, tipo);
            
            if(resultado == 0)
            {
                ui_show_success("Usuario creado exitosamente!");
                return; // Salir exitosamente
            }
            else
            {
                strcpy(mensaje_error, "Error al crear usuario");
                error_encontrado = 1;
            }
        }

        // Si hubo error, mostrar y preguntar si reintentar
        if(error_encontrado)
        {
            ui_clear_screen();
            ui_print_colored(LINES/2 - 1, (COLS-strlen(mensaje_error))/2, mensaje_error, 3);
            
            if(error_encontrado == 2)
            {
                mvprintw(LINES/2, (COLS-90)/2, 
                        "Debe tener: 1 mayuscula, 1 minuscula, 1 numero, 2 simbolos(*/_-+.), min 8 caracteres");
            }
            else if(error_encontrado == 3)
            {
                mvprintw(LINES/2, (COLS-40)/2, "Solo numeros, minimo 8 digitos");
            }
            
            mvprintw(LINES/2 + 2, (COLS-40)/2, "Desea intentar nuevamente? (S/N)");
            ui_print_footer("Presione S para reintentar o N para volver al menu");
            refresh();
            
            int respuesta = ui_read_char_option(LINES/2 + 3, COLS/2, "SN");
            
            if(respuesta == 'N' || respuesta == 27) // N o ESC
            {
                intentar_nuevamente = 0;
            }
        }
    }
}

// Recuperar contraseña
void sesion_recuperar_password()
{
    char user[MAX_CHAIN_SIZE];
    char new_pass[MAX_CHAIN_SIZE];

    ui_clear_screen();
    ui_print_title("Recuperar Contrasena");
    
    attron(COLOR_PAIR(10) | A_BOLD);
    mvprintw(6, 5, "Ingrese su usuario:");
    attroff(COLOR_PAIR(10) | A_BOLD);
    ui_print_footer("Presione ESC para cancelar");
    refresh();

    // Leer usuario
    ui_read_input(6, 25, user, MAX_CHAIN_SIZE, 0);
    if(strlen(user) == 0) return;

    // Buscar usuario
    int encontrado = -1;
    for(int i = 0; i < usuario_get_total(); i++)
    {
        USUARIO* u = usuario_get_by_index(i);
        if(strcmp(u->user, user) == 0)
        {
            encontrado = i;
            break;
        }
    }

    if(encontrado == -1)
    {
        ui_show_error("Usuario no encontrado");
        return;
    }

    // Mostrar usuario encontrado
    USUARIO* u = usuario_get_by_index(encontrado);
    
    ui_clear_screen();
    ui_print_title("Recuperar Contrasena");
    
    attron(COLOR_PAIR(9) | A_BOLD);
    mvprintw(6, 5, "> Usuario encontrado!");
    attroff(COLOR_PAIR(9) | A_BOLD);
    
    char msg[256];
    sprintf(msg, "Nombre: %s", u->name);
    attron(COLOR_PAIR(10));
    mvprintw(8, 5, "%s", msg);
    mvprintw(10, 5, "Nueva contrasena:");
    attroff(COLOR_PAIR(10));
    
    ui_print_footer("Presione ESC para cancelar");
    refresh();

    // Leer nueva contraseña
    ui_read_input(10, 24, new_pass, MAX_CHAIN_SIZE, 1);
    if(strlen(new_pass) == 0) return;

    if(validar_password(new_pass) != 0)
    {
        ui_clear_screen();
        ui_print_colored(LINES/2, (COLS-30)/2, "La contrasena es invalida", 3);
        mvprintw(LINES/2 + 1, (COLS-90)/2, 
                "Debe tener: 1 mayuscula, 1 minuscula, 1 numero, 2 simbolos(*/_-+.), min 8 caracteres");
        ui_print_footer("Presione cualquier tecla para continuar...");
        refresh();
        ui_wait_key();
        return;
    }

    // Actualizar contraseña
    int resultado = usuario_recuperar_pass(user, new_pass);
    
    if(resultado == 0)
    {
        ui_show_success("Contrasena actualizada exitosamente!");
    }
    else
    {
        ui_show_error("Error al actualizar contrasena");
    }
}
