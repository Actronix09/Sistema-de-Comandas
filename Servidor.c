#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include "usuario.h"
#include "productos.h"
#include "pedidos.h"
#include "protocolo.h"
#include "logger.h"
#include "ticket.h"

#define PERMISOS 0644
#define MAX_CLIENTES 10

// Estructura de datos compartidos
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

// Estructura para argumentos del hilo
typedef struct {
    DatosCompartidos *datos;
    int semid;
    int cliente_index;
    int cliente_num;
} ArgumentosHilo;

// Variables globales para limpieza
static int shmid_global = -1;
static int semid_global = -1;
static int semid_estado_global = -1;
static pthread_t hilo_log_auth;
static pthread_t hilo_log_cocina;

// Funciones de semáforo
int Crea_semaforo(key_t llave, int valor_inicial) {
    int semid = semget(llave, 1, IPC_CREAT | PERMISOS);
    if (semid == -1) return -1;
    semctl(semid, 0, SETVAL, valor_inicial);
    return semid;
}

void down(int semid) {
    struct sembuf op_p[] = {{0, -1, 0}};
    semop(semid, op_p, 1);
}

void up(int semid) {
    struct sembuf op_v[] = {{0, +1, 0}};
    semop(semid, op_v, 1);
}

// Función para limpiar recursos al salir
void limpiar_recursos(int signal) {
    printf("\n\n========================================\n");
    printf("Cerrando servidor...\n");
    
    // Detener hilos de logging
    logger_activo = 0;
    printf("Esperando a que finalicen los hilos de logging...\n");
    pthread_join(hilo_log_auth, NULL);
    pthread_join(hilo_log_cocina, NULL);
    
    logger_cleanup();
    
    if (semid_estado_global != -1) {
        down(semid_estado_global);
        up(semid_estado_global);
        semctl(semid_estado_global, 0, IPC_RMID, 0);
        printf("Semaforo de estado eliminado\n");
    }
    
    if (shmid_global != -1) {
        shmctl(shmid_global, IPC_RMID, 0);
        printf("Memoria compartida eliminada\n");
    }
    
    if (semid_global != -1) {
        semctl(semid_global, 0, IPC_RMID, 0);
        printf("Semaforo principal eliminado\n");
    }
    
    printf("Servidor cerrado correctamente\n");
    printf("========================================\n");
    exit(0);
}

// Función para generar ticket automáticamente
void generar_ticket_automatico(PEDIDO* pedido, float total) {
    // Crear ticket desde pedido
    int ticket_id = ticket_crear_desde_pedido(pedido);
    if(ticket_id > 0) {
        TICKET* t = ticket_get_by_id(ticket_id);
        if(t) {
            t->total = total;  // Asignar el total recibido
            ticket_guardar();
        }
        printf("    > Ticket generado automáticamente: #%d para pedido #%d - Total: $%.2f\n", 
               ticket_id, pedido->id, total);
    }
}

// Función para procesar peticiones
void procesarPeticion(Peticion *pet, Respuesta *resp, int cliente_num) {
    switch(pet->operacion) {
        case OP_LOGIN: {
            int index = usuario_autenticar(pet->user, pet->pass);
            if(index >= 0) {
                USUARIO* u = usuario_get_by_index(index);
                resp->codigo = RESP_OK;
                resp->tipo_usuario = u->tipo;
                strncpy(resp->nombre, u->name, MAX_CHAIN_SIZE-1);
                snprintf(resp->mensaje, TAM_MENSAJE, "Bienvenido %s", u->name);
                printf("    > Login exitoso: %s (tipo: %s)\n", u->name, u->tipo == 1 ? "Cocina" : "Mesero");
                
                // LOG: Login exitoso
                char detalles[256];
                snprintf(detalles, sizeof(detalles), "Tipo: %s", u->tipo == 1 ? "Cocina" : "Mesero");
                log_evento_auth(LOG_LOGIN_EXITOSO, pet->user, detalles, cliente_num);
            } else {
                resp->codigo = RESP_CREDENCIALES_INVALIDAS;
                resp->tipo_usuario = -1;
                strcpy(resp->mensaje, "Usuario o contraseña incorrectos");
                printf("    X Login fallido para: %s\n", pet->user);
                
                // LOG: Login fallido
                log_evento_auth(LOG_LOGIN_FALLIDO, pet->user, "Credenciales incorrectas", cliente_num);
            }
            break;
        }
        
        case OP_CREAR_USUARIO: {
            if(usuario_existe(pet->user)) {
                resp->codigo = RESP_USUARIO_EXISTE;
                strcpy(resp->mensaje, "El usuario ya existe");
                printf("    X Usuario ya existe: %s\n", pet->user);
                
                // LOG: Usuario ya existe
                log_evento_auth(LOG_USUARIO_EXISTE, pet->user, "Intento de crear usuario duplicado", cliente_num);
                break;
            }
            
            if(validar_password(pet->pass) != 0) {
                resp->codigo = RESP_VALIDACION_FALLIDA;
                strcpy(resp->mensaje, "La contraseña no cumple los requisitos");
                printf("    X Contraseña inválida para: %s\n", pet->user);
                break;
            }
            
            if(validar_email(pet->mail) != 0) {
                resp->codigo = RESP_VALIDACION_FALLIDA;
                strcpy(resp->mensaje, "El email es inválido");
                printf("    X Email inválido: %s\n", pet->mail);
                break;
            }
            
            if(validar_telefono(pet->telf) != 0) {
                resp->codigo = RESP_VALIDACION_FALLIDA;
                strcpy(resp->mensaje, "El teléfono es inválido");
                printf("    X Teléfono inválido: %s\n", pet->telf);
                break;
            }
            
            int resultado = usuario_crear(pet->name, pet->user, pet->pass, pet->mail, pet->telf, pet->tipo);
            
            if(resultado == 0) {
                resp->codigo = RESP_OK;
                strcpy(resp->mensaje, "Usuario creado exitosamente");
                printf("    > Usuario creado: %s (tipo: %s)\n", pet->user, pet->tipo == 1 ? "Cocina" : "Mesero");
                
                // LOG: Usuario creado
                char detalles[256];
                snprintf(detalles, sizeof(detalles), "Nombre: %s, Tipo: %s, Email: %s", pet->name, pet->tipo == 1 ? "Cocina" : "Mesero", pet->mail);
                log_evento_auth(LOG_USUARIO_CREADO, pet->user, detalles, cliente_num);
            } else {
                resp->codigo = RESP_ERROR;
                strcpy(resp->mensaje, "Error al crear usuario");
                printf("    X Error al crear usuario: %s\n", pet->user);
            }
            break;
        }
        
        case OP_RECUPERAR_PASS: {
            if(!usuario_existe(pet->user)) {
                resp->codigo = RESP_USUARIO_NO_EXISTE;
                strcpy(resp->mensaje, "Usuario no encontrado");
                printf("    X Usuario no encontrado: %s\n", pet->user);
                
                // LOG: Recuperación fallida
                log_evento_auth(LOG_PASSWORD_FALLIDA, pet->user, "Usuario no encontrado", cliente_num);
                break;
            }
            
            if(validar_password(pet->pass) != 0) {
                resp->codigo = RESP_VALIDACION_FALLIDA;
                strcpy(resp->mensaje, "La nueva contraseña no cumple los requisitos");
                printf("    X Nueva contraseña inválida para: %s\n", pet->user);
                
                // LOG: Recuperación fallida
                log_evento_auth(LOG_PASSWORD_FALLIDA, pet->user, "Contraseña no cumple requisitos", cliente_num);
                break;
            }
            
            int resultado = usuario_recuperar_pass(pet->user, pet->pass);
            if(resultado == 0) {
                resp->codigo = RESP_OK;
                strcpy(resp->mensaje, "Contraseña actualizada exitosamente");
                printf("    > Contraseña actualizada: %s\n", pet->user);
                
                // LOG: Contraseña recuperada
                log_evento_auth(LOG_PASSWORD_RECUPERADA, pet->user, "Contraseña actualizada exitosamente", cliente_num);
            } else {
                resp->codigo = RESP_ERROR;
                strcpy(resp->mensaje, "Error al actualizar contraseña");
                printf("    X Error al actualizar contraseña: %s\n", pet->user);
            }
            break;
        }
        
        case OP_VERIFICAR_USUARIO: {
            if(usuario_existe(pet->user)) {
                resp->codigo = RESP_USUARIO_EXISTE;
                strcpy(resp->mensaje, "El usuario ya existe");
            } else {
                resp->codigo = RESP_OK;
                strcpy(resp->mensaje, "Usuario disponible");
            }
            break;
        }
        
        case OP_LISTAR_PRODUCTOS: {
            int total = productos_get_total();
            resp->num_productos = (total > MAX_PRODUCTOS_RESPUESTA) ? MAX_PRODUCTOS_RESPUESTA : total;
            
            for(int i = 0; i < resp->num_productos; i++) {
                PRODUCTO* p = productos_get_by_index(i);
                if(p) {
                    resp->productos[i].id = p->id;
                    strncpy(resp->productos[i].nombre, p->nombre, 99);
                    strncpy(resp->productos[i].descripcion, p->descripcion, 199);
                    resp->productos[i].precio = p->precio;
                }
            }
            
            resp->codigo = RESP_OK;
            snprintf(resp->mensaje, TAM_MENSAJE, "%d productos encontrados", resp->num_productos);
            printf("    > Lista de productos enviada: %d productos\n", resp->num_productos);
            break;
        }
        
        case OP_CREAR_PEDIDO: {
            int id_pedido = pedidos_crear(pet->mesa, pet->name, (ItemPedido*)pet->items, pet->num_items);
            
            if(id_pedido > 0) {
                // **NUEVO: Generar ticket automático**
                PEDIDO* nuevo_pedido = pedidos_get_by_id(id_pedido);

                if(nuevo_pedido) {
                generar_ticket_automatico(nuevo_pedido, pet->total_pedido);
}
                
                resp->codigo = RESP_OK;
                resp->pedido_id_creado = id_pedido;
                snprintf(resp->mensaje, TAM_MENSAJE, "Pedido #%d creado exitosamente", id_pedido);
                printf("    > Pedido creado: #%d - Mesa: %s - Items: %d\n", id_pedido, pet->mesa, pet->num_items);

                // LOG: Pedido creado
                char detalles[256];
                snprintf(detalles, sizeof(detalles), "Creado por mesero");
                log_evento_cocina(LOG_PEDIDO_CREADO, id_pedido, pet->mesa, pet->name, pet->num_items, detalles);
            } else {
                resp->codigo = RESP_ERROR;
                strcpy(resp->mensaje, "Error al crear pedido");
                printf("    X Error al crear pedido\n");
                
                // LOG: Error en pedido
                log_evento_cocina(LOG_PEDIDO_ERROR, 0, pet->mesa, pet->name, pet->num_items, "Error al crear pedido");
            }
            break;
        }
        
        case OP_LISTAR_PEDIDOS: {
            int total = pedidos_get_total();
            int count = 0;
            
            for(int i = 0; i < total && count < 50; i++) {
                PEDIDO* p = pedidos_get_by_index(i);
                if(p && p->estado != ESTADO_LISTO) {
                    resp->pedidos[count].id = p->id;
                    strncpy(resp->pedidos[count].mesa, p->mesa, 49);
                    strncpy(resp->pedidos[count].mesero, p->mesero, 99);
                    resp->pedidos[count].num_items = p->num_items;
                    
                    for(int j = 0; j < p->num_items && j < MAX_ITEMS_PEDIDO; j++) {
                        resp->pedidos[count].items[j].producto_id = p->items[j].producto_id;
                        strncpy(resp->pedidos[count].items[j].nombre_producto, p->items[j].nombre_producto, 99);
                        resp->pedidos[count].items[j].cantidad = p->items[j].cantidad;
                    }
                    
                    resp->pedidos[count].estado = p->estado;
                    strncpy(resp->pedidos[count].fecha, p->fecha, 19);
                    count++;
                }
            }
            
            resp->num_pedidos = count;
            resp->codigo = RESP_OK;
            snprintf(resp->mensaje, TAM_MENSAJE, "%d pedidos activos", count);
            printf("    > Lista de pedidos enviada: %d pedidos\n", count);
            break;
        }
        
        case OP_CAMBIAR_ESTADO_PEDIDO: {
            int resultado = pedidos_cambiar_estado(pet->pedido_id, pet->nuevo_estado);
            
            if(resultado == 0) {
                resp->codigo = RESP_OK;
                const char* estados[] = {"NO EMPEZADO", "EN PROGRESO", "LISTO"};
                snprintf(resp->mensaje, TAM_MENSAJE, "Pedido #%d marcado como %s", 
                        pet->pedido_id, estados[pet->nuevo_estado]);
                printf("    > Estado actualizado: Pedido #%d -> %s\n", pet->pedido_id, estados[pet->nuevo_estado]);
                
                // LOG: Cambio de estado
                PEDIDO* p = pedidos_get_by_id(pet->pedido_id);
                TipoEventoCocina tipo_log;
                char detalles[256];
                
                if(pet->nuevo_estado == ESTADO_EN_PROGRESO) {
                    tipo_log = LOG_PEDIDO_EN_PROGRESO;
                    snprintf(detalles, sizeof(detalles), "Estado cambiado a EN PROGRESO");
                } else if(pet->nuevo_estado == ESTADO_LISTO) {
                    tipo_log = LOG_PEDIDO_COMPLETADO;
                    snprintf(detalles, sizeof(detalles), "Estado cambiado a COMPLETADO");
                } else {
                    tipo_log = LOG_PEDIDO_CREADO;
                    snprintf(detalles, sizeof(detalles), "Estado cambiado");
                }
                
                if(p) {
                    log_evento_cocina(tipo_log, pet->pedido_id, p->mesa, p->mesero, p->num_items, detalles);
                }
            } else {
                resp->codigo = RESP_ERROR;
                strcpy(resp->mensaje, "Error al cambiar estado del pedido");
                printf("    X Error al cambiar estado del pedido #%d\n", pet->pedido_id);
                
                // LOG: Error
                log_evento_cocina(LOG_PEDIDO_ERROR, pet->pedido_id, "", "", 0, "Error al cambiar estado");
            }
            break;
        }

        // Casos para ticket
        case OP_LISTAR_TICKETS: {
            int total = ticket_get_total();
            int count = 0;
            
            // Solo tickets pendientes de pago
            for(int i = 0; i < total && count < 50; i++) {
                TICKET* t = ticket_get_by_index(i);
                if(t && t->estado == TICKET_PENDIENTE_PAGO) {
                    resp->tickets[count].id = t->id;
                    resp->tickets[count].pedido_id = t->pedido_id;
                    strncpy(resp->tickets[count].mesa, t->mesa, 49);
                    strncpy(resp->tickets[count].mesero, t->mesero, 99);
                    resp->tickets[count].num_items = t->num_items;
                    
                    for(int j = 0; j < t->num_items && j < MAX_ITEMS_PEDIDO; j++) {
                        resp->tickets[count].items[j].producto_id = t->items[j].producto_id;
                        strncpy(resp->tickets[count].items[j].nombre_producto, 
                            t->items[j].nombre_producto, 99);
                        resp->tickets[count].items[j].cantidad = t->items[j].cantidad;
                    }
                    
                    resp->tickets[count].total = t->total;
                    resp->tickets[count].estado = t->estado;
                    strncpy(resp->tickets[count].fecha, t->fecha, 19);
                    count++;
                }
            }
            
            resp->num_tickets = count;
            resp->codigo = RESP_OK;
            snprintf(resp->mensaje, TAM_MENSAJE, "%d tickets pendientes", count);
            printf("    > Lista de tickets enviada: %d tickets\n", count);
            break;
        }

        case OP_MARCAR_TICKET_PAGADO: {
            int resultado = ticket_marcar_pagado(pet->pedido_id);
            
            if(resultado == 0) {
                resp->codigo = RESP_OK;
                resp->ticket_id_pagado = pet->pedido_id;
                strcpy(resp->mensaje, "Ticket marcado como pagado");
                printf("    > Ticket marcado como pagado: #%d\n", pet->pedido_id);
                
                // También marcar el pedido como pagado
                pedidos_cambiar_estado(pet->pedido_id, ESTADO_PAGADO);
            } else {
                resp->codigo = RESP_ERROR;
                strcpy(resp->mensaje, "Error al marcar ticket como pagado");
                printf("    X Error al marcar ticket como pagado #%d\n", pet->pedido_id);
            }
            break;
        }
        
        default:
            resp->codigo = RESP_ERROR;
            strcpy(resp->mensaje, "Operación no reconocida");
            printf("    X Operación desconocida: %d\n", pet->operacion);
            break;
    }
}

// Hilo para atender cliente
void *AtenderCliente(void *argumentos) {
    ArgumentosHilo *args = (ArgumentosHilo*)argumentos;
    int mi_index = args->cliente_index;
    
    printf("\n========== HILO CREADO ==========\n");
    printf("ID del hilo: %ld\n", pthread_self());
    printf("Atendiendo al cliente #%d (índice %d)\n", args->cliente_num, mi_index);
    
    while (1) {
        down(args->semid);
        
        if (args->datos->clientes[mi_index].cliente_conectado == 0) {
            up(args->semid);
            break;
        }
        
        if (args->datos->clientes[mi_index].peticion_lista == 1) {
            Peticion pet = args->datos->clientes[mi_index].peticion;
            args->datos->clientes[mi_index].peticion_lista = 0;
            up(args->semid);
            
            printf("\n========== PETICIÓN RECIBIDA ==========\n");
            printf("Cliente #%d\n", args->cliente_num);
            printf("Operación: ");
            switch(pet.operacion) {
                case OP_LOGIN: printf("LOGIN\n"); break;
                case OP_CREAR_USUARIO: printf("CREAR_USUARIO\n"); break;
                case OP_RECUPERAR_PASS: printf("RECUPERAR_PASS\n"); break;
                case OP_VERIFICAR_USUARIO: printf("VERIFICAR_USUARIO\n"); break;
                case OP_LISTAR_PRODUCTOS: printf("LISTAR_PRODUCTOS\n"); break;
                case OP_CREAR_PEDIDO: printf("CREAR_PEDIDO\n"); break;
                case OP_LISTAR_PEDIDOS: printf("LISTAR_PEDIDOS\n"); break;
                case OP_CAMBIAR_ESTADO_PEDIDO: printf("CAMBIAR_ESTADO_PEDIDO\n"); break;
                case OP_LISTAR_TICKETS: printf("LISTAR_TICKETS\n"); break;          // NUEVO
                case OP_MARCAR_TICKET_PAGADO: printf("MARCAR_TICKET_PAGADO\n"); break; // NUEVO
                default: printf("DESCONOCIDA\n"); break;
            }
            if(pet.operacion == OP_LOGIN || pet.operacion == OP_CREAR_USUARIO) {
                printf("Usuario: %s\n", pet.user);
            }
            
            Respuesta resp;
            memset(&resp, 0, sizeof(Respuesta));
            procesarPeticion(&pet, &resp, args->cliente_num);
            
            down(args->semid);
            args->datos->clientes[mi_index].respuesta = resp;
            args->datos->clientes[mi_index].respuesta_lista = 1;
            up(args->semid);
            
            printf("Respuesta enviada: %s\n", resp.mensaje);
            printf("======================================\n");
            
            if(pet.operacion == OP_LOGOUT) {
                break;
            }
        } else {
            up(args->semid);
        }
        
        usleep(50000);
    }
    
    down(args->semid);
    args->datos->clientes[mi_index].cliente_conectado = 0;
    args->datos->clientes[mi_index].hilo_asignado = 0;
    args->datos->num_clientes_activos--;
    printf("\nCliente #%d finalizado. Clientes activos: %d\n", args->cliente_num, args->datos->num_clientes_activos);
    
    // LOG: Cliente desconectado
    char detalles[256];
    snprintf(detalles, sizeof(detalles), "Desconexión normal");
    log_evento_auth(LOG_CLIENTE_DESCONECTADO, "", detalles, args->cliente_num);
    
    up(args->semid);
    
    pthread_exit(NULL);
}

int main() {
    int semid, semid_estado;
    DatosCompartidos *datos;
    key_t llave_mem, llave_sem, llave_sem_estado;
    pthread_t hilos_clientes[MAX_CLIENTES];
    ArgumentosHilo args[MAX_CLIENTES];
    int cliente_contador = 0;
    
    // Configurar manejador de señales para limpieza
    signal(SIGINT, limpiar_recursos);
    signal(SIGTERM, limpiar_recursos);
    
    printf("\n");
    printf("+---------------------------------------+\n");
    printf("|  SERVIDOR DE USUARIOS INICIADO        |\n");
    printf("+---------------------------------------+\n");
    printf("\nPID del Servidor: %d\n", getpid());
    
    // Inicializar sistemas
    usuario_init();
    usuario_cargar();
    printf("Sistema de usuarios inicializado\n");
    printf("Usuarios registrados: %d\n", usuario_get_total());
    
    productos_init();
    productos_cargar();
    printf("Productos cargados: %d\n", productos_get_total());
    
    pedidos_init();
    pedidos_cargar();
    printf("Pedidos cargados: %d\n", pedidos_get_total());

    //Inicializacion de tickets
    ticket_init();
    ticket_cargar();    
    printf("Tickets cargados: %d\n", ticket_get_total());
    
    // Inicializar sistema de logging
    logger_init();
    printf("Sistema de logging inicializado\n");
    
    // Crear hilos de logging
    if (pthread_create(&hilo_log_auth, NULL, hilo_logger_auth, NULL) != 0) {
        fprintf(stderr, "Error al crear hilo de logging de autenticación\n");
        exit(-1);
    }
    pthread_detach(hilo_log_auth);
    
    if (pthread_create(&hilo_log_cocina, NULL, hilo_logger_cocina, NULL) != 0) {
        fprintf(stderr, "Error al crear hilo de logging de cocina\n");
        exit(-1);
    }
    pthread_detach(hilo_log_cocina);
    
    // Crear archivos de llave
    FILE *f1 = fopen("servidor_usuarios_mem", "a");
    FILE *f2 = fopen("servidor_usuarios_sem", "a");
    FILE *f3 = fopen("servidor_estado_sem", "a");
    if (f1) fclose(f1);
    if (f2) fclose(f2);
    if (f3) fclose(f3);
    
    llave_mem = ftok("servidor_usuarios_mem", 'U');
    llave_sem = ftok("servidor_usuarios_sem", 'V');
    llave_sem_estado = ftok("servidor_estado_sem", 'E');
    
    if (llave_mem == -1 || llave_sem == -1 || llave_sem_estado == -1) {
        perror("Error al crear llaves");
        exit(-1);
    }
    
    // Crear semáforo de estado
    semid_estado = Crea_semaforo(llave_sem_estado, 0);
    if (semid_estado == -1) {
        perror("Error al crear semáforo de estado");
        exit(-1);
    }
    semid_estado_global = semid_estado;
    printf("Semáforo de estado creado (ID: %d)\n", semid_estado);
    
    shmid_global = shmget(llave_mem, sizeof(DatosCompartidos), IPC_CREAT | PERMISOS);
    if (shmid_global == -1) {
        perror("Error al crear memoria compartida");
        semctl(semid_estado, 0, IPC_RMID, 0);
        exit(-1);
    }
    
    datos = (DatosCompartidos *)shmat(shmid_global, 0, 0);
    if (datos == (void *)-1) {
        perror("Error al adjuntar memoria compartida");
        shmctl(shmid_global, IPC_RMID, 0);
        semctl(semid_estado, 0, IPC_RMID, 0);
        exit(-1);
    }
    
    semid = Crea_semaforo(llave_sem, 1);
    if (semid == -1) {
        perror("Error al crear semáforo");
        shmdt(datos);
        shmctl(shmid_global, IPC_RMID, 0);
        semctl(semid_estado, 0, IPC_RMID, 0);
        exit(-1);
    }
    semid_global = semid;
    
    down(semid);
    datos->servidor_activo = 1;
    datos->num_clientes_activos = 0;
    for (int i = 0; i < MAX_CLIENTES; i++) {
        datos->clientes[i].cliente_conectado = 0;
        datos->clientes[i].peticion_lista = 0;
        datos->clientes[i].respuesta_lista = 0;
        datos->clientes[i].cliente_id = 0;
        datos->clientes[i].hilo_asignado = 0;
    }
    up(semid);
    
    printf("Memoria compartida creada (ID: %d)\n", shmid_global);
    printf("Semáforo creado (ID: %d)\n", semid);
    
    // SEÑALIZAR QUE EL SERVIDOR ESTÁ LISTO
    up(semid_estado);
    printf("\n*** SERVIDOR LISTO PARA ACEPTAR CONEXIONES ***\n");
    printf("\nEsperando clientes...\n");
    printf("(Presione Ctrl+C para detener)\n\n");
    
    while (1) {
        int slot_disponible = -1;
        
        down(semid);
        for (int i = 0; i < MAX_CLIENTES; i++) {
            if (datos->clientes[i].cliente_conectado == 1 && 
                datos->clientes[i].hilo_asignado == 0) {
                slot_disponible = i;
                datos->clientes[i].hilo_asignado = 1;
                break;
            }
        }
        up(semid);
        
        if (slot_disponible != -1) {
            cliente_contador++;
            
            down(semid);
            datos->num_clientes_activos++;
            int num_activos = datos->num_clientes_activos;
            up(semid);
            
            printf("\n========== NUEVO CLIENTE ==========\n");
            printf("Cliente #%d conectado (slot %d)\n", cliente_contador, slot_disponible);
            printf("Clientes activos: %d/%d\n", num_activos, MAX_CLIENTES);
            
            // LOG: Cliente conectado
            char detalles[256];
            snprintf(detalles, sizeof(detalles), "Slot: %d, PID: %d", slot_disponible, datos->clientes[slot_disponible].cliente_id);
            log_evento_auth(LOG_CLIENTE_CONECTADO, "", detalles, cliente_contador);
            
            args[slot_disponible].datos = datos;
            args[slot_disponible].semid = semid;
            args[slot_disponible].cliente_index = slot_disponible;
            args[slot_disponible].cliente_num = cliente_contador;
            
            if (pthread_create(&hilos_clientes[slot_disponible], NULL, AtenderCliente, (void*)&args[slot_disponible]) != 0) {
                fprintf(stderr, "Error al crear hilo\n");
                down(semid);
                datos->clientes[slot_disponible].cliente_conectado = 0;
                datos->clientes[slot_disponible].hilo_asignado = 0;
                datos->num_clientes_activos--;
                up(semid);
            } else {
                pthread_detach(hilos_clientes[slot_disponible]);
            }
        }
        
        usleep(100000);
    }
    
    limpiar_recursos(0);
    
    return 0;
}