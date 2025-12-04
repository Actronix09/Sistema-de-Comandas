#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include "usuario.h"
#include "productos.h"
#include "pedidos.h"
#include "protocolo.h"

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

// Función para procesar peticiones
void procesarPeticion(Peticion *pet, Respuesta *resp) {
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
            } else {
                resp->codigo = RESP_CREDENCIALES_INVALIDAS;
                resp->tipo_usuario = -1;
                strcpy(resp->mensaje, "Usuario o contraseña incorrectos");
                printf("    X Login fallido para: %s\n", pet->user);
            }
            break;
        }
        
        case OP_CREAR_USUARIO: {
            if(usuario_existe(pet->user)) {
                resp->codigo = RESP_USUARIO_EXISTE;
                strcpy(resp->mensaje, "El usuario ya existe");
                printf("    X Usuario ya existe: %s\n", pet->user);
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
                break;
            }
            
            if(validar_password(pet->pass) != 0) {
                resp->codigo = RESP_VALIDACION_FALLIDA;
                strcpy(resp->mensaje, "La nueva contraseña no cumple los requisitos");
                printf("    X Nueva contraseña inválida para: %s\n", pet->user);
                break;
            }
            
            int resultado = usuario_recuperar_pass(pet->user, pet->pass);
            if(resultado == 0) {
                resp->codigo = RESP_OK;
                strcpy(resp->mensaje, "Contraseña actualizada exitosamente");
                printf("    > Contraseña actualizada: %s\n", pet->user);
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
                resp->codigo = RESP_OK;
                resp->pedido_id_creado = id_pedido;
                snprintf(resp->mensaje, TAM_MENSAJE, "Pedido #%d creado exitosamente", id_pedido);
                printf("    > Pedido creado: #%d - Mesa: %s - Items: %d\n", id_pedido, pet->mesa, pet->num_items);
            } else {
                resp->codigo = RESP_ERROR;
                strcpy(resp->mensaje, "Error al crear pedido");
                printf("    X Error al crear pedido\n");
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
            } else {
                resp->codigo = RESP_ERROR;
                strcpy(resp->mensaje, "Error al cambiar estado del pedido");
                printf("    X Error al cambiar estado del pedido #%d\n", pet->pedido_id);
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
                default: printf("DESCONOCIDA\n"); break;
            }
            if(pet.operacion == OP_LOGIN || pet.operacion == OP_CREAR_USUARIO) {
                printf("Usuario: %s\n", pet.user);
            }
            
            Respuesta resp;
            memset(&resp, 0, sizeof(Respuesta));
            procesarPeticion(&pet, &resp);
            
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
    up(args->semid);
    
    pthread_exit(NULL);
}

int main() {
    int shmid, semid;
    DatosCompartidos *datos;
    key_t llave_mem, llave_sem;
    pthread_t hilos_clientes[MAX_CLIENTES];
    ArgumentosHilo args[MAX_CLIENTES];
    int cliente_contador = 0;
    
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
    
    // Crear archivos de llave
    FILE *f1 = fopen("servidor_usuarios_mem", "a");
    FILE *f2 = fopen("servidor_usuarios_sem", "a");
    if (f1) fclose(f1);
    if (f2) fclose(f2);
    
    llave_mem = ftok("servidor_usuarios_mem", 'U');
    llave_sem = ftok("servidor_usuarios_sem", 'V');
    
    if (llave_mem == -1 || llave_sem == -1) {
        perror("Error al crear llaves");
        exit(-1);
    }
    
    shmid = shmget(llave_mem, sizeof(DatosCompartidos), IPC_CREAT | PERMISOS);
    if (shmid == -1) {
        perror("Error al crear memoria compartida");
        exit(-1);
    }
    
    datos = (DatosCompartidos *)shmat(shmid, 0, 0);
    if (datos == (void *)-1) {
        perror("Error al adjuntar memoria compartida");
        shmctl(shmid, IPC_RMID, 0);
        exit(-1);
    }
    
    semid = Crea_semaforo(llave_sem, 1);
    if (semid == -1) {
        perror("Error al crear semáforo");
        shmdt(datos);
        shmctl(shmid, IPC_RMID, 0);
        exit(-1);
    }
    
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
    
    printf("\nMemoria compartida creada (ID: %d)\n", shmid);
    printf("Semáforo creado (ID: %d)\n", semid);
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
    
    shmdt(datos);
    shmctl(shmid, IPC_RMID, 0);
    semctl(semid, 0, IPC_RMID, 0);
    
    return 0;
}