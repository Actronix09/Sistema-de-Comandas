#include "ui.h"
#include "usuario.h"
#include "sesion.h"

int main()
{
    // Inicializar sistema
    ui_init();
    usuario_init();
    usuario_cargar();

    // Opciones del menú
    char *opciones[] = {
        "Iniciar Sesion",
        "Crear Usuario",
        "Recuperar Contrasena",
        "Salir"
    };

    int salir = 0;
    
    while(!salir)
    {
        int opcion = ui_show_menu("Sistema de Comandas", opciones, 4);

        switch(opcion)
        {
            case 0: // Iniciar Sesión
                sesion_iniciar();
                break;
                
            case 1: // Crear Usuario
                sesion_crear_usuario();
                break;
                
            case 2: // Recuperar Contraseña
                sesion_recuperar_password();
                break;
                
            case 3: // Salir
                salir = 1;
                break;
        }
    }

    // Finalizar sistema
    ui_cleanup();
    
    return 0;
}