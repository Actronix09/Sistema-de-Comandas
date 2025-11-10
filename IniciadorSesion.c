#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <openssl/evp.h>
#include <string.h>
#include <ctype.h>

#define MAX_CHAIN_SIZE 100
#define MAX_USUARIOS   100

// Definición de pares de colores
#define COLOR_TITULO     1  // Naranja brillante para títulos
#define COLOR_EXITO      2  // Verde para mensajes exitosos
#define COLOR_ERROR      3  // Rojo para errores
#define COLOR_MENU       4  // Café claro para opciones de menú
#define COLOR_SELECCION  5  // Naranja para selección
#define COLOR_BORDE      6  // Café oscuro para bordes
#define COLOR_INFO       7  // Amarillo/naranja suave para información
#define COLOR_FONDO      8  // Fondo general

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
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len;

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(ctx, EVP_md5(), NULL);
    EVP_DigestUpdate(ctx, input, strlen(input));
    EVP_DigestFinal_ex(ctx, digest, &digest_len);
    EVP_MD_CTX_free(ctx);

    for(unsigned int i = 0; i < digest_len; i++){
        sprintf(&output[i*2], "%02x", (unsigned int)digest[i]);
    }
    output[32] = '\0';
}

// Función para cargar usuarios
void cargar_usuarios()
{
    FILE *archivo = fopen("usuarios.txt", "r");
    if(archivo != NULL)
    {
        total_usuarios = 0;
        char linea[512];

        while(fgets(linea, sizeof(linea), archivo) && total_usuarios < MAX_USUARIOS)
        {
            linea[strcspn(linea, "\n")] = 0;

            char *token = strtok(linea, "|");
            if(token) strncpy(usuarios[total_usuarios].name, token, MAX_CHAIN_SIZE-1);

            token = strtok(NULL, "|");
            if(token) strncpy(usuarios[total_usuarios].user, token, MAX_CHAIN_SIZE-1);

            token = strtok(NULL, "|");
            if(token) strncpy(usuarios[total_usuarios].pass, token, 32);

            token = strtok(NULL, "|");
            if(token) strncpy(usuarios[total_usuarios].mail, token, MAX_CHAIN_SIZE-1);

            token = strtok(NULL, "|");
            if(token) strncpy(usuarios[total_usuarios].telf, token, MAX_CHAIN_SIZE-1);

            token = strtok(NULL, "|");
            if(token) usuarios[total_usuarios].tipo = atoi(token);

            total_usuarios++;
        }
        fclose(archivo);
    }
}

// Función para guardar usuarios
void guardar_usuarios()
{
    FILE *archivo = fopen("usuarios.txt", "w");
    if(archivo != NULL)
    {
        for(int i = 0; i < total_usuarios; i++)
        {
            fprintf(archivo, "%s|%s|%s|%s|%s|%d\n",
                    usuarios[i].name,
                    usuarios[i].user,
                    usuarios[i].pass,
                    usuarios[i].mail,
                    usuarios[i].telf,
                    usuarios[i].tipo);
        }
        fclose(archivo);
    }
}

// Funcion para leer la entrada con visualización de caracteres
void leer_input(int fila, int col, char* buffer, int max_len, int ocultar)
{
    int i = 0;
    int tecla;
    buffer[0] = '\0';

    move(fila, col);
    clrtoeol();

    curs_set(1);

    while(1) 
    {
        tecla = getch();
        if(tecla == 10) break; // ENTER
        else if(tecla == 27) // ESC
        {
            buffer[0] = '\0';
            i = 0;
            break;
        }
        else if((tecla == 127 || tecla == 8 || tecla == KEY_BACKSPACE) && i > 0)
        {
            i--;
            buffer[i] = '\0';
            move(fila, col+i);
            addch(' ');
            move(fila, col + i);
        }
        else if(tecla >= 32 && tecla <= 126 && i < max_len - 1)
        {
            buffer[i] = tecla;
            i++;
            buffer[i] = '\0';
            if(ocultar) addch('*');
            else addch(tecla);
        }
        refresh();
    }
    curs_set(0);
}

// Función para verificar contraseña
int check_pass(char pass[MAX_CHAIN_SIZE])
{   
    int charespc = 0;
    int num = 0;
    int mayus = 0;
    int minus = 0;
    int longitud = 0;

    for(int i = 0; pass[i] != '\0' && i < MAX_CHAIN_SIZE; i++)
    {
        longitud ++;
        if(pass[i] >= '0' && pass[i] <= '9') num++;
        else if(pass[i] >= 'A' && pass[i] <= 'Z') mayus++;
        else if(pass[i] >= 'a' && pass[i] <= 'z') minus++;
        else if(pass[i] == '*' || pass[i] == '/' || pass[i] == '-' || pass[i] == '_' || pass[i] == '+' || pass[i] == '.') charespc++;
    }
    return (charespc >= 2 && num > 0 && mayus > 0 && minus > 0 && longitud >= 8) ? 0 : 1;
}

// Función para verificar correo
int check_mail(char mail[MAX_CHAIN_SIZE])
{   
    int arroba = 0;
    int punto = 0;

    for(int i = 0; mail[i] != '\0' && i < MAX_CHAIN_SIZE; i++)
    {
        if(mail[i] == '@') arroba++;
        if(mail[i] == '.') punto++;
    }
    return (arroba == 1 && punto > 0) ? 0 : 1;
}

// Función para verificar numero de telefono
int check_telf(char telf[MAX_CHAIN_SIZE])
{   
    int longitud = 0;
    for(int i = 0; telf[i] != '\0' && i < MAX_CHAIN_SIZE; i++)
    {
        longitud++;
        if(telf[i] < '0' || telf[i] > '9') return 1;
    }
    return (longitud >= 8) ? 0 : 1;
}

void iniciar_sesion()
{
    char user[MAX_CHAIN_SIZE];
    char pass[MAX_CHAIN_SIZE];
    char pass_encrip[33];
    int encontrado = 0;

    clear();
    
    // Borde con color café
    attron(COLOR_PAIR(COLOR_BORDE));
    border('|', '|', '-', '-', '+', '+', '+', '+');
    attroff(COLOR_PAIR(COLOR_BORDE));

    attron(COLOR_PAIR(COLOR_TITULO) | A_BOLD);
    mvprintw(2, (COLS-14)/2, "Iniciar Sesion");
    attroff(COLOR_PAIR(COLOR_TITULO) | A_BOLD);
    
    attron(COLOR_PAIR(COLOR_TITULO));
    mvprintw(3, (COLS-14)/2, "==============");
    attroff(COLOR_PAIR(COLOR_TITULO));

    attron(COLOR_PAIR(COLOR_INFO));
    mvprintw(6, 5, "Usuario:");
    mvprintw(8, 5, "Contrasena:");
    attroff(COLOR_PAIR(COLOR_INFO));

    attron(COLOR_PAIR(COLOR_MENU));
    mvprintw(LINES-3, 2, "Presiona ESC para cancelar");
    attroff(COLOR_PAIR(COLOR_MENU));

    refresh();

    leer_input(6, 14, user, MAX_CHAIN_SIZE, 0);
    if(strlen(user) == 0) return;

    leer_input(8, 18, pass, MAX_CHAIN_SIZE, 1);
    if(strlen(pass) == 0) return;

    encriptar(pass, pass_encrip);

    for(int i = 0; i < total_usuarios; i++)
    {
        if(strcmp(usuarios[i].user, user) == 0 && strcmp(usuarios[i].pass, pass_encrip) == 0)
        {
            encontrado = 1;

            clear();
            attron(COLOR_PAIR(COLOR_BORDE));
            border('|', '|', '-', '-', '+', '+', '+', '+');
            attroff(COLOR_PAIR(COLOR_BORDE));

            attron(COLOR_PAIR(COLOR_EXITO) | A_BOLD);
            mvprintw(LINES/2 - 2, (COLS-20)/2, "Inicio exitoso!");
            attroff(COLOR_PAIR(COLOR_EXITO) | A_BOLD);
            
            attron(COLOR_PAIR(COLOR_INFO));
            mvprintw(LINES/2, (COLS-30)/2, "Bienvenido: %s", usuarios[i].name);
            mvprintw(LINES/2 + 1, (COLS-30)/2, "Tipo de usuario: %s", usuarios[i].tipo == 1 ? "Cocina" : "Mesero");
            attroff(COLOR_PAIR(COLOR_INFO));
            
            attron(COLOR_PAIR(COLOR_MENU));
            mvprintw(LINES-3, 2, "Presione cualquier tecla para continuar...");
            attroff(COLOR_PAIR(COLOR_MENU));
            refresh();
            getch();
            break;
        }
    }

    if(!encontrado)
    {
        clear();
        attron(COLOR_PAIR(COLOR_BORDE));
        border('|', '|', '-', '-', '+', '+', '+', '+');
        attroff(COLOR_PAIR(COLOR_BORDE));

        attron(COLOR_PAIR(COLOR_ERROR) | A_BOLD);
        mvprintw(LINES/2, (COLS-35)/2, "Usuario o contrasena incorrectos");
        attroff(COLOR_PAIR(COLOR_ERROR) | A_BOLD);

        attron(COLOR_PAIR(COLOR_MENU));
        mvprintw(LINES-3, 2, "Presione cualquier tecla para continuar...");
        attroff(COLOR_PAIR(COLOR_MENU));
        refresh();
        getch();
    }
}

void crear_usuario()
{
    if(total_usuarios >= MAX_USUARIOS) {
        clear();
        attron(COLOR_PAIR(COLOR_BORDE));
        border('|', '|', '-', '-', '+', '+', '+', '+');
        attroff(COLOR_PAIR(COLOR_BORDE));
        
        attron(COLOR_PAIR(COLOR_ERROR) | A_BOLD);
        mvprintw(LINES/2, (COLS-40)/2, "Limite de usuarios alcanzado");
        attroff(COLOR_PAIR(COLOR_ERROR) | A_BOLD);
        
        attron(COLOR_PAIR(COLOR_MENU));
        mvprintw(LINES-3, 2, "Presione cualquier tecla para continuar...");
        attroff(COLOR_PAIR(COLOR_MENU));
        refresh();
        getch();
        return;
    }

    USUARIO nuevo;
    char pass[MAX_CHAIN_SIZE];
    char opcion_tipo;
    int val;
    int completado = 0;

    while(!completado) {
        clear();
        attron(COLOR_PAIR(COLOR_BORDE));
        border('|', '|', '-', '-', '+', '+', '+', '+');
        attroff(COLOR_PAIR(COLOR_BORDE));

        attron(COLOR_PAIR(COLOR_TITULO) | A_BOLD);
        mvprintw(2, (COLS-13)/2, "Crear Usuario");
        attroff(COLOR_PAIR(COLOR_TITULO) | A_BOLD);
        
        attron(COLOR_PAIR(COLOR_TITULO));
        mvprintw(3, (COLS-13)/2, "=============");
        attroff(COLOR_PAIR(COLOR_TITULO));

        attron(COLOR_PAIR(COLOR_INFO));
        mvprintw(6, 5, "Nombre completo:");
        mvprintw(8, 5, "Usuario:");
        mvprintw(10, 5, "Contrasena:");
        mvprintw(12, 5, "Email:");
        mvprintw(14, 5, "Telefono:");
        mvprintw(16, 5, "Tipo (C=Cocina, M=Mesero):");
        attroff(COLOR_PAIR(COLOR_INFO));

        attron(COLOR_PAIR(COLOR_MENU));
        mvprintw(LINES-3, 2, "Presione ESC para cancelar");
        attroff(COLOR_PAIR(COLOR_MENU));
        refresh();

        // Nombre
        leer_input(6, 22, nuevo.name, MAX_CHAIN_SIZE, 0);
        if(strlen(nuevo.name) == 0) return; // ESC presionado
        
        // Usuario
        leer_input(8, 14, nuevo.user, MAX_CHAIN_SIZE, 0);
        if(strlen(nuevo.user) == 0) return; // ESC presionado

        // Verificar si el usuario ya existe
        int usuario_existe = 0;
        for(int i = 0; i < total_usuarios; i++) {
            if(strcmp(usuarios[i].user, nuevo.user) == 0) {
                usuario_existe = 1;
                break;
            }
        }

        if(usuario_existe) {
            clear();
            attron(COLOR_PAIR(COLOR_BORDE));
            border('|', '|', '-', '-', '+', '+', '+', '+');
            attroff(COLOR_PAIR(COLOR_BORDE));
            
            attron(COLOR_PAIR(COLOR_ERROR) | A_BOLD);
            mvprintw(LINES/2, (COLS-30)/2, "El usuario ya existe");
            attroff(COLOR_PAIR(COLOR_ERROR) | A_BOLD);
            
            attron(COLOR_PAIR(COLOR_MENU));
            mvprintw(LINES-3, 2, "Presione cualquier tecla para continuar...");
            attroff(COLOR_PAIR(COLOR_MENU));
            refresh();
            getch();
            continue; // Volver a empezar
        }

        // Contraseña
        leer_input(10, 17, pass, MAX_CHAIN_SIZE, 1);
        if(strlen(pass) == 0) return; // ESC presionado
        
        val = check_pass(pass);
        if(val != 0) {
            clear();
            attron(COLOR_PAIR(COLOR_BORDE));
            border('|', '|', '-', '-', '+', '+', '+', '+');
            attroff(COLOR_PAIR(COLOR_BORDE));
            
            attron(COLOR_PAIR(COLOR_ERROR) | A_BOLD);
            mvprintw(LINES/2, (COLS-30)/2, "La contrasena es invalida");
            attroff(COLOR_PAIR(COLOR_ERROR) | A_BOLD);
            
            attron(COLOR_PAIR(COLOR_ERROR));
            mvprintw(LINES/2 + 1, (COLS-90)/2, "Debe tener: 1 mayuscula, 1 minuscula, 1 numero, 2 simbolos(*/_-+.), min 8 caracteres");
            attroff(COLOR_PAIR(COLOR_ERROR));
            
            attron(COLOR_PAIR(COLOR_MENU));
            mvprintw(LINES-3, 2, "Presione cualquier tecla para continuar...");
            attroff(COLOR_PAIR(COLOR_MENU));
            refresh();
            getch();
            continue; // Volver a empezar
        }
        
        // Email
        leer_input(12, 12, nuevo.mail, MAX_CHAIN_SIZE, 0);
        if(strlen(nuevo.mail) == 0) return; // ESC presionado
        
        val = check_mail(nuevo.mail);
        if(val != 0) {
            clear();
            attron(COLOR_PAIR(COLOR_BORDE));
            border('|', '|', '-', '-', '+', '+', '+', '+');
            attroff(COLOR_PAIR(COLOR_BORDE));
            
            attron(COLOR_PAIR(COLOR_ERROR) | A_BOLD);
            mvprintw(LINES/2, (COLS-30)/2, "El correo es invalido");
            attroff(COLOR_PAIR(COLOR_ERROR) | A_BOLD);
            
            attron(COLOR_PAIR(COLOR_MENU));
            mvprintw(LINES-3, 2, "Presione cualquier tecla para continuar...");
            attroff(COLOR_PAIR(COLOR_MENU));
            refresh();
            getch();
            continue; // Volver a empezar
        }
        
        // Teléfono
        leer_input(14, 15, nuevo.telf, MAX_CHAIN_SIZE, 0);
        if(strlen(nuevo.telf) == 0) return; // ESC presionado
        
        val = check_telf(nuevo.telf);
        if(val != 0) {
            clear();
            attron(COLOR_PAIR(COLOR_BORDE));
            border('|', '|', '-', '-', '+', '+', '+', '+');
            attroff(COLOR_PAIR(COLOR_BORDE));
            
            attron(COLOR_PAIR(COLOR_ERROR) | A_BOLD);
            mvprintw(LINES/2, (COLS-30)/2, "El telefono es invalido");
            attroff(COLOR_PAIR(COLOR_ERROR) | A_BOLD);
            
            attron(COLOR_PAIR(COLOR_ERROR));
            mvprintw(LINES/2 + 1, 5, "Solo numeros, minimo 8 digitos");
            attroff(COLOR_PAIR(COLOR_ERROR));
            
            attron(COLOR_PAIR(COLOR_MENU));
            mvprintw(LINES-3, 2, "Presione cualquier tecla para continuar...");
            attroff(COLOR_PAIR(COLOR_MENU));
            refresh();
            getch();
            continue; // Volver a empezar
        }

        // Tipo de usuario
        move(16, 32);
        curs_set(1);
        do {
            opcion_tipo = getch();
            opcion_tipo = toupper(opcion_tipo);
        } while(opcion_tipo != 'C' && opcion_tipo != 'M' && opcion_tipo != 27);
        curs_set(0);

        if(opcion_tipo == 27) return; // ESC presionado

        nuevo.tipo = (opcion_tipo == 'C') ? 1 : 0;

        // Si llegamos aquí, todo está correcto
        completado = 1;
    }

    // Guardar el usuario
    encriptar(pass, nuevo.pass);
    usuarios[total_usuarios] = nuevo;
    total_usuarios++;
    guardar_usuarios();

    // Mensaje de éxito
    clear();
    attron(COLOR_PAIR(COLOR_BORDE));
    border('|', '|', '-', '-', '+', '+', '+', '+');
    attroff(COLOR_PAIR(COLOR_BORDE));

    attron(COLOR_PAIR(COLOR_EXITO) | A_BOLD);
    mvprintw(LINES/2, (COLS-30)/2, "Usuario creado exitosamente!");
    attroff(COLOR_PAIR(COLOR_EXITO) | A_BOLD);

    attron(COLOR_PAIR(COLOR_MENU));
    mvprintw(LINES-3, 2, "Presione cualquier tecla para continuar...");
    attroff(COLOR_PAIR(COLOR_MENU));
    refresh();
    getch();
}

void recuperar_contrasena()
{
    char user[MAX_CHAIN_SIZE];
    char pass[MAX_CHAIN_SIZE];
    char pass_encrip[33];
    int encontrado = -1;
    int val;

    clear();
    attron(COLOR_PAIR(COLOR_BORDE));
    border('|', '|', '-', '-', '+', '+', '+', '+');
    attroff(COLOR_PAIR(COLOR_BORDE));

    attron(COLOR_PAIR(COLOR_TITULO) | A_BOLD);
    mvprintw(2, (COLS-23)/2, "Recuperar Contrasena");
    attroff(COLOR_PAIR(COLOR_TITULO) | A_BOLD);
    
    attron(COLOR_PAIR(COLOR_TITULO));
    mvprintw(3, (COLS-23)/2, "====================");
    attroff(COLOR_PAIR(COLOR_TITULO));

    attron(COLOR_PAIR(COLOR_INFO));
    mvprintw(6, 5, "Ingrese su usuario:");
    attroff(COLOR_PAIR(COLOR_INFO));

    attron(COLOR_PAIR(COLOR_MENU));
    mvprintw(LINES-3, 2, "Presione ESC para cancelar");
    attroff(COLOR_PAIR(COLOR_MENU));
    refresh();

    leer_input(6, 25, user, MAX_CHAIN_SIZE, 0);
    if(strlen(user) == 0) return;

    for(int i = 0; i < total_usuarios; i++)
    {
        if(strcmp(usuarios[i].user, user) == 0)
        {
            encontrado = i;
            break;
        }
    }

    if(encontrado == -1)
    {
        clear();
        attron(COLOR_PAIR(COLOR_BORDE));
        border('|', '|', '-', '-', '+', '+', '+', '+');
        attroff(COLOR_PAIR(COLOR_BORDE));

        attron(COLOR_PAIR(COLOR_ERROR) | A_BOLD);
        mvprintw(LINES/2, (COLS-25)/2, "Usuario no encontrado");
        attroff(COLOR_PAIR(COLOR_ERROR) | A_BOLD);

        attron(COLOR_PAIR(COLOR_MENU));
        mvprintw(LINES-3, 2, "Presione cualquier tecla para continuar...");
        attroff(COLOR_PAIR(COLOR_MENU));
        refresh();
        getch();
        return;
    }

    clear();
    attron(COLOR_PAIR(COLOR_BORDE));
    border('|', '|', '-', '-', '+', '+', '+', '+');
    attroff(COLOR_PAIR(COLOR_BORDE));

    attron(COLOR_PAIR(COLOR_TITULO) | A_BOLD);
    mvprintw(2, (COLS-23)/2, "Recuperar Contrasena");
    attroff(COLOR_PAIR(COLOR_TITULO) | A_BOLD);
    
    attron(COLOR_PAIR(COLOR_TITULO));
    mvprintw(3, (COLS-23)/2, "====================");
    attroff(COLOR_PAIR(COLOR_TITULO));

    attron(COLOR_PAIR(COLOR_EXITO) | A_BOLD);
    mvprintw(6, 5, "Usuario encontrado!");
    attroff(COLOR_PAIR(COLOR_EXITO) | A_BOLD);

    attron(COLOR_PAIR(COLOR_INFO));
    mvprintw(8, 5, "Nombre: %s", usuarios[encontrado].name);
    mvprintw(10, 5, "Nueva contrasena:");
    attroff(COLOR_PAIR(COLOR_INFO));

    attron(COLOR_PAIR(COLOR_MENU));
    mvprintw(LINES-3, 2, "Presione ESC para cancelar");
    attroff(COLOR_PAIR(COLOR_MENU));
    refresh();

    leer_input(10, 24, pass, MAX_CHAIN_SIZE, 1);
    if(strlen(pass) == 0) return;

    val = check_pass(pass);
    if(val != 0)
    {
        clear();
        attron(COLOR_PAIR(COLOR_BORDE));
        border('|', '|', '-', '-', '+', '+', '+', '+');
        attroff(COLOR_PAIR(COLOR_BORDE));
        
        attron(COLOR_PAIR(COLOR_ERROR) | A_BOLD);
        mvprintw(LINES/2, (COLS-30)/2, "La contrasena es invalida");
        attroff(COLOR_PAIR(COLOR_ERROR) | A_BOLD);
        
        attron(COLOR_PAIR(COLOR_ERROR));
        mvprintw(LINES/2 + 1, (COLS-90)/2, "Debe tener: 1 mayuscula, 1 minuscula, 1 numero, 2 simbolos(*/_-+.), min 8 caracteres");
        attroff(COLOR_PAIR(COLOR_ERROR));
        
        attron(COLOR_PAIR(COLOR_MENU));
        mvprintw(LINES-3, 2, "Presione cualquier tecla para continuar...");
        attroff(COLOR_PAIR(COLOR_MENU));
        refresh();
        getch();
        recuperar_contrasena();
    }

    encriptar(pass, pass_encrip);
    strncpy(usuarios[encontrado].pass, pass_encrip, 32);
    guardar_usuarios();

    clear();
    attron(COLOR_PAIR(COLOR_BORDE));
    border('|', '|', '-', '-', '+', '+', '+', '+');
    attroff(COLOR_PAIR(COLOR_BORDE));

    attron(COLOR_PAIR(COLOR_EXITO) | A_BOLD);
    mvprintw(LINES/2, (COLS-35)/2, "Contrasena actualizada exitosamente!");
    attroff(COLOR_PAIR(COLOR_EXITO) | A_BOLD);

    attron(COLOR_PAIR(COLOR_MENU));
    mvprintw(LINES-3, 2, "Presione cualquier tecla para continuar...");
    attroff(COLOR_PAIR(COLOR_MENU));
    refresh();
    getch();
}

int main_menu()
{
    int opcion = 0;
    int tecla;
    char *opciones[] = {
        "Iniciar Sesion",
        "Crear Usuario",
        "Recuperar Contrasena",
        "Salir"
    };
    int num_opciones = 4;

    clear();

    // Borde con color café
    attron(COLOR_PAIR(COLOR_BORDE));
    border('|', '|', '-', '-', '+', '+', '+', '+');
    attroff(COLOR_PAIR(COLOR_BORDE));

    // Títulos con color naranja
    attron(COLOR_PAIR(COLOR_TITULO) | A_BOLD);
    mvprintw(2, (COLS-18)/2, "Sistema de Comandas");
    attroff(COLOR_PAIR(COLOR_TITULO) | A_BOLD);
    
    attron(COLOR_PAIR(COLOR_TITULO));
    mvprintw(3, (COLS-19)/2, "===================");
    attroff(COLOR_PAIR(COLOR_TITULO));

    // Instrucciones
    attron(COLOR_PAIR(COLOR_MENU));
    mvprintw(LINES-3, 2, "Use las flechas para navegar, ENTER para seleccionar y ESC para salir");
    attroff(COLOR_PAIR(COLOR_MENU));

    while(1) {
        for(int i = 0; i < num_opciones; i++) {
            // Limpiar la línea completa antes de dibujar
            move(8+i*2, (COLS - strlen(opciones[i]))/2 - 3);
            clrtoeol();
            
            if(i == opcion) {
                // Opción seleccionada en naranja brillante
                attron(COLOR_PAIR(COLOR_SELECCION) | A_BOLD);
                mvprintw(8+i*2, (COLS - strlen(opciones[i]))/2 - 2, "> %s <", opciones[i]);
                attroff(COLOR_PAIR(COLOR_SELECCION) | A_BOLD);
            } else {
                // Opciones normales en café claro
                attron(COLOR_PAIR(COLOR_MENU));
                mvprintw(8+i*2, (COLS - strlen(opciones[i]))/2, "%s", opciones[i]);
                attroff(COLOR_PAIR(COLOR_MENU));
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
                return 3;
            default:
                break;
        }
    }
}

int main()
{
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);

    // Configuración de colores tema restaurante
    if(has_colors())
    {
        start_color();
        
        // Usar el fondo por defecto o cambiarlo si el terminal lo soporta
        use_default_colors();
        
        // Definir colores personalizados con fondo café oscuro
        init_pair(COLOR_FONDO, COLOR_WHITE, COLOR_BLACK);       // Fondo general
        init_pair(COLOR_TITULO, COLOR_YELLOW, COLOR_BLACK);     // Títulos en amarillo/naranja
        init_pair(COLOR_EXITO, COLOR_GREEN, COLOR_BLACK);       // Mensajes de éxito
        init_pair(COLOR_ERROR, COLOR_RED, COLOR_BLACK);         // Mensajes de error
        init_pair(COLOR_MENU, COLOR_CYAN, COLOR_BLACK);         // Opciones de menú en café claro
        init_pair(COLOR_SELECCION, COLOR_YELLOW, COLOR_BLACK);  // Selección en naranja brillante
        init_pair(COLOR_BORDE, COLOR_YELLOW, COLOR_BLACK);      // Bordes en café
        init_pair(COLOR_INFO, COLOR_WHITE, COLOR_BLACK);        // Información general
        
        // Aplicar color de fondo a toda la pantalla
        bkgd(COLOR_PAIR(COLOR_FONDO));
    }

    // Cargar usuarios existentes
    cargar_usuarios();

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
            case 2: // Recuperar Contraseña
                recuperar_contrasena();
                break;
            case 3: // Salir
                salir = 1;
                break;
        }
    }

    // Finalizar ncurses
    endwin();
    return 0;
}