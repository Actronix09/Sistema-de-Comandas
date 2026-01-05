#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include "ui.h"
#include "usuario.h"
#include "protocolo.h"
#include "interfaces.h"
#include "recuperar_password.h"

#define PERMISOS 0644
#define MAX_CLIENTES 10
#define MAX_INTENTOS_CONEXION 30

// Datos para cada cliente conectado
typedef struct {
    int cliente_conectado;
    int peticion_lista;
    int respuesta_lista;
    Peticion peticion;
    Respuesta respuesta;
    int cliente_id;
    int hilo_asignado;
} DatosCliente;

typedef struct {
    int servidor_activo;
    int num_clientes_activos;
    DatosCliente clientes[MAX_CLIENTES];
} DatosCompartidos;

// Variables globales para comunicación con servidor
DatosCompartidos *datos_servidor = NULL;
int semid_servidor = -1;
int mi_slot = -1;

// Funciones de semáforo
void down(int semid) {
    struct sembuf op_p[] = {{0, -1, 0}};
    semop(semid, op_p, 1);
}

void up(int semid) {
    struct sembuf op_v[] = {{0, +1, 0}};
    semop(semid, op_v, 1);
}

// Verificar si el semáforo está disponible sin bloquearse
int try_down(int semid) {
    struct sembuf op_p[] = {{0, -1, IPC_NOWAIT}};
    return semop(semid, op_p, 1);
}

// Esperar a que el servidor esté listo
int esperar_servidor() {
    key_t llave_sem_estado;
    int semid_estado;
    int intentos = 0;
    
    // Obtener la llave del semáforo de estado
    llave_sem_estado = ftok("servidor_estado_sem", 'E');
    if (llave_sem_estado == -1) {
        return -1;
    }
    
    printf("Buscando servidor");
    fflush(stdout);
    
    // Intentar obtener el semáforo de estado
    while (intentos < MAX_INTENTOS_CONEXION) {
        semid_estado = semget(llave_sem_estado, 1, PERMISOS);
        
        if (semid_estado != -1) {
            // El semáforo existe, intentar hacer down
            if (try_down(semid_estado) == 0) {
                // Éxito: el servidor está listo
                up(semid_estado); // Devolver el semáforo para otros clientes
                printf(" >\n");
                printf("Servidor encontrado y listo!\n");
                return 0;
            }
        }
        
        // El servidor no está listo, esperar y reintentar
        printf(".");
        fflush(stdout);
        sleep(1);
        intentos++;
    }
    
    printf(" X\n");
    printf("Timeout: El servidor no respondio en %d segundos\n", MAX_INTENTOS_CONEXION);
    return -1;
}

// Conectar al servidor
int conectar_servidor() {
    key_t llave_mem, llave_sem;
    int shmid;
    
    // Primero esperar a que el servidor esté listo
    if (esperar_servidor() != 0) {
        return -1;
    }
    
    printf("Conectando a memoria compartida...\n");
    
    llave_mem = ftok("servidor_usuarios_mem", 'U');
    llave_sem = ftok("servidor_usuarios_sem", 'V');
    
    if (llave_mem == -1 || llave_sem == -1) {
        printf("Error: No se pudieron crear las llaves\n");
        return -1;
    }
    
    shmid = shmget(llave_mem, sizeof(DatosCompartidos), PERMISOS);
    if (shmid == -1) {
        printf("Error: No se pudo acceder a la memoria compartida\n");
        return -1;
    }
    
    datos_servidor = (DatosCompartidos *)shmat(shmid, 0, 0);
    if (datos_servidor == (void *)-1) {
        printf("Error: No se pudo adjuntar la memoria compartida\n");
        return -1;
    }
    
    semid_servidor = semget(llave_sem, 1, PERMISOS);
    if (semid_servidor == -1) {
        printf("Error: No se pudo acceder al semaforo\n");
        shmdt(datos_servidor);
        return -1;
    }
    
    // Verificar servidor activo
    down(semid_servidor);
    if (datos_servidor->servidor_activo == 0) {
        printf("Error: El servidor no esta activo\n");
        up(semid_servidor);
        shmdt(datos_servidor);
        return -1;
    }
    up(semid_servidor);
    
    printf("Buscando slot disponible...\n");
    
    // Buscar slot disponible
    down(semid_servidor);
    for (int i = 0; i < MAX_CLIENTES; i++) {
        if (datos_servidor->clientes[i].cliente_conectado == 0) {
            mi_slot = i;
            datos_servidor->clientes[i].cliente_conectado = 1;
            datos_servidor->clientes[i].peticion_lista = 0;
            datos_servidor->clientes[i].respuesta_lista = 0;
            datos_servidor->clientes[i].cliente_id = getpid();
            datos_servidor->clientes[i].hilo_asignado = 0;
            break;
        }
    }
    up(semid_servidor);
    
    if (mi_slot == -1) {
        printf("Error: No hay slots disponibles (servidor lleno)\n");
        shmdt(datos_servidor);
        return -1;
    }
    
    printf("Slot asignado: %d\n", mi_slot);
    return 0;
}

// Enviar petición y esperar respuesta
int enviar_peticion(Peticion *pet, Respuesta *resp) {
    if (mi_slot == -1) return -1;
    
    // Enviar petición
    down(semid_servidor);
    datos_servidor->clientes[mi_slot].peticion = *pet;
    datos_servidor->clientes[mi_slot].peticion_lista = 1;
    up(semid_servidor);
    
    // Esperar respuesta
    int timeout = 0;
    while (timeout < 100) {
        down(semid_servidor);
        if (datos_servidor->clientes[mi_slot].respuesta_lista == 1) {
            *resp = datos_servidor->clientes[mi_slot].respuesta;
            datos_servidor->clientes[mi_slot].respuesta_lista = 0;
            up(semid_servidor);
            return 0;
        }
        up(semid_servidor);
        usleep(100000);
        timeout++;
    }
    
    return -1;
}

// Desconectar del servidor
void desconectar_servidor() {
    if (mi_slot != -1 && datos_servidor != NULL) {
        down(semid_servidor);
        datos_servidor->clientes[mi_slot].cliente_conectado = 0;
        datos_servidor->clientes[mi_slot].hilo_asignado = 0;
        up(semid_servidor);
        shmdt(datos_servidor);
    }
}

// Iniciar sesión
int iniciar_sesion(USUARIO *usuario_logueado) {
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
    
    ui_read_input(6, 14, user, MAX_CHAIN_SIZE, 0);
    if(strlen(user) == 0) return -1;
    
    ui_read_input(8, 18, pass, MAX_CHAIN_SIZE, 1);
    if(strlen(pass) == 0) return -1;
    
    // Preparar petición
    Peticion pet;
    Respuesta resp;
    memset(&pet, 0, sizeof(Peticion));
    memset(&resp, 0, sizeof(Respuesta));
    
    pet.operacion = OP_LOGIN;
    strncpy(pet.user, user, MAX_CHAIN_SIZE-1);
    strncpy(pet.pass, pass, MAX_CHAIN_SIZE-1);
    
    // Enviar al servidor
    if (enviar_peticion(&pet, &resp) != 0) {
        ui_show_error("Error de comunicacion con el servidor");
        return -1;
    }
    
    if (resp.codigo == RESP_OK) {
        // Llenar datos del usuario
        strncpy(usuario_logueado->user, user, MAX_CHAIN_SIZE-1);
        strncpy(usuario_logueado->name, resp.nombre, MAX_CHAIN_SIZE-1);
        usuario_logueado->tipo = resp.tipo_usuario;
        
        ui_show_success(resp.mensaje);
        return 0;
    } else {
        ui_show_error(resp.mensaje);
        return -1;
    }
}

// Crear usuario
void crear_usuario() {
    int intentar_nuevamente = 1;
    
    while(intentar_nuevamente) {
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
        mvprintw(16, 5, "Tipo:");
        attroff(COLOR_PAIR(10) | A_BOLD);
        
        ui_print_footer("Presione ESC para cancelar");
        refresh();
        
        // Leer nombre
        ui_read_input(6, 22, name, MAX_CHAIN_SIZE, 0);
        if(strlen(name) == 0) return;
        
        // Leer usuario
        ui_read_input(8, 14, user, MAX_CHAIN_SIZE, 0);
        if(strlen(user) == 0) return;
        
        // Verificar si existe
        Peticion pet_ver;
        Respuesta resp_ver;
        memset(&pet_ver, 0, sizeof(Peticion));
        memset(&resp_ver, 0, sizeof(Respuesta));
        
        pet_ver.operacion = OP_VERIFICAR_USUARIO;
        strncpy(pet_ver.user, user, MAX_CHAIN_SIZE-1);
        
        if (enviar_peticion(&pet_ver, &resp_ver) == 0 && resp_ver.codigo == RESP_USUARIO_EXISTE) {
            strcpy(mensaje_error, "El usuario ya existe");
            error_encontrado = 1;
        }
        
        if(!error_encontrado) {
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
            refresh();

            // Leer contraseña
            ui_read_input(10, 17, pass, MAX_CHAIN_SIZE, 1);
            if(strlen(pass) == 0) return;

            if(validar_password(pass) != 0) {
                strcpy(mensaje_error, "La contrasena es invalida");

                ui_clear_screen();
                
                ui_print_colored(LINES/2 - 3, (COLS-strlen(mensaje_error))/2, mensaje_error, 3);

                // Mensaje de reintento
                mvprintw(LINES/2 + 1, (COLS-strlen("¿Intentar otra vez?"))/2, "¿Intentar otra vez?");

                int centro_x = (COLS - 32) / 2; // Centrado para dos botones de 12 caracteres con separación de 20
                int boton_y = LINES/2 + 3;
                int boton_ancho = 12;
                int boton_alto = 3;

                int seleccion = 0; // 0 para Sí, 1 para No
                int salir_seleccion = 0;

                while(!salir_seleccion) {
                    // Dibujar botones usando la función centralizada
                    dibujar_botones_si_no(centro_x, boton_y, seleccion, boton_ancho, boton_alto);

                    ui_print_footer("Use flechas para navegar y Enter para seleccionar");
                    refresh();

                    int tecla = getch();
                    if(tecla == KEY_LEFT || tecla == KEY_RIGHT) {
                        seleccion = 1 - seleccion; // Alternar entre 0 y 1
                    } else if(tecla == 10) { // Enter
                        if(seleccion == 0) { // Sí
                            salir_seleccion = 1;
                            // Volver al inicio del bucle para intentar de nuevo
                        } else { // No
                            intentar_nuevamente = 0;
                            salir_seleccion = 1;
                        }
                    } else if(tecla == 27) { // ESC
                        intentar_nuevamente = 0;
                        salir_seleccion = 1;
                    }
                }
                continue; // Volver al inicio del bucle para intentar de nuevo
            }
        }

        if(!error_encontrado) {
            // Limpiar requisitos anteriores y mostrar requisitos de email
            for(int i = 1; i <= 9; i++) {
                move(i, COLS - 29);
                clrtoeol();
            }

            attron(COLOR_PAIR(6) | A_BOLD);
            mvprintw(1, COLS - 29, "+==========================+");
            mvprintw(2, COLS - 29, "|   REQUISITOS DE EMAIL    |");
            mvprintw(3, COLS - 29, "+==========================+");
            attroff(COLOR_PAIR(6) | A_BOLD);
            attron(COLOR_PAIR(6));
            mvprintw(4, COLS - 29, "| > 1 arroba (@)           |");
            mvprintw(5, COLS - 29, "| > Al menos 1 punto (.)   |");
            mvprintw(6, COLS - 29, "|                          |");
            mvprintw(7, COLS - 29, "| Ejemplo:                 |");
            mvprintw(8, COLS - 29, "| usuario@correo.com       |");
            attroff(COLOR_PAIR(6));
            attron(COLOR_PAIR(6) | A_BOLD);
            mvprintw(9, COLS - 29, "+==========================+");
            attroff(COLOR_PAIR(6) | A_BOLD);
            refresh();

            // Leer email
            ui_read_input(12, 12, mail, MAX_CHAIN_SIZE, 0);
            if(strlen(mail) == 0) return;

            if(validar_email(mail) != 0) {
                strcpy(mensaje_error, "El correo es invalido");

                ui_clear_screen();
                
                ui_print_colored(LINES/2 - 1, (COLS-strlen(mensaje_error))/2, mensaje_error, 3);

                // Mensaje de reintento
                mvprintw(LINES/2 + 1, (COLS-strlen("¿Intentar otra vez?"))/2, "¿Intentar otra vez?");

                int centro_x = (COLS - 32) / 2; // Centrado para dos botones de 12 caracteres con separación de 20
                int boton_y = LINES/2 + 3;
                int boton_ancho = 12;
                int boton_alto = 3;

                int seleccion = 0; // 0 para Sí, 1 para No
                int salir_seleccion = 0;

                while(!salir_seleccion) {
                    // Dibujar botones usando la función centralizada
                    dibujar_botones_si_no(centro_x, boton_y, seleccion, boton_ancho, boton_alto);

                    ui_print_footer("Use flechas para navegar y Enter para seleccionar");
                    refresh();

                    int tecla = getch();
                    if(tecla == KEY_LEFT || tecla == KEY_RIGHT) {
                        seleccion = 1 - seleccion; // Alternar entre 0 y 1
                    } else if(tecla == 10) { // Enter
                        if(seleccion == 0) { // Sí
                            salir_seleccion = 1;
                            // Volver al inicio del bucle para intentar de nuevo
                        } else { // No
                            intentar_nuevamente = 0;
                            salir_seleccion = 1;
                        }
                    } else if(tecla == 27) { // ESC
                        intentar_nuevamente = 0;
                        salir_seleccion = 1;
                    }
                }
                continue; // Volver al inicio del bucle para intentar de nuevo
            }
        }

        if(!error_encontrado) {
            // Limpiar requisitos anteriores y mostrar requisitos de teléfono
            for(int i = 1; i <= 9; i++) {
                move(i, COLS - 29);
                clrtoeol();
            }

            attron(COLOR_PAIR(6) | A_BOLD);
            mvprintw(1, COLS - 29, "+==========================+");
            mvprintw(2, COLS - 29, "|  REQUISITOS DE TELEFONO  |");
            mvprintw(3, COLS - 29, "+==========================+");
            attroff(COLOR_PAIR(6) | A_BOLD);
            attron(COLOR_PAIR(6));
            mvprintw(4, COLS - 29, "| > Minimo 8 digitos       |");
            mvprintw(5, COLS - 29, "| > Solo numeros (0-9)     |");
            mvprintw(6, COLS - 29, "|                          |");
            mvprintw(7, COLS - 29, "| Ejemplo:                 |");
            mvprintw(8, COLS - 29, "| 5512345678               |");
            attroff(COLOR_PAIR(6));
            attron(COLOR_PAIR(6) | A_BOLD);
            mvprintw(9, COLS - 29, "+==========================+");
            attroff(COLOR_PAIR(6) | A_BOLD);
            refresh();

            // Leer teléfono
            ui_read_input(14, 15, telf, MAX_CHAIN_SIZE, 0);
            if(strlen(telf) == 0) return;

            if(validar_telefono(telf) != 0) {
                strcpy(mensaje_error, "El telefono es invalido");

                ui_clear_screen();
                
                ui_print_colored(LINES/2 - 1, (COLS-strlen(mensaje_error))/2, mensaje_error, 3);

                // Mensaje de reintento
                mvprintw(LINES/2 + 2, (COLS-strlen("¿Intentar otra vez?"))/2, "¿Intentar otra vez?");

                int centro_x = (COLS - 32) / 2; // Centrado para dos botones de 12 caracteres con separación de 20
                int boton_y = LINES/2 + 4;
                int boton_ancho = 12;
                int boton_alto = 3;

                int seleccion = 0; // 0 para Sí, 1 para No
                int salir_seleccion = 0;

                while(!salir_seleccion) {
                    // Dibujar botones usando la función centralizada
                    dibujar_botones_si_no(centro_x, boton_y, seleccion, boton_ancho, boton_alto);

                    ui_print_footer("Use flechas para navegar y Enter para seleccionar");
                    refresh();

                    int tecla = getch();
                    if(tecla == KEY_LEFT || tecla == KEY_RIGHT) {
                        seleccion = 1 - seleccion; // Alternar entre 0 y 1
                    } else if(tecla == 10) { // Enter
                        if(seleccion == 0) { // Sí
                            salir_seleccion = 1;
                            // Volver al inicio del bucle para intentar de nuevo
                        } else { // No
                            intentar_nuevamente = 0;
                            salir_seleccion = 1;
                        }
                    } else if(tecla == 27) { // ESC
                        intentar_nuevamente = 0;
                        salir_seleccion = 1;
                    }
                }
                continue; // Volver al inicio del bucle para intentar de nuevo
            }
        }
        
        if(!error_encontrado) {
            // Limpiar requisitos anteriores y mostrar requisitos de tipo
            for(int i = 1; i <= 9; i++) {
                move(i, COLS - 29);
                clrtoeol();
            }

            attron(COLOR_PAIR(6) | A_BOLD);
            mvprintw(1, COLS - 29, "+==========================+");
            mvprintw(2, COLS - 29, "|     TIPO DE USUARIO      |");
            mvprintw(3, COLS - 29, "+==========================+");
            attroff(COLOR_PAIR(6) | A_BOLD);
            attron(COLOR_PAIR(6));
            mvprintw(4, COLS - 29, "| > M: Mesero              |");
            mvprintw(5, COLS - 29, "| > C: Cocina              |");
            mvprintw(6, COLS - 29, "|                          |");
            mvprintw(7, COLS - 29, "| Ingrese M o C            |");
            mvprintw(8, COLS - 29, "|                          |");
            attroff(COLOR_PAIR(6));
            attron(COLOR_PAIR(6) | A_BOLD);
            mvprintw(9, COLS - 29, "+==========================+");
            attroff(COLOR_PAIR(6) | A_BOLD);
            refresh();

            // Leer tipo
            char tipo_char;
            tipo_char = mvwgetch(stdscr, 16,11);
            tipo = (tipo_char == 'C' || tipo_char == 'c') ? 1 : 0;

            // Preparar petición
            Peticion pet;
            Respuesta resp;
            memset(&pet, 0, sizeof(Peticion));
            memset(&resp, 0, sizeof(Respuesta));

            pet.operacion = OP_CREAR_USUARIO;
            strncpy(pet.name, name, MAX_CHAIN_SIZE-1);
            strncpy(pet.user, user, MAX_CHAIN_SIZE-1);
            strncpy(pet.pass, pass, MAX_CHAIN_SIZE-1);
            strncpy(pet.mail, mail, MAX_CHAIN_SIZE-1);
            strncpy(pet.telf, telf, MAX_CHAIN_SIZE-1);
            pet.tipo = tipo;

            // Enviar al servidor
            if (enviar_peticion(&pet, &resp) != 0) {
                ui_show_error("Error de comunicacion con el servidor");
                return;
            }

            if (resp.codigo == RESP_OK) {
                ui_show_success(resp.mensaje);
                return;
            } else {
                strcpy(mensaje_error, resp.mensaje);
                error_encontrado = 1;
            }
        }
        
        // Si hubo error, mostrar y preguntar si reintentar
        if(error_encontrado) {
            ui_clear_screen();
            ui_print_colored(LINES/2 - 3, (COLS-strlen(mensaje_error))/2, mensaje_error, 3);

            if(error_encontrado == 2) {
                mvprintw(LINES/2 - 1, (COLS-90)/2,
                        "Debe tener: 1 mayuscula, 1 minuscula, 1 numero, 2 simbolos(*/_-+.), min 8 caracteres");
            }
            else if(error_encontrado == 3) {
                mvprintw(LINES/2 - 1, (COLS-40)/2, "Solo numeros, minimo 8 digitos");
            }

            // Mensaje de reintento
            mvprintw(LINES/2 + 2, (COLS-strlen("¿Intentar otra vez?"))/2, "¿Intentar otra vez?");

            // Dibujar botones de selección Sí/No
            int centro_x = (COLS - 40) / 2; // Centrado para dos botones de 12 caracteres con espacio
            int boton_y = LINES/2 + 4;
            int boton_ancho = 12;
            int boton_alto = 3;

            int seleccion = 0; // 0 para Sí, 1 para No
            int salir_seleccion = 0;

            while(!salir_seleccion) {
                // Dibujar botones usando la función centralizada
                dibujar_botones_si_no(centro_x, boton_y, seleccion, boton_ancho, boton_alto);

                ui_print_footer("Use flechas para navegar y Enter para seleccionar");
                refresh();

                int tecla = getch();
                if(tecla == KEY_LEFT || tecla == KEY_RIGHT) {
                    seleccion = 1 - seleccion; // Alternar entre 0 y 1
                } else if(tecla == 10) { // Enter
                    if(seleccion == 0) { // Sí
                        salir_seleccion = 1;
                        // No es necesario hacer nada más aquí, simplemente continuar el bucle
                    } else { // No
                        intentar_nuevamente = 0;
                        salir_seleccion = 1;
                    }
                } else if(tecla == 27) { // ESC
                    intentar_nuevamente = 0;
                    salir_seleccion = 1;
                }
            }
        }
    }
}

// Recuperar contraseña
void recuperar_password() {
    char user[MAX_CHAIN_SIZE];
    char token[100];
    char new_pass[MAX_CHAIN_SIZE];

    ui_clear_screen();
    ui_print_title("Recuperar Contrasena - Paso 1");

    attron(COLOR_PAIR(10) | A_BOLD);
    mvprintw(6, 5, "Ingrese su usuario:");
    attroff(COLOR_PAIR(10) | A_BOLD);
    ui_print_footer("Presione ESC para cancelar");
    refresh();

    ui_read_input(6, 25, user, MAX_CHAIN_SIZE, 0);
    if(strlen(user) == 0) return;

    // Verificar si existe
    Peticion pet_ver;
    Respuesta resp_ver;
    memset(&pet_ver, 0, sizeof(Peticion));
    memset(&resp_ver, 0, sizeof(Respuesta));

    pet_ver.operacion = OP_VERIFICAR_USUARIO;
    strncpy(pet_ver.user, user, MAX_CHAIN_SIZE-1);

    if (enviar_peticion(&pet_ver, &resp_ver) == 0 && resp_ver.codigo != RESP_USUARIO_EXISTE) {
        ui_show_error("Usuario no encontrado");
        return;
    }

    ui_clear_screen();
    ui_print_title("Recuperar Contrasena - Paso 1");

    attron(COLOR_PAIR(9) | A_BOLD);
    mvprintw(6, 5, "> Usuario encontrado!");
    mvprintw(8, 5, "> Se le ha enviado un token de recuperacion.");
    mvprintw(10, 5, "> Por favor revise su metodo de notificacion.");
    attroff(COLOR_PAIR(9) | A_BOLD);

    // Generar token de recuperación
    if(generar_token_recuperacion(user) != 0) {
        ui_show_error("Error al generar token de recuperacion");
        return;
    }

    ui_print_footer("Presione cualquier tecla para continuar...");
    refresh();
    ui_wait_key();

    // Segundo paso: Ingresar token y nueva contraseña
    ui_clear_screen();
    ui_print_title("Recuperar Contrasena - Paso 2");

    attron(COLOR_PAIR(10) | A_BOLD);
    mvprintw(6, 5, "Ingrese el token recibido:");
    mvprintw(8, 5, "Nueva contrasena:");
    attroff(COLOR_PAIR(10) | A_BOLD);

    // Mostrar requisitos
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

    ui_print_footer("Presione ESC para cancelar");
    refresh();

    ui_read_input(6, 28, token, sizeof(token), 0);
    if(strlen(token) == 0) return;

    ui_read_input(8, 24, new_pass, MAX_CHAIN_SIZE, 1);
    if(strlen(new_pass) == 0) return;

    if(validar_password(new_pass) != 0) {
        ui_clear_screen();
        ui_print_colored(LINES/2, (COLS-30)/2, "La contrasena es invalida", 3);
        mvprintw(LINES/2 + 1, (COLS-90)/2,
                "Debe tener: 1 mayuscula, 1 minuscula, 1 numero, 2 simbolos(*/_-+.), min 8 caracteres");
        ui_print_footer("Presione cualquier tecla para continuar...");
        refresh();
        ui_wait_key();
        return;
    }

    // Cambiar contraseña usando token
    int resultado = cambiar_password_por_token(user, token, new_pass);
    if(resultado != 0) {
        switch(resultado) {
            case -1:
                ui_show_error("Token no encontrado");
                break;
            case -2:
                ui_show_error("Token ya fue usado");
                break;
            case -3:
                ui_show_error("Token expirado");
                break;
            case -4:
                ui_show_error("Nueva contraseña invalida");
                break;
            case -5:
                ui_show_error("Error al cambiar la contraseña");
                break;
            case -6:
                ui_show_error("Error al marcar token como usado");
                break;
            default:
                ui_show_error("Error desconocido al recuperar contraseña");
                break;
        }
        return;
    }

    ui_show_success("Contraseña cambiada exitosamente!");
}

int main() {
    // Inicializar UI
    ui_init();
    
    // Conectar al servidor (con espera)
    if (conectar_servidor() != 0) {
        ui_show_error("No se pudo conectar al servidor.\nAsegurese de que este ejecutandose.");
        ui_cleanup();
        return -1;
    }
    
    ui_show_success("Conectado al servidor exitosamente!");
    sleep(1);
    
    // Menú principal
    char *opciones[] = {
        "Iniciar Sesion",
        "Crear Usuario",
        "Recuperar Contrasena",
        "Salir"
    };
    
    int salir = 0;
    
    while(!salir) {
        int opcion = ui_show_menu("Sistema de Comandas", opciones, 4);
        
        switch(opcion) {
            case 0: { // Iniciar Sesión
                USUARIO usuario_logueado;
                memset(&usuario_logueado, 0, sizeof(USUARIO));
                
                if (iniciar_sesion(&usuario_logueado) == 0) {
                    // Lanzar interfaz según tipo
                    if (usuario_logueado.tipo == 2) {
                        // Administrador
                        interfaz_admin_ejecutar(&usuario_logueado);
                    } else if (usuario_logueado.tipo == 1) {
                        // Cocina
                        interfaz_cocina_ejecutar(&usuario_logueado);
                    } else {
                        // Mesero
                        interfaz_mesero_ejecutar(&usuario_logueado);
                    }
                }
                break;
            }
            
            case 1: // Crear Usuario
                crear_usuario();
                break;
                
            case 2: // Recuperar Contraseña
                recuperar_password();
                break;
                
            case 3: // Salir
                salir = 1;
                break;
        }
    }
    
    // Desconectar y limpiar
    desconectar_servidor();
    ui_cleanup();
    
    return 0;
}
