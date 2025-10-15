#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE 256
#define MAX_USER 50
#define MAX_PASS 50

// Trim: elimina espacios en ambos extremos y \n \r \t
void trim(char *s) {
    if (!s) return;
    // trim leading
    char *start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);

    // trim trailing
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[len - 1] = '\0';
        len--;
    }
}

// Función para verificar usuario en el archivo
int verificar_usuario(const char *usuario, const char *password) {
    FILE *file = fopen("usuario.txt", "r");
    if (!file) {
        perror("Error al abrir usuario.txt");
        return 0;
    }

    char linea[MAX_LINE];
    while (fgets(linea, sizeof(linea), file)) {
        // hacemos una copia porque strtok modifica
        char tmp[MAX_LINE];
        strncpy(tmp, linea, sizeof(tmp));
        tmp[sizeof(tmp)-1] = '\0';

        // tokenizar por coma y también eliminar saltos de línea / CR
        char *tok_user = strtok(tmp, ",\n\r");
        char *tok_pass = NULL;
        if (tok_user) tok_pass = strtok(NULL, ",\n\r"); // siguiente campo

        if (!tok_user || !tok_pass) continue; // línea mal formada, saltar

        trim(tok_user);
        trim(tok_pass);

        // comparar
        if (strcmp(usuario, tok_user) == 0 && strcmp(password, tok_pass) == 0) {
            fclose(file);
            return 1; // encontrado
        }
    }

    fclose(file);
    return 0; // no encontrado
}
void custom_password_input(WINDOW *win, int y, int x, char *str, int max_len) {
      int ch, i = 0;
 
      // Colocar el cursor en la posición inicial
      wmove(win, y, x);
 
      // Bucle para capturar la entrada
      while ((ch = wgetch(win)) != '\n' && ch != '\r' && i < max_len) {
          if (ch == KEY_BACKSPACE || ch == 127) { // Manejar la tecla de retroceso
              if (i > 0) {
                  i--;
                  mvwdelch(win, y, x + i); // Borra el asterisco
              }
          } else if (isprint(ch)) { // Solo procesar caracteres imprimibles
              str[i++] = (char)ch;
              mvwaddch(win, y, x + i - 1, '*'); // Imprime el asterisco
          }
          wrefresh(win);
      }
 
      //  termino con un nulo
      str[i] = '\0';
 }

int main() {
    char usuario[MAX_USER];
    char password[MAX_PASS];

    // Iniciar ncurses
    initscr();
    noecho();  // no mostrar caracteres por defecto
    cbreak();
    keypad(stdscr, TRUE);

    int yMax, xMax;
    getmaxyx(stdscr, yMax, xMax);

    // Crear ventana centrada
    int h = 10, w = 50;
    int starty = (yMax - h) / 2;
    int startx = (xMax - w) / 2;
    if (starty < 0) starty = 0;
    if (startx < 0) startx = 0;

    WINDOW *loginwin = newwin(h, w, starty, startx);
    box(loginwin, 0, 0);
    refresh();
    wrefresh(loginwin);

    mvwprintw(loginwin, 1, (w - 10) / 2, "===LOGIN ===");
    mvwprintw(loginwin, 3, 2, "Usuario: ");
    wrefresh(loginwin);

    // Leer usuario
    echo();
    mvwgetnstr(loginwin, 3, 11, usuario, MAX_USER - 1);
    usuario[MAX_USER-1] = '\0';
    trim(usuario); // limpiar espacios/enter

    // Leer contraseña
    mvwprintw(loginwin, 5, 2, "Password: ");
	wrefresh(loginwin);
   	custom_password_input(loginwin, 5, 12, password, MAX_PASS);
    password[MAX_PASS-1] = '\0';
    trim(password);

    // Verificar credenciales
    if (verificar_usuario(usuario, password)) {
        mvwprintw(loginwin, 7, 10, "Acceso concedido!");
    } else {
        mvwprintw(loginwin, 7, 6, "Usuario o clave incorrectos.");
    }

    wrefresh(loginwin);
    wgetch(loginwin); // esperar tecla
    endwin(); // cerrar ncurses

    return 0;
}