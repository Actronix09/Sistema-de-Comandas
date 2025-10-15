#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ncurses.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include <sys/sem.h>
#include <ctype.h>

#define MAX_USUARIO 50
#define MAX_CONTRASENA 20
#define MAX_NOMBRE 50
#define MAX_EMAIL 50
#define MAX_TELEFONO 15
#define MSG_SIZE 256
#define ARCHIVO_USUARIOS "Usuarios.txt"
#define ARCHIVO_REGISTROS "registro_usuarios.txt"
#define REGISTRO_KEY 9999
#define MAX_CLIENTES 500
#define ARCHIVO_PRODUCTOS "productos.txt"
#define ARCHIVO_CARRITO "carrito.txt"
#define ARCHIVO_COMPRAS "compras.txt"

typedef struct
{
    char mensaje[MSG_SIZE];
    char usuario[MAX_USUARIO];
} Mensaje;

void mostrar_catalogo(const char *usuario);
void mostrar_catalogo_por_seccion(const char *usuario);
void ver_carrito(const char *usuario);
void realizar_compra(const char *usuario);
void ver_perfil_usuario(const char *usuario);
int menu_usuario(const char *usuario, Mensaje *mem);
int iniciar_sesion(char *usuario_actual, Mensaje **mem_ref);

typedef struct
{
    char nombre[MAX_NOMBRE];
    char email[MAX_EMAIL];
    char telefono[MAX_TELEFONO];
    char usuario[MAX_USUARIO];
    char contrasena[MAX_CONTRASENA];
} Usuario;

typedef struct
{
    int ocupado;
    key_t clave_memoria;
    char id[MAX_USUARIO];
} RegistroCliente;

pthread_mutex_t registro_mutex = PTHREAD_MUTEX_INITIALIZER;

void cifrar(char *texto)
{
    while (*texto)
    {
        *texto ^= 0xAA;
        texto++;
    }
}

void imprimir_centrado(int fila, const char *texto)
{
    int columna = (COLS - strlen(texto)) / 2;
    mvprintw(fila, columna, "%s", texto);
}

void imprimir_borde()
{
    int y, x;
    getmaxyx(stdscr, y, x);
    for (int i = 0; i < x; i++)
    {
        mvaddch(0, i, '*');
        mvaddch(y - 1, i, '*');
    }
    for (int i = 0; i < y; i++)
    {
        mvaddch(i, 0, '*');
        mvaddch(i, x - 1, '*');
    }
}

void obtener_entrada(const char *etiqueta, char *entrada, int max_longitud, int ocultar)
{
    echo();
    curs_set(1);
    int y, x, ch, idx = 0;
    getmaxyx(stdscr, y, x);
    clear();
    imprimir_borde();
    imprimir_centrado(y / 2 - 1, etiqueta);
    move(y / 2, (x - max_longitud) / 2);

    if (ocultar)
        noecho();

    while ((ch = getch()) != '\n' && idx < max_longitud - 1)
    {
        if ((ch == KEY_BACKSPACE || ch == 127 || ch == 8) && idx > 0)
        {
            idx--;
            mvaddch(y / 2, (x - max_longitud) / 2 + idx, ' ');
            move(y / 2, (x - max_longitud) / 2 + idx);
        }
        else if (ch >= 32 && ch <= 126)
        {
            entrada[idx++] = ch;
            mvaddch(y / 2, (x - max_longitud) / 2 + idx - 1, ocultar ? '*' : ch);
        }
    }
    entrada[idx] = '\0';
    curs_set(0);
}

int menu_seleccion(const char *titulo, const char *opciones[], int num_opciones)
{
    int seleccion = 0;
    int tecla;

    do
    {
        clear();
        imprimir_borde();

        // Mostrar título
        imprimir_centrado(3, "---------------------------------");
        imprimir_centrado(4, titulo);
        imprimir_centrado(5, "---------------------------------");

        // Mostrar opciones
        for (int i = 0; i < num_opciones; i++)
        {
            if (i == seleccion)
            {
                attron(A_REVERSE);
                imprimir_centrado(7 + i, opciones[i]);
                attroff(A_REVERSE);
            }
            else
            {
                imprimir_centrado(7 + i, opciones[i]);
            }
        }

        // Mostrar instrucciones
        imprimir_centrado(7 + num_opciones + 2, "Use las flechas para navegar y ENTER para seleccionar");

        refresh();

        tecla = getch();

        switch (tecla)
        {
        case KEY_UP:
            if (seleccion > 0)
                seleccion--;
            break;
        case KEY_DOWN:
            if (seleccion < num_opciones - 1)
                seleccion++;
            break;
        case '\n':
            return seleccion + 1; // Retorna 1 para la primera opción, 2 para la segunda, etc.
        }
    } while (tecla != 27); // 27 es ESC

    return -1; // Si presiona ESC
}

int menu_principal()
{
    const char *opciones[] = {
        "1. Iniciar Sesión",
        "2. Registrarse",
        "3. Salir"};

    return menu_seleccion("|   Sistema de Cliente-Zapatería   |", opciones, 3);
}

int menu_usuario(const char *usuario, Mensaje *mem)
{
    const char *opciones[] = {
        "1. Ver catálogo",
        "2. Ver carrito de compra",
        "3. Realizar compra",
        "4. Ver perfil de usuario",
        "5. Salir"};

    int opcion = 0; // Inicializamos la variable opcion
    int conexion_activa = 1;

    do
    {
        clear();
        imprimir_borde();

        // Mostrar título
        imprimir_centrado(3, "---------------------------------");
        imprimir_centrado(4, "|   Menú Principal   |");
        imprimir_centrado(5, "---------------------------------");

        // Mostrar mensaje de bienvenida
        char saludo[100];
        snprintf(saludo, sizeof(saludo), "Bienvenid@: %s", usuario);
        imprimir_centrado(7, saludo);

        // Mostrar opciones
        for (int i = 0; i < 5; i++)
        {
            if (i == opcion)
            {
                attron(A_REVERSE);
                imprimir_centrado(9 + i, opciones[i]);
                attroff(A_REVERSE);
            }
            else
            {
                imprimir_centrado(9 + i, opciones[i]);
            }
        }

        // Mostrar instrucciones
        imprimir_centrado(16, "Use las flechas para navegar y ENTER para seleccionar");

        refresh();

        int tecla = getch();

        switch (tecla)
        {
        case KEY_UP:
            if (opcion > 0)
                opcion--;
            break;
        case KEY_DOWN:
            if (opcion < 4)
                opcion++;
            break;
        case '\n':
            switch (opcion)
            { // Usamos directamente opcion (0-based)
            case 0:
                strcpy(mem->mensaje, "Ver catálogo");
                mostrar_catalogo_por_seccion(usuario);
                break;

            case 1:
                strcpy(mem->mensaje, "Ver carrito");
                ver_carrito(usuario);
                break;
            case 2:
                strcpy(mem->mensaje, "Realizar compra");
                realizar_compra(usuario);
                break;
            case 3:
                strcpy(mem->mensaje, "Ver perfil");
                ver_perfil_usuario(usuario);
                break;
            case 4:
                strcpy(mem->mensaje, "salir");
                conexion_activa = 0;
                break;
            }
            break;
        case 27: // ESC
            conexion_activa = 0;
            break;
        }

        usleep(100000);
    } while (conexion_activa);

    return 0;
}

void mostrar_catalogo(const char *usuario)
{
    clear();

    typedef struct
    {
        char nombre[100];
        float precio;
        int stock_total;
        int stock_disponible;
    } Producto;

    Producto productos[100];
    int total_productos = 0;

    // Leer inventario
    FILE *archivo = fopen(ARCHIVO_PRODUCTOS, "r");
    if (!archivo)
    {
        imprimir_centrado(LINES / 2, "No se pudo abrir el catálogo.");
        getch();
        return;
    }

    char linea[256];
    while (fgets(linea, sizeof(linea), archivo))
    {
        char precio_str[20], nombre[100], cantidad_str[10];
        if (sscanf(linea, "%[^|]|%[^|]|%s", precio_str, nombre, cantidad_str) == 3)
        {
            strcpy(productos[total_productos].nombre, nombre);
            productos[total_productos].precio = atof(precio_str);
            productos[total_productos].stock_total = atoi(cantidad_str);
            productos[total_productos].stock_disponible = productos[total_productos].stock_total;
            total_productos++;
        }
    }
    fclose(archivo);

    // Leer carrito del usuario y descontar del stock disponible
    archivo = fopen(ARCHIVO_CARRITO, "r");
    if (archivo)
    {
        while (fgets(linea, sizeof(linea), archivo))
        {
            char user[MAX_USUARIO], precio_str[20], nombre[100], cantidad_str[10];
            if (sscanf(linea, "%[^|]|%[^|]|%[^|]|%s", user, precio_str, nombre, cantidad_str) == 4)
            {
                if (strcmp(user, usuario) == 0)
                {
                    for (int i = 0; i < total_productos; i++)
                    {
                        if (strcmp(productos[i].nombre, nombre) == 0)
                        {
                            productos[i].stock_disponible -= atoi(cantidad_str);
                            if (productos[i].stock_disponible < 0)
                                productos[i].stock_disponible = 0;
                            break;
                        }
                    }
                }
            }
        }
        fclose(archivo);
    }

    // Productos agregados durante esta sesión
    typedef struct
    {
        char nombre[100];
        int cantidad;
    } ProductoAgregado;

    ProductoAgregado carrito_sesion[100];
    int total_agregados = 0;

    int seleccion = 0;
    int tecla;

    do
    {
        clear();
        imprimir_borde();
        imprimir_centrado(1, "Catálogo de Productos (ENTER para agregar, ESC para salir)");

        int y = 3;
        for (int i = 0; i < total_productos; i++)
        {
            char buffer[256];
            if (i == seleccion)
                attron(A_REVERSE);

            if (productos[i].stock_disponible > 0)
                snprintf(buffer, sizeof(buffer), "%d. %s - $%.2f (Disponibles: %d)",
                         i + 1, productos[i].nombre, productos[i].precio, productos[i].stock_disponible);
            else
                snprintf(buffer, sizeof(buffer), "%d. %s - $%.2f (Sin stock)", i + 1, productos[i].nombre, productos[i].precio);

            mvprintw(y++, 4, "%s", buffer);
            if (i == seleccion)
                attroff(A_REVERSE);
        }

        int y_base = y + 1;
        mvprintw(y_base++, 4, "Productos agregados en esta sesión:");
        for (int i = 0; i < total_agregados; i++)
        {
            mvprintw(y_base++, 6, "- %s x%d", carrito_sesion[i].nombre, carrito_sesion[i].cantidad);
        }

        refresh();
        tecla = getch();

        switch (tecla)
        {
        case KEY_UP:
            if (seleccion > 0)
                seleccion--;
            break;
        case KEY_DOWN:
            if (seleccion < total_productos - 1)
                seleccion++;
            break;
        case '\n':
        {
            if (productos[seleccion].stock_disponible <= 0)
            {
                mvprintw(LINES - 3, 4, "No hay stock disponible.");
                getch();
                break;
            }

            echo();
            curs_set(1);
            mvprintw(LINES - 4, 4, "¿Cuántas unidades deseas agregar? (máximo %d): ", productos[seleccion].stock_disponible);
            char entrada[10];
            getnstr(entrada, 9);
            noecho();
            curs_set(0);
            int cantidad = atoi(entrada);

            if (cantidad <= 0 || cantidad > productos[seleccion].stock_disponible)
            {
                mvprintw(LINES - 3, 4, "Cantidad inválida.");
                getch();
                break;
            }

            FILE *carrito = fopen(ARCHIVO_CARRITO, "a");
            if (carrito)
            {
                for (int i = 0; i < cantidad; i++)
                {
                    fprintf(carrito, "%s|%.2f|%s|1\n", usuario, productos[seleccion].precio, productos[seleccion].nombre);
                }
                fclose(carrito);
            }

            productos[seleccion].stock_disponible -= cantidad;

            int existe = 0;
            for (int i = 0; i < total_agregados; i++)
            {
                if (strcmp(carrito_sesion[i].nombre, productos[seleccion].nombre) == 0)
                {
                    carrito_sesion[i].cantidad += cantidad;
                    existe = 1;
                    break;
                }
            }
            if (!existe)
            {
                strcpy(carrito_sesion[total_agregados].nombre, productos[seleccion].nombre);
                carrito_sesion[total_agregados].cantidad = cantidad;
                total_agregados++;
            }

            mvprintw(LINES - 3, 4, "Producto agregado.");
            getch();
            break;
        }
        case 27:
            return;
        }
    } while (1);
}

void ver_carrito(const char *usuario)
{
    typedef struct
    {
        char nombre[100];
        char talla[10];
        float precio_unitario;
        int cantidad;
    } ProductoAgrupado;

    ProductoAgrupado productos[100];
    int total_productos = 0;
    float total = 0.0;
    int seleccion = 0;
    int pos_producto = 0;

    // Leer carrito y agrupar productos
    FILE *archivo = fopen(ARCHIVO_CARRITO, "r");
    if (!archivo)
    {
        imprimir_centrado(LINES / 2, "No se pudo abrir el carrito.");
        getch();
        return;
    }

    char linea[256];
    while (fgets(linea, sizeof(linea), archivo))
    {
        char user[MAX_USUARIO], precio_str[20], nombre[100], talla[10], cantidad_str[10];
        if (sscanf(linea, "%[^|]|%[^|]|%[^|]|%[^|]|%s", user, precio_str, nombre, talla, cantidad_str) == 5)

        {
            if (strcmp(user, usuario) == 0)
            {
                int cantidad = atoi(cantidad_str);
                float precio = atof(precio_str);

                int encontrado = 0;
                for (int i = 0; i < total_productos; i++)
                {
                    if (strcmp(productos[i].nombre, nombre) == 0 &&
                        strcmp(productos[i].talla, talla) == 0)
                    {
                        productos[i].cantidad += cantidad;
                        encontrado = 1;
                        break;
                    }
                }

                if (!encontrado)
                {
                    strcpy(productos[total_productos].nombre, nombre);
                    strcpy(productos[total_productos].talla, talla);
                    productos[total_productos].precio_unitario = precio;
                    productos[total_productos].cantidad = cantidad;
                    total_productos++;
                }

                total += precio * cantidad;
            }
        }
    }
    fclose(archivo);

    // Navegación
    do
    {
        clear();
        imprimir_borde();
        imprimir_centrado(1, "Carrito de Compras:");

        int y = 3;
        if (total_productos == 0)
        {
            mvprintw(y++, 4, "Tu carrito está vacío.");
        }
        else
        {
            for (int i = 0; i < total_productos; i++)
            {
                float subtotal = productos[i].precio_unitario * productos[i].cantidad;
                if (seleccion == 0 && pos_producto == i)
                {
                    attron(A_REVERSE);
                    mvprintw(y++, 4, "> %d %s / Talla %s - $%.2f",
                             productos[i].cantidad,
                             productos[i].nombre,
                             productos[i].talla,
                             subtotal);
                    attroff(A_REVERSE);
                }
                else
                {
                    mvprintw(y++, 4, "  %d %s / Talla %s - $%.2f",
                             productos[i].cantidad,
                             productos[i].nombre,
                             productos[i].talla,
                             subtotal);
                }
            }
        }

        char total_str[64];
        snprintf(total_str, sizeof(total_str), "Total: $%.2f", total);
        imprimir_centrado(y + 1, total_str);

        // Opciones de acción
        y += 3;
        if (seleccion == 1)
        {
            attron(A_REVERSE);
            mvprintw(y, 4, "[ Borrar todo el carrito ]");
            attroff(A_REVERSE);
        }
        else
        {
            mvprintw(y, 4, "  Borrar todo el carrito  ");
        }

        if (seleccion == 2)
        {
            attron(A_REVERSE);
            mvprintw(y + 2, 4, "[ Regresar al menú ]");
            attroff(A_REVERSE);
        }
        else
        {
            mvprintw(y + 2, 4, "  Regresar al menú  ");
        }

        imprimir_centrado(y + 4, "Flechas: Navegar | ENTER: Eliminar unidades | ESC: Volver");
        refresh();

        int tecla = getch();
        switch (tecla)
        {
        case KEY_UP:
            if (seleccion == 0 && pos_producto > 0)
                pos_producto--;
            else if (seleccion > 0)
                seleccion--;
            break;
        case KEY_DOWN:
            if (seleccion == 0 && pos_producto < total_productos - 1)
                pos_producto++;
            else if (seleccion < 2 && total_productos > 0)
                seleccion++;
            break;

        case '\n':
            if (seleccion == 1)
            {
                FILE *original = fopen(ARCHIVO_CARRITO, "r");
                FILE *nuevo = fopen("carrito_temp.txt", "w");
                while (fgets(linea, sizeof(linea), original))
                {
                    char user[MAX_USUARIO];
                    sscanf(linea, "%[^|]|", user);
                    if (strcmp(user, usuario) != 0)
                    {
                        fputs(linea, nuevo);
                    }
                }
                fclose(original);
                fclose(nuevo);
                remove(ARCHIVO_CARRITO);
                rename("carrito_temp.txt", ARCHIVO_CARRITO);
                return;
            }
            else if (seleccion == 2)
            {
                return;
            }
            else if (total_productos > 0)
            {
                int max_eliminar = productos[pos_producto].cantidad;

                echo();
                curs_set(1);
                mvprintw(LINES - 3, 4, "¿Cuántas unidades deseas eliminar? (1 a %d): ", max_eliminar);
                char entrada[10];
                getnstr(entrada, 9);
                noecho();
                curs_set(0);

                int eliminar = atoi(entrada);
                if (eliminar <= 0 || eliminar > max_eliminar)
                {
                    mvprintw(LINES - 2, 4, "Cantidad inválida.");
                    getch();
                    break;
                }

                // Eliminar del archivo
                FILE *original = fopen(ARCHIVO_CARRITO, "r");
                FILE *nuevo = fopen("carrito_temp.txt", "w");

                int eliminadas = 0;
                while (fgets(linea, sizeof(linea), original))
                {
                    char user[MAX_USUARIO], precio_str[20], nombre[100], talla[10], cantidad_str[10];
                    if (sscanf(linea, "%[^|]|%[^|]|%[^|]|%[^|]|%s", user, precio_str, nombre, talla, cantidad_str) == 5)
                    {
                        if (strcmp(user, usuario) == 0 &&
                            strcmp(nombre, productos[pos_producto].nombre) == 0 &&
                            strcmp(talla, productos[pos_producto].talla) == 0 &&
                            eliminadas < eliminar)
                        {
                            eliminadas++;
                            continue;
                        }
                    }
                    fputs(linea, nuevo);
                }

                fclose(original);
                fclose(nuevo);
                remove(ARCHIVO_CARRITO);
                rename("carrito_temp.txt", ARCHIVO_CARRITO);

                productos[pos_producto].cantidad -= eliminar;
                total -= productos[pos_producto].precio_unitario * eliminar;

                if (productos[pos_producto].cantidad == 0)
                {
                    for (int i = pos_producto; i < total_productos - 1; i++)
                    {
                        productos[i] = productos[i + 1];
                    }
                    total_productos--;
                    if (pos_producto >= total_productos && pos_producto > 0)
                        pos_producto--;
                }
            }
            break;

        case 27:
            return;
        }
    } while (1);
}

void mostrar_catalogo_por_seccion(const char *usuario)
{
    typedef struct
    {
        char nombre[100];
        float precio;
        char seccion[30];
        char talla[10];
        int stock_total;
        int stock_disponible;
    } Producto;

    Producto productos[200];
    int total_productos = 0;

    FILE *archivo = fopen(ARCHIVO_PRODUCTOS, "r");
    if (!archivo)
    {
        imprimir_centrado(LINES / 2, "No se pudo abrir el catálogo.");
        getch();
        return;
    }

    char linea[256];
    while (fgets(linea, sizeof(linea), archivo))
    {
        char precio_str[20], nombre[100], seccion[30], talla[10], cantidad_str[10];
        char *token = strtok(linea, "|");
        if (!token)
            continue;
        strcpy(precio_str, token);

        token = strtok(NULL, "|");
        if (!token)
            continue;
        strcpy(nombre, token);

        token = strtok(NULL, "|");
        if (!token)
            continue;
        // Copiar y limpiar espacios/saltos de línea
        while (isspace(*token))
            token++;
        strncpy(seccion, token, sizeof(seccion));
        seccion[sizeof(seccion) - 1] = '\0';
        seccion[strcspn(seccion, "\r\n")] = '\0'; // corta en salto de línea

        token = strtok(NULL, "|");
        if (!token)
            continue;
        strcpy(talla, token);

        token = strtok(NULL, "|\n");
        if (!token)
            continue;
        strcpy(cantidad_str, token);

        // Guardar producto
        strcpy(productos[total_productos].nombre, nombre);
        strcpy(productos[total_productos].seccion, seccion);
        strcpy(productos[total_productos].talla, talla);
        productos[total_productos].precio = atof(precio_str);
        productos[total_productos].stock_total = atoi(cantidad_str);
        productos[total_productos].stock_disponible = productos[total_productos].stock_total;
        total_productos++;
    }
    fclose(archivo);

    // Descontar productos del carrito
    archivo = fopen(ARCHIVO_CARRITO, "r");
    if (archivo)
    {
        while (fgets(linea, sizeof(linea), archivo))
        {
            char user[MAX_USUARIO], precio_str[20], nombre[100], talla[10], cantidad_str[10];
            if (sscanf(linea, "%[^|]|%[^|]|%[^|]|%[^|]|%s", user, precio_str, nombre, talla, cantidad_str) == 5)
            {
                if (strcmp(user, usuario) == 0)
                {
                    for (int i = 0; i < total_productos; i++)
                    {
                        if (strcmp(productos[i].nombre, nombre) == 0 &&
                            strcmp(productos[i].talla, talla) == 0)
                        {
                            productos[i].stock_disponible -= atoi(cantidad_str);
                            if (productos[i].stock_disponible < 0)
                                productos[i].stock_disponible = 0;
                        }
                    }
                }
            }
        }
        fclose(archivo);
    }

    const char *secciones[] = {"Casual", "Deportivo", "Escolar"};
    int seleccion = menu_seleccion("Selecciona una sección:", secciones, 3);
    seleccion -= 1; // ✅ Ajuste necesario

    if (seleccion < 0 || seleccion >= 3)
        return;

    const char *seccion_actual = secciones[seleccion];

    Producto productos_seccion[200];
    int total_seccion = 0;
    for (int i = 0; i < total_productos; i++)
    {
        if (strcasecmp(productos[i].seccion, seccion_actual) == 0)
        {
            productos_seccion[total_seccion++] = productos[i];
        }
    }

    if (total_seccion == 0)
    {
        imprimir_centrado(LINES / 2, "No hay productos en esta sección.");
        getch();
        return;
    }

    int seleccion_producto = 0;
    int tecla;
    do
    {
        clear();
        imprimir_borde();
        imprimir_centrado(1, "Catálogo de productos:");

        int y = 3;
        for (int i = 0; i < total_seccion; i++)
        {
            if (i == seleccion_producto)
                attron(A_REVERSE);
            mvprintw(y++, 4, "%d. %s / Talla %s - $%.2f (Disponibles: %d)",
                     i + 1,
                     productos_seccion[i].nombre,
                     productos_seccion[i].talla,
                     productos_seccion[i].precio,
                     productos_seccion[i].stock_disponible);
            if (i == seleccion_producto)
                attroff(A_REVERSE);
        }

        mvprintw(y + 1, 4, "Flechas para navegar | ENTER para agregar | ESC para salir");
        refresh();

        tecla = getch();
        switch (tecla)
        {
        case KEY_UP:
            if (seleccion_producto > 0)
                seleccion_producto--;
            break;
        case KEY_DOWN:
            if (seleccion_producto < total_seccion - 1)
                seleccion_producto++;
            break;
        case '\n':
        {
            Producto *p = &productos_seccion[seleccion_producto];
            if (p->stock_disponible == 0)
            {
                mvprintw(LINES - 2, 4, "Sin stock disponible.");
                getch();
                break;
            }

            echo();
            mvprintw(LINES - 3, 4, "¿Cuántas unidades deseas agregar? ");
            char entrada[10];
            getnstr(entrada, 9);
            noecho();

            int cantidad = atoi(entrada);
            if (cantidad <= 0 || cantidad > p->stock_disponible)
            {
                mvprintw(LINES - 2, 4, "Cantidad inválida.");
                getch();
                break;
            }

            FILE *carrito = fopen(ARCHIVO_CARRITO, "a");
            if (carrito)
            {
                for (int i = 0; i < cantidad; i++)
                {
                    fprintf(carrito, "%s|%.2f|%s|%s|1\n", usuario, p->precio, p->nombre, p->talla);
                }
                fclose(carrito);
            }

            p->stock_disponible -= cantidad;
            mvprintw(LINES - 2, 4, "Producto agregado.");
            getch();
            break;
        }
        case 27:
            return;
        }
    } while (1);
}

typedef struct
{
    char nombre[100];
    int cantidad;
} ProductoContado;

void realizar_compra(const char *usuario)
{
    clear();
    FILE *archivo = fopen(ARCHIVO_CARRITO, "r");
    if (!archivo)
    {
        imprimir_centrado(LINES / 2, "No se pudo acceder al carrito.");
        getch();
        return;
    }

    typedef struct
    {
        char nombre[100];
        char talla[10];
        char seccion[30];
        float precio;
        int cantidad;
    } ProductoCompra;

    ProductoCompra productos[100];
    int num_productos = 0;
    char linea[256];
    float total = 0.0;
    int y = 3;
    char seccion[30]; // <- Falta en tu definición

    imprimir_borde();
    imprimir_centrado(1, "Resumen de su compra:");

    // Leer carrito
    while (fgets(linea, sizeof(linea), archivo))
    {
        char user[MAX_USUARIO], precio_str[20], nombre[100], talla[10], cantidad_str[10];
        if (sscanf(linea, "%[^|]|%[^|]|%[^|]|%[^|]|%s", user, precio_str, nombre, talla, cantidad_str) == 5)
        {
            if (strcmp(user, usuario) == 0)
            {
                float precio = atof(precio_str);
                int cantidad = atoi(cantidad_str);

                // Agrupar por nombre + talla
                int encontrado = 0;
                for (int i = 0; i < num_productos; i++)
                {
                    if (strcmp(productos[i].nombre, nombre) == 0 && strcmp(productos[i].talla, talla) == 0)
                    {
                        productos[i].cantidad += cantidad;
                        encontrado = 1;
                        break;
                    }
                }

                if (!encontrado)
                {
                    strcpy(productos[num_productos].nombre, nombre);
                    strcpy(productos[num_productos].talla, talla);
                    productos[num_productos].precio = precio;
                    productos[num_productos].cantidad = cantidad;
                    num_productos++;
                }

                total += precio * cantidad;
            }
        }
    }
    fclose(archivo);

    if (num_productos == 0)
    {
        imprimir_centrado(LINES / 2, "No hay productos en el carrito.");
        getch();
        return;
    }

    // Verificar stock
    FILE *inventario = fopen(ARCHIVO_PRODUCTOS, "r");
    if (!inventario)
    {
        imprimir_centrado(LINES / 2, "No se pudo abrir el inventario.");
        getch();
        return;
    }

    int error_stock = 0;
    ProductoCompra inventario_original[200];
    int total_inventario = 0;

    while (fgets(linea, sizeof(linea), inventario))
    {
        char precio_str[20], nombre[100], seccion_local[30], talla[10], stock_str[10];
        if (sscanf(linea, "%[^|]|%[^|]|%[^|]|%[^|]|%s", precio_str, nombre, seccion_local, talla, stock_str) == 5)
        {
            strcpy(inventario_original[total_inventario].nombre, nombre);
            strcpy(inventario_original[total_inventario].seccion, seccion_local);
            strcpy(inventario_original[total_inventario].talla, talla);
            inventario_original[total_inventario].precio = atof(precio_str);
            inventario_original[total_inventario].cantidad = atoi(stock_str);
            total_inventario++;
        }
    }

    fclose(inventario);

    for (int i = 0; i < num_productos; i++)
    {
        int encontrado = 0;
        for (int j = 0; j < total_inventario; j++)
        {
            if (strcmp(productos[i].nombre, inventario_original[j].nombre) == 0 &&
                strcmp(productos[i].talla, inventario_original[j].talla) == 0)
            {
                encontrado = 1;
                if (productos[i].cantidad > inventario_original[j].cantidad)
                {
                    mvprintw(y++, 4, "'%s T-%s': solicitados %d, disponibles %d",
                             productos[i].nombre, productos[i].talla,
                             productos[i].cantidad, inventario_original[j].cantidad);
                    error_stock = 1;
                }
                break;
            }
        }
        if (!encontrado)
        {
            mvprintw(y++, 4, "'%s T-%s' no se encontró en el inventario.", productos[i].nombre, productos[i].talla);
            error_stock = 1;
        }
    }

    if (error_stock)
    {
        mvprintw(y + 1, 4, "Compra cancelada. Modifique su carrito.");
        getch();
        return;
    }

    for (int i = 0; i < num_productos; i++)
    {
        mvprintw(y++, 4, "%d. %s T-%s x%d - $%.2f", i + 1,
                 productos[i].nombre, productos[i].talla, productos[i].cantidad,
                 productos[i].precio * productos[i].cantidad);
    }

    char total_str[64];
    snprintf(total_str, sizeof(total_str), "Total a pagar: $%.2f", total);
    mvprintw(y + 1, 4, "%s", total_str);

    char id[16];
    srand(time(NULL));
    snprintf(id, sizeof(id), "ID%05d", rand() % 100000);
    mvprintw(y + 3, 4, "Pase a pagar a la caja. Su ID es \"%s\"", id);

    FILE *compras = fopen(ARCHIVO_COMPRAS, "a");
    if (compras)
    {
        fprintf(compras, "%s|%.2f|%s\n", usuario, total, id);
        fclose(compras);
    }

    FILE *nuevo_inv = fopen("productos_temp.txt", "w");
    for (int j = 0; j < total_inventario; j++)
    {
        int cantidad_final = inventario_original[j].cantidad;
        for (int i = 0; i < num_productos; i++)
        {
            if (strcmp(productos[i].nombre, inventario_original[j].nombre) == 0 &&
                strcmp(productos[i].talla, inventario_original[j].talla) == 0)
            {
                cantidad_final -= productos[i].cantidad;
                if (cantidad_final < 0)
                    cantidad_final = 0;
                break;
            }
        }
        fprintf(nuevo_inv, "%.2f|%s|%s|%s|%d\n",
                inventario_original[j].precio,
                inventario_original[j].nombre,
                inventario_original[j].seccion, // Asegúrate de guardarla
                inventario_original[j].talla,
                cantidad_final);
    }
    fclose(nuevo_inv);
    remove(ARCHIVO_PRODUCTOS);
    rename("productos_temp.txt", ARCHIVO_PRODUCTOS);

    archivo = fopen(ARCHIVO_CARRITO, "r");
    FILE *temp_carrito = fopen("carrito_temp.txt", "w");
    while (fgets(linea, sizeof(linea), archivo))
    {
        char user[MAX_USUARIO];
        sscanf(linea, "%[^|]|", user);
        if (strcmp(user, usuario) != 0)
        {
            fputs(linea, temp_carrito);
        }
    }
    fclose(archivo);
    fclose(temp_carrito);
    remove(ARCHIVO_CARRITO);
    rename("carrito_temp.txt", ARCHIVO_CARRITO);

    mvprintw(y + 5, 4, "Compra registrada correctamente.");
    mvprintw(y + 6, 4, "Presione cualquier tecla para continuar...");
    getch();
}

void ver_perfil_usuario(const char *usuario)
{
    FILE *archivo = fopen(ARCHIVO_USUARIOS, "r");
    if (!archivo)
    {
        clear();
        imprimir_borde();
        imprimir_centrado(LINES / 2, "No se pudo abrir el archivo de usuarios.");
        getch();
        return;
    }

    Usuario usr;
    int encontrado = 0;
    char linea[256];
    while (fgets(linea, sizeof(linea), archivo))
    {
        Usuario temp;
        sscanf(linea, "%[^|]|%[^|]|%[^|]|%[^|]|%s",
               temp.nombre, temp.email, temp.telefono, temp.usuario, temp.contrasena);
        if (strcmp(temp.usuario, usuario) == 0)
        {
            usr = temp;
            encontrado = 1;
            break;
        }
    }
    fclose(archivo);

    if (!encontrado)
    {
        clear();
        imprimir_borde();
        imprimir_centrado(LINES / 2, "Usuario no encontrado.");
        getch();
        return;
    }

    int opcion = 0;
    int salir = 0;

    do
    {
        clear();
        imprimir_borde();
        imprimir_centrado(3, "---------------------------------");
        imprimir_centrado(4, "|   Perfil de Usuario   |");
        imprimir_centrado(5, "---------------------------------");

        int y = 7;
        mvprintw(y++, 4, "Nombre   : %s", usr.nombre);
        mvprintw(y++, 4, "Correo   : %s", usr.email);
        mvprintw(y++, 4, "Teléfono : %s", usr.telefono);
        mvprintw(y++, 4, "Usuario  : %s", usr.usuario);

        y += 2;

        if (opcion == 0)
        {
            attron(A_REVERSE);
            mvprintw(y, 10, "[ Actualizar información ]");
            attroff(A_REVERSE);
            mvprintw(y + 2, 10, "  Regresar al menú  ");
        }
        else
        {
            mvprintw(y, 10, "  Actualizar información  ");
            attron(A_REVERSE);
            mvprintw(y + 2, 10, "[ Regresar al menú ]");
            attroff(A_REVERSE);
        }

        imprimir_centrado(LINES - 3, "Use las flechas para navegar y ENTER para seleccionar");

        refresh();

        int tecla = getch();

        switch (tecla)
        {
        case KEY_UP:
            if (opcion > 0)
                opcion--;
            break;
        case KEY_DOWN:
            if (opcion < 1)
                opcion++;
            break;
        case '\n':
            if (opcion == 0)
            {
                char contrasena_actual[MAX_CONTRASENA];
                obtener_entrada("Ingrese su contraseña actual: ", contrasena_actual, MAX_CONTRASENA, 1);
                cifrar(contrasena_actual);  // ✅ Comparar cifrada

                if (strcmp(contrasena_actual, usr.contrasena) != 0)
                {
                    clear();
                    imprimir_borde();
                    imprimir_centrado(LINES / 2, "Contraseña incorrecta.");
                    getch();
                    break;
                }

                int cambiar_password = 0;
                char respuesta[3];

                obtener_entrada("¿Editar nombre? (s/n): ", respuesta, 3, 0);
                if (respuesta[0] == 's' || respuesta[0] == 'S')
                    obtener_entrada("Nuevo nombre: ", usr.nombre, MAX_NOMBRE, 0);

                obtener_entrada("¿Editar correo? (s/n): ", respuesta, 3, 0);
                if (respuesta[0] == 's' || respuesta[0] == 'S')
                    obtener_entrada("Nuevo correo: ", usr.email, MAX_EMAIL, 0);

                obtener_entrada("¿Editar teléfono? (s/n): ", respuesta, 3, 0);
                if (respuesta[0] == 's' || respuesta[0] == 'S')
                    obtener_entrada("Nuevo teléfono: ", usr.telefono, MAX_TELEFONO, 0);

                obtener_entrada("¿Editar contraseña? (s/n): ", respuesta, 3, 0);
                if (respuesta[0] == 's' || respuesta[0] == 'S')
                {
                    char nueva[MAX_CONTRASENA], confirmacion[MAX_CONTRASENA];
                    while (1)
                    {
                        obtener_entrada("Nueva contraseña: ", nueva, MAX_CONTRASENA, 1);
                        obtener_entrada("Confirme contraseña: ", confirmacion, MAX_CONTRASENA, 1);
                        if (strcmp(nueva, confirmacion) == 0)
                        {
                            cifrar(nueva);  // ✅ Cifrar nueva antes de guardar
                            strcpy(usr.contrasena, nueva);
                            cambiar_password = 1;
                            break;
                        }
                        clear();
                        imprimir_borde();
                        imprimir_centrado(LINES / 2, "No coinciden. Intente de nuevo.");
                        getch();
                    }
                }

                archivo = fopen(ARCHIVO_USUARIOS, "r");
                FILE *temp = fopen("usuarios_temp.txt", "w");
                while (fgets(linea, sizeof(linea), archivo))
                {
                    Usuario temp_usr;
                    sscanf(linea, "%[^|]|%[^|]|%[^|]|%[^|]|%s",
                           temp_usr.nombre, temp_usr.email, temp_usr.telefono, temp_usr.usuario, temp_usr.contrasena);
                    if (strcmp(temp_usr.usuario, usuario) == 0)
                    {
                        fprintf(temp, "%s|%s|%s|%s|%s\n",
                                usr.nombre, usr.email, usr.telefono, usr.usuario, usr.contrasena);
                    }
                    else
                    {
                        fputs(linea, temp);
                    }
                }
                fclose(archivo);
                fclose(temp);
                remove(ARCHIVO_USUARIOS);
                rename("usuarios_temp.txt", ARCHIVO_USUARIOS);

                clear();
                imprimir_borde();
                imprimir_centrado(LINES / 2, "Perfil actualizado correctamente.");
                getch();
            }
            else
            {
                salir = 1;
            }
            break;
        case 27:
            salir = 1;
            break;
        }

    } while (!salir);
}

void registrar_usuario()
{
    clear();
    imprimir_borde();
    imprimir_centrado(LINES / 2 - 2, "Registro de usuario");
    imprimir_centrado(LINES / 2, "Presione 'r' para regresar o cualquier otra tecla para continuar");
    int ch = getch();
    if (ch == 'r' || ch == 'R')
        return;

    Usuario nuevo_usuario;

    obtener_entrada("Ingrese su nombre completo: ", nuevo_usuario.nombre, MAX_NOMBRE, 0);
    obtener_entrada("Ingrese su correo electrónico: ", nuevo_usuario.email, MAX_EMAIL, 0);
    obtener_entrada("Ingrese su número de teléfono: ", nuevo_usuario.telefono, MAX_TELEFONO, 0);
    obtener_entrada("Ingrese nombre de usuario: ", nuevo_usuario.usuario, MAX_USUARIO, 0);

    // Validar que el usuario no exista
    FILE *archivo = fopen(ARCHIVO_USUARIOS, "r");
    if (archivo)
    {
        char linea[256];
        Usuario usr_tmp;
        while (fgets(linea, sizeof(linea), archivo))
        {
            if (sscanf(linea, "%[^|]|%[^|]|%[^|]|%[^|]|%s",
                       usr_tmp.nombre, usr_tmp.email, usr_tmp.telefono, usr_tmp.usuario, usr_tmp.contrasena) == 5)
            {
                if (strcmp(nuevo_usuario.usuario, usr_tmp.usuario) == 0)
                {
                    fclose(archivo);
                    clear();
                    imprimir_borde();
                    imprimir_centrado(LINES / 2, "El nombre de usuario ya está en uso.");
                    getch();
                    return;
                }
            }
        }
        fclose(archivo);
    }

    // Solicitar y confirmar contraseña
    while (1)
    {
        obtener_entrada("Ingrese contraseña: ", nuevo_usuario.contrasena, MAX_CONTRASENA, 1);
        char confirmacion[MAX_CONTRASENA];
        obtener_entrada("Confirme contraseña: ", confirmacion, MAX_CONTRASENA, 1);

        if (strcmp(nuevo_usuario.contrasena, confirmacion) == 0)
            break;

        clear();
        imprimir_borde();
        imprimir_centrado(LINES / 2, "Las contraseñas no coinciden. Intente de nuevo.");
        getch();
    }

    // Cifrar la contraseña antes de guardar
    cifrar(nuevo_usuario.contrasena);

    archivo = fopen(ARCHIVO_USUARIOS, "a");
    if (archivo)
    {
        fprintf(archivo, "%s|%s|%s|%s|%s\n",
                nuevo_usuario.nombre,
                nuevo_usuario.email,
                nuevo_usuario.telefono,
                nuevo_usuario.usuario,
                nuevo_usuario.contrasena);
        fclose(archivo);
    }

    // Guardar registro de actividad
    FILE *registro = fopen(ARCHIVO_REGISTROS, "a");
    if (registro)
    {
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        fprintf(registro, "%s registrado el %02d-%02d-%04d a las %02d:%02d:%02d\n",
                nuevo_usuario.usuario, tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900,
                tm.tm_hour, tm.tm_min, tm.tm_sec);
        fclose(registro);
    }

    clear();
    imprimir_borde();
    imprimir_centrado(LINES / 2, "Usuario registrado exitosamente.");
    getch();
}

int iniciar_sesion(char *usuario_actual, Mensaje **mem_ref)
{
    clear();
    imprimir_borde();
    imprimir_centrado(LINES / 2 - 2, "Inicio de sesión");
    imprimir_centrado(LINES / 2, "Presione 'r' para regresar o cualquier otra tecla para continuar");
    int ch_previo = getch();
    if (ch_previo == 'r' || ch_previo == 'R')
        return 0;

    char usuario[MAX_USUARIO], contrasena[MAX_CONTRASENA], cifrada[MAX_CONTRASENA];
    int intentos_login = 3;

    while (intentos_login > 0)
    {
        clear();
        imprimir_borde();
        imprimir_centrado(LINES / 2 - 2, "INICIAR SESIÓN");
        obtener_entrada("Nombre de usuario: ", usuario, MAX_USUARIO, 0);
        obtener_entrada("Contraseña: ", contrasena, MAX_CONTRASENA, 1);

        // Encriptar para comparación
        strcpy(cifrada, contrasena);
        cifrar(cifrada);

        FILE *archivo = fopen(ARCHIVO_USUARIOS, "r");
        if (!archivo)
        {
            clear();
            imprimir_borde();
            imprimir_centrado(LINES / 2, "Error al acceder a la base de usuarios.");
            getch();
            intentos_login--;
            continue;
        }

        int encontrado = 0;
        char linea[256];
        while (fgets(linea, sizeof(linea), archivo))
        {
            char nombre_guardado[MAX_NOMBRE];
            char email_guardado[MAX_EMAIL];
            char telefono_guardado[MAX_TELEFONO];
            char usuario_guardado[MAX_USUARIO];
            char contrasena_guardada[MAX_CONTRASENA];

            int campos = sscanf(linea, "%[^|]|%[^|]|%[^|]|%[^|]|%s",
                                nombre_guardado, email_guardado, telefono_guardado,
                                usuario_guardado, contrasena_guardada);

            if (campos >= 5 && strcmp(usuario, usuario_guardado) == 0 &&
                strcmp(cifrada, contrasena_guardada) == 0)
            {
                encontrado = 1;
                break;
            }
        }
        fclose(archivo);

        if (!encontrado)
        {
            intentos_login--;
            clear();
            imprimir_borde();
            imprimir_centrado(LINES / 2 - 1, "Usuario o contraseña incorrectos.");
            imprimir_centrado(LINES / 2 + 1, intentos_login > 0 ? "Intente nuevamente." : "Sin intentos restantes.");
            getch();
            continue;
        }

        // Registrar en log
        FILE *registro = fopen(ARCHIVO_REGISTROS, "a");
        if (registro)
        {
            time_t t = time(NULL);
            struct tm tm = *localtime(&t);
            fprintf(registro, "%s inició sesión el %02d-%02d-%04d a las %02d:%02d:%02d\n",
                    usuario, tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900,
                    tm.tm_hour, tm.tm_min, tm.tm_sec);
            fclose(registro);
        }

        clear();
        imprimir_borde();
        imprimir_centrado(LINES / 2, "Conectando con el servidor...");
        refresh();

        // Paso 1: Intentar conectar directamente con la clave fija
        int intentos_conexion = 50;
        int shmid = -1;

        while (intentos_conexion-- > 0)
        {
            shmid = shmget(REGISTRO_KEY, 0, 0666); // Intento de conexión
            if (shmid >= 0)
                break;

            usleep(100000); // Esperar 100ms entre intentos
        }

        if (shmid < 0)
        {
            clear();
            imprimir_borde();
            imprimir_centrado(LINES / 2, "Error: Servidor no disponible");
            imprimir_centrado(LINES / 2 + 1, "Por favor inicie el servidor primero");
            getch();
            return 0;
        }
        pthread_mutex_lock(&registro_mutex);

        RegistroCliente *registro_clientes = (RegistroCliente *)shmat(shmid, NULL, 0);
        if (registro_clientes == (void *)-1)
        {
            pthread_mutex_unlock(&registro_mutex);
            clear();
            imprimir_borde();
            imprimir_centrado(LINES / 2, "Error al conectar con el servidor");
            getch();
            return 0;
        }

        // Buscar posición libre
        int pos = -1;
        for (int i = 0; i < MAX_CLIENTES; i++)
        {
            if (registro_clientes[i].ocupado == 0)
            {
                pos = i;
                break;
            }
        }

        if (pos == -1)
        {
            clear();
            imprimir_borde();
            imprimir_centrado(LINES / 2, "Servidor lleno. Intente más tarde.");
            getch();
            shmdt(registro_clientes);
            pthread_mutex_unlock(&registro_mutex);
            return 0;
        }

        // Crear memoria compartida para este cliente
        key_t clave_cliente = ftok("/tmp", pos + 65); // Usamos /tmp que siempre existe
        if (clave_cliente == -1)
        {
            clear();
            imprimir_borde();
            imprimir_centrado(LINES / 2, "Error al generar clave IPC.");
            shmdt(registro_clientes);
            pthread_mutex_unlock(&registro_mutex);
            getch();
            return 0;
        }

        int shmid_cliente = shmget(clave_cliente, sizeof(Mensaje), IPC_CREAT | 0666);
        if (shmid_cliente < 0)
        {
            clear();
            imprimir_borde();
            imprimir_centrado(LINES / 2, "Error al crear segmento de memoria compartida.");
            shmdt(registro_clientes);
            pthread_mutex_unlock(&registro_mutex);
            getch();
            return 0;
        }

        Mensaje *mem_cliente = (Mensaje *)shmat(shmid_cliente, NULL, 0);
        if (mem_cliente == (void *)-1)
        {
            clear();
            imprimir_borde();
            imprimir_centrado(LINES / 2, "Error al acceder a memoria compartida.");
            shmdt(registro_clientes);
            shmctl(shmid_cliente, IPC_RMID, NULL);
            pthread_mutex_unlock(&registro_mutex);
            getch();
            return 0;
        }

        // Configurar memoria compartida
        memset(mem_cliente, 0, sizeof(Mensaje)); // Limpiar toda la estructura
        strcpy(mem_cliente->usuario, usuario);

        // Registrar cliente
        registro_clientes[pos].ocupado = 1;
        registro_clientes[pos].clave_memoria = clave_cliente;
        strcpy(registro_clientes[pos].id, usuario);

        // Despertar al servidor con el semáforo
        int semid = semget(12345, 1, 0666); // 12345 es SEM_KEY
        if (semid >= 0)
        {
            struct sembuf operacion = {0, 1, 0}; // Incrementar semáforo
            if (semop(semid, &operacion, 1) == -1)
            {
                perror("Error al despertar al servidor");
            }
        }

        // Forzar sincronización de memoria
        __sync_synchronize();

        shmdt(registro_clientes);
        pthread_mutex_unlock(&registro_mutex);

        strcpy(usuario_actual, usuario);
        *mem_ref = mem_cliente;

        // Preparar mensaje antes de notificar
        strcpy(mem_cliente->mensaje, "conectado");
        strcpy(mem_cliente->usuario, usuario);

        // Forzar sincronización del mensaje
        __sync_synchronize();

        // Esperar confirmación del servidor con timeout
        clear();
        imprimir_borde();
        imprimir_centrado(LINES / 2, "Esperando confirmación del servidor...");
        refresh();

        int espera = 50; // 5 segundos
        while (espera-- > 0)
        {
            if (strcmp(mem_cliente->mensaje, "conectado_ok") == 0)
            {
                break;
            }
            usleep(100000);
        }

        if (espera <= 0)
        {
            clear();
            imprimir_borde();
            imprimir_centrado(LINES / 2, "Error: Tiempo de espera agotado. Servidor no responde.");

            // Limpieza de recursos
            pthread_mutex_lock(&registro_mutex);
            shmid = shmget(REGISTRO_KEY, sizeof(RegistroCliente) * MAX_CLIENTES, 0666);
            if (shmid >= 0)
            {
                RegistroCliente *reg = (RegistroCliente *)shmat(shmid, NULL, 0);
                if (reg != (void *)-1)
                {
                    reg[pos].ocupado = 0; // Liberar la posición
                    shmdt(reg);
                }
            }
            pthread_mutex_unlock(&registro_mutex);

            shmdt(mem_cliente);
            shmctl(shmid_cliente, IPC_RMID, NULL);
            getch();
            return 0;
        }

        clear();
        imprimir_borde();
        imprimir_centrado(LINES / 2, "Inicio de sesión exitoso!");
        refresh();
        napms(1000);

        return 1;
    }

    return 0;
}

int main()
{
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    start_color();

    // Inicializar colores
    init_pair(1, COLOR_WHITE, COLOR_BLUE);
    init_pair(2, COLOR_BLACK, COLOR_WHITE);
    init_pair(3, COLOR_RED, COLOR_BLACK);

    bkgd(COLOR_PAIR(1));

    Mensaje *mem = NULL;
    char usuario_actual[MAX_USUARIO] = {0};

    int opcion;
    do
    {
        opcion = menu_principal();
        switch (opcion)
        {
        case 1:
            if (iniciar_sesion(usuario_actual, &mem))
            {
                menu_usuario(usuario_actual, mem);
                // Al salir, notificar desconexión
                if (mem)
                {
                    strcpy(mem->mensaje, "desconectado");
                    shmdt(mem);
                    mem = NULL;
                }
            }
            break;
        case 2:
            registrar_usuario();
            break;
        case 3:
            if (mem)
            {
                strcpy(mem->mensaje, "desconectado");
                shmdt(mem);
            }
            endwin();
            return 0;
        default:
            break;
        }
    } while (1);

    endwin();
    return 0;
}