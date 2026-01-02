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
#include "inventario.h"
#include "protocolo.h"
#include "logger.h"

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
                printf("    > Login exitoso: %s (tipo: %s)\n", u->name, u->tipo == 2 ? "Administrador" : (u->tipo == 1 ? "Cocina" : "Mesero"));

                // LOG: Login exitoso
                char detalles[256];
                snprintf(detalles, sizeof(detalles), "Tipo: %s", u->tipo == 2 ? "Administrador" : (u->tipo == 1 ? "Cocina" : "Mesero"));
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
                printf("    > Usuario creado: %s (tipo: %s)\n", pet->user, pet->tipo == 2 ? "Administrador" : (pet->tipo == 1 ? "Cocina" : "Mesero"));

                // LOG: Usuario creado
                char detalles[256];
                snprintf(detalles, sizeof(detalles), "Nombre: %s, Tipo: %s, Email: %s", pet->name, pet->tipo == 2 ? "Administrador" : (pet->tipo == 1 ? "Cocina" : "Mesero"), pet->mail);
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
                resp->codigo = RESP_OK;
                resp->pedido_id_creado = id_pedido;
                snprintf(resp->mensaje, TAM_MENSAJE, "Pedido #%d creado exitosamente", id_pedido);
                printf("    > Pedido creado: #%d - Mesa: %s - Items: %d\n", id_pedido, pet->mesa, pet->num_items);

                // LOG: Pedido creado
                char detalles[256];
                snprintf(detalles, sizeof(detalles), "Creado por mesero");
                log_evento_cocina(LOG_PEDIDO_CREADO, id_pedido, pet->mesa, pet->name, pet->num_items, detalles);
            } else if(id_pedido == -2) {
                resp->codigo = RESP_ERROR;
                strcpy(resp->mensaje, "No hay suficientes ingredientes para crear el pedido");
                printf("    X No hay suficientes ingredientes para el pedido\n");

                // LOG: Error en pedido por falta de ingredientes
                log_evento_cocina(LOG_PEDIDO_ERROR, 0, pet->mesa, pet->name, pet->num_items, "No hay suficientes ingredientes");
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

        case OP_LISTAR_USUARIOS: {
            int total = usuario_get_total();
            resp->num_usuarios = total > 100 ? 100 : total;

            for(int i = 0; i < resp->num_usuarios; i++) {
                USUARIO* u = usuario_get_by_index(i);
                if(u) {
                    strncpy(resp->usuarios[i].name, u->name, MAX_CHAIN_SIZE-1);
                    strncpy(resp->usuarios[i].user, u->user, MAX_CHAIN_SIZE-1);
                    strncpy(resp->usuarios[i].pass, u->pass, 32);
                    strncpy(resp->usuarios[i].mail, u->mail, MAX_CHAIN_SIZE-1);
                    strncpy(resp->usuarios[i].telf, u->telf, MAX_CHAIN_SIZE-1);
                    resp->usuarios[i].tipo = u->tipo;
                }
            }

            resp->codigo = RESP_OK;
            snprintf(resp->mensaje, TAM_MENSAJE, "%d usuarios encontrados", resp->num_usuarios);
            printf("    > Lista de usuarios enviada: %d usuarios\n", resp->num_usuarios);
            break;
        }

        case OP_OBTENER_USUARIO: {
            USUARIO* usr = NULL;
            for(int i = 0; i < usuario_get_total(); i++) {
                USUARIO* temp = usuario_get_by_index(i);
                if(temp && strcmp(temp->user, pet->user) == 0) {
                    usr = temp;
                    break;
                }
            }

            if(usr) {
                strncpy(resp->usuario.name, usr->name, MAX_CHAIN_SIZE-1);
                strncpy(resp->usuario.user, usr->user, MAX_CHAIN_SIZE-1);
                strncpy(resp->usuario.pass, usr->pass, 32);
                strncpy(resp->usuario.mail, usr->mail, MAX_CHAIN_SIZE-1);
                strncpy(resp->usuario.telf, usr->telf, MAX_CHAIN_SIZE-1);
                resp->usuario.tipo = usr->tipo;
                resp->codigo = RESP_OK;
                snprintf(resp->mensaje, TAM_MENSAJE, "Usuario '%s' encontrado", usr->user);
                printf("    > Usuario '%s' obtenido\n", usr->user);
            } else {
                resp->codigo = RESP_ERROR;
                strcpy(resp->mensaje, "Usuario no encontrado");
                printf("    X Usuario no encontrado: %s\n", pet->user);
            }
            break;
        }

        case OP_MODIFICAR_USUARIO: {
            // Buscar el usuario
            USUARIO* usr = NULL;
            for(int i = 0; i < usuario_get_total(); i++) {
                USUARIO* temp = usuario_get_by_index(i);
                if(temp && strcmp(temp->user, pet->user) == 0) {
                    usr = temp;
                    break;
                }
            }

            if(usr) {
                // Modificar solo los campos proporcionados
                if(strlen(pet->name) > 0) {
                    strncpy(usr->name, pet->name, MAX_CHAIN_SIZE-1);
                }
                if(strlen(pet->mail) > 0) {
                    strncpy(usr->mail, pet->mail, MAX_CHAIN_SIZE-1);
                }
                if(strlen(pet->telf) > 0) {
                    strncpy(usr->telf, pet->telf, MAX_CHAIN_SIZE-1);
                }
                if(pet->tipo >= 0 && pet->tipo <= 2) {
                    usr->tipo = pet->tipo;
                }

                // Guardar cambios
                usuario_guardar();

                resp->codigo = RESP_OK;
                snprintf(resp->mensaje, TAM_MENSAJE, "Usuario '%s' modificado exitosamente", usr->user);
                printf("    > Usuario '%s' modificado\n", usr->user);
            } else {
                resp->codigo = RESP_ERROR;
                strcpy(resp->mensaje, "Usuario no encontrado");
                printf("    X Usuario no encontrado: %s\n", pet->user);
            }
            break;
        }

        case OP_ELIMINAR_USUARIO: {
            // Eliminar usuario
            int index_a_eliminar = -1;
            for(int i = 0; i < usuario_get_total(); i++) {
                USUARIO* temp = usuario_get_by_index(i);
                if(temp && strcmp(temp->user, pet->user) == 0) {
                    index_a_eliminar = i;
                    break;
                }
            }

            if(index_a_eliminar != -1) {
                for(int i = index_a_eliminar; i < usuario_get_total() - 1; i++) {
                    USUARIO* next = usuario_get_by_index(i + 1);
                    USUARIO* current = usuario_get_by_index(i);
                    *current = *next;
                }

                // Decrementar el total de usuarios
                extern int total_usuarios;
                total_usuarios--;

                // Guardar cambios
                usuario_guardar();

                resp->codigo = RESP_OK;
                snprintf(resp->mensaje, TAM_MENSAJE, "Usuario '%s' eliminado exitosamente", pet->user);
                printf("    > Usuario '%s' eliminado\n", pet->user);
            } else {
                resp->codigo = RESP_ERROR;
                strcpy(resp->mensaje, "Usuario no encontrado");
                printf("    X Usuario no encontrado: %s\n", pet->user);
            }
            break;
        }

        case OP_OBTENER_PRODUCTO: {
            PRODUCTO* prod = productos_get_by_id(pet->producto_id);
            if(prod) {
                resp->producto.id = prod->id;
                strncpy(resp->producto.nombre, prod->nombre, 99);
                strncpy(resp->producto.descripcion, prod->descripcion, 199);
                resp->producto.precio = prod->precio;
                resp->codigo = RESP_OK;
                snprintf(resp->mensaje, TAM_MENSAJE, "Producto #%d encontrado", prod->id);
                printf("    > Producto #%d obtenido\n", prod->id);
            } else {
                resp->codigo = RESP_ERROR;
                strcpy(resp->mensaje, "Producto no encontrado");
                printf("    X Producto no encontrado: %d\n", pet->producto_id);
            }
            break;
        }

        case OP_MODIFICAR_PRODUCTO: {
            PRODUCTO* prod = productos_get_by_id(pet->producto_id);
            if(prod) {
                // Modificar solo los campos proporcionados
                if(strlen(pet->name) > 0) {
                    strncpy(prod->nombre, pet->name, MAX_NOMBRE_PRODUCTO-1);
                }
                if(strlen(pet->descripcion) > 0) {
                    strncpy(prod->descripcion, pet->descripcion, MAX_DESCRIPCION-1);
                }
                if(pet->precio > 0) {
                    prod->precio = pet->precio;
                }

                // Si hay ingredientes especificados, actualizarlos
                if(pet->num_ingredientes > 0) {
                    prod->num_ingredientes = 0;
                    for(int i = 0; i < pet->num_ingredientes && i < MAX_INGREDIENTES_PRODUCTO; i++) {
                        prod->ingredientes[i].id = prod->num_ingredientes + 1;
                        prod->ingredientes[i].id_ingrediente = pet->ingredientes[i].id;
                        prod->ingredientes[i].cantidad_necesaria = pet->ingredientes[i].cantidad;
                        prod->num_ingredientes++;
                    }
                }

                // Guardar cambios
                productos_guardar();

                resp->codigo = RESP_OK;
                snprintf(resp->mensaje, TAM_MENSAJE, "Producto #%d modificado exitosamente", prod->id);
                printf("    > Producto #%d modificado\n", prod->id);
            } else {
                resp->codigo = RESP_ERROR;
                strcpy(resp->mensaje, "Producto no encontrado");
                printf("    X Producto no encontrado: %d\n", pet->producto_id);
            }
            break;
        }

        case OP_ELIMINAR_PRODUCTO: {
            if(producto_eliminar(pet->producto_id) == 0) {
                resp->codigo = RESP_OK;
                snprintf(resp->mensaje, TAM_MENSAJE, "Producto #%d eliminado exitosamente", pet->producto_id);
                printf("    > Producto #%d eliminado\n", pet->producto_id);
            } else {
                resp->codigo = RESP_ERROR;
                strcpy(resp->mensaje, "Producto no encontrado o error al eliminar");
                printf("    X Error al eliminar producto: %d\n", pet->producto_id);
            }
            break;
        }

        case OP_CREAR_PRODUCTO: {
            int resultado = productos_agregar(pet->name, pet->descripcion, pet->precio);
            if(resultado == 0) {
                // Obtener el producto recién creado para obtener su ID
                PRODUCTO* nuevo_prod = productos_get_by_index(productos_get_total() - 1);
                if(nuevo_prod) {
                    // Si hay ingredientes especificados, agregarlos
                    for(int i = 0; i < pet->num_ingredientes && i < MAX_INGREDIENTES_PRODUCTO; i++) {
                        producto_agregar_ingrediente(nuevo_prod->id, pet->ingredientes[i].id, pet->ingredientes[i].cantidad);
                    }
                }

                resp->codigo = RESP_OK;
                snprintf(resp->mensaje, TAM_MENSAJE, "Producto creado exitosamente");
                printf("    > Producto creado\n");
            } else {
                resp->codigo = RESP_ERROR;
                strcpy(resp->mensaje, "Error al crear producto");
                printf("    X Error al crear producto\n");
            }
            break;
        }

        case OP_LISTAR_INGREDIENTES: {
            int total = inventario_get_total();
            resp->num_ingredientes = total > 100 ? 100 : total;

            for(int i = 0; i < resp->num_ingredientes; i++) {
                Ingrediente* ing = inventario_get_by_index(i);
                if(ing) {
                    resp->ingredientes[i].id = ing->id;
                    strncpy(resp->ingredientes[i].nombre, ing->nombre, 99);
                    resp->ingredientes[i].cantidad = ing->cantidad;
                }
            }

            resp->codigo = RESP_OK;
            snprintf(resp->mensaje, TAM_MENSAJE, "%d ingredientes encontrados", resp->num_ingredientes);
            printf("    > Lista de ingredientes enviada: %d ingredientes\n", resp->num_ingredientes);
            break;
        }

        case OP_OBTENER_INGREDIENTE: {
            Ingrediente* ing = inventario_get_by_id(pet->ingrediente_id);
            if(ing) {
                resp->ingrediente.id = ing->id;
                strncpy(resp->ingrediente.nombre, ing->nombre, 99);
                resp->ingrediente.cantidad = ing->cantidad;
                resp->codigo = RESP_OK;
                snprintf(resp->mensaje, TAM_MENSAJE, "Ingrediente #%d encontrado", ing->id);
                printf("    > Ingrediente #%d obtenido\n", ing->id);
            } else {
                resp->codigo = RESP_ERROR;
                strcpy(resp->mensaje, "Ingrediente no encontrado");
                printf("    X Ingrediente no encontrado: %d\n", pet->ingrediente_id);
            }
            break;
        }

        case OP_CREAR_INGREDIENTE: {
            int resultado = inventario_agregar_ingrediente(pet->name, pet->cantidad);
            if(resultado == 0) {
                resp->codigo = RESP_OK;
                snprintf(resp->mensaje, TAM_MENSAJE, "Ingrediente creado exitosamente");
                printf("    > Ingrediente '%s' creado\n", pet->name);
            } else {
                resp->codigo = RESP_ERROR;
                strcpy(resp->mensaje, "Error al crear ingrediente");
                printf("    X Error al crear ingrediente\n");
            }
            break;
        }

        case OP_MODIFICAR_INGREDIENTE: {
            if(inventario_modificar_cantidad(pet->ingrediente_id, pet->cantidad) == 0) {
                resp->codigo = RESP_OK;
                snprintf(resp->mensaje, TAM_MENSAJE, "Ingrediente modificado exitosamente");
                printf("    > Ingrediente #%d modificado\n", pet->ingrediente_id);
            } else {
                resp->codigo = RESP_ERROR;
                strcpy(resp->mensaje, "Ingrediente no encontrado o error al modificar");
                printf("    X Error al modificar ingrediente: %d\n", pet->ingrediente_id);
            }
            break;
        }

        case OP_ELIMINAR_INGREDIENTE: {
            if(inventario_eliminar_ingrediente(pet->ingrediente_id) == 0) {
                resp->codigo = RESP_OK;
                snprintf(resp->mensaje, TAM_MENSAJE, "Ingrediente eliminado exitosamente");
                printf("    > Ingrediente #%d eliminado\n", pet->ingrediente_id);
            } else {
                resp->codigo = RESP_ERROR;
                strcpy(resp->mensaje, "Ingrediente no encontrado o error al eliminar");
                printf("    X Error al eliminar ingrediente: %d\n", pet->ingrediente_id);
            }
            break;
        }

        case OP_ELIMINAR_PEDIDO: {
            if(pedido_eliminar(pet->pedido_id) == 0) {
                resp->codigo = RESP_OK;
                snprintf(resp->mensaje, TAM_MENSAJE, "Pedido #%d eliminado exitosamente", pet->pedido_id);
                printf("    > Pedido #%d eliminado\n", pet->pedido_id);
            } else {
                resp->codigo = RESP_ERROR;
                strcpy(resp->mensaje, "Pedido no encontrado o error al eliminar");
                printf("    X Error al eliminar pedido: %d\n", pet->pedido_id);
            }
            break;
        }

        case OP_OBTENER_PEDIDO: {
            PEDIDO* ped = pedidos_get_by_id(pet->pedido_id);
            if(ped) {
                resp->pedido.id = ped->id;
                strncpy(resp->pedido.mesa, ped->mesa, 49);
                strncpy(resp->pedido.mesero, ped->mesero, 99);
                resp->pedido.num_items = ped->num_items;
                resp->pedido.estado = ped->estado;
                strncpy(resp->pedido.fecha, ped->fecha, 19);

                for(int j = 0; j < ped->num_items && j < MAX_ITEMS_PEDIDO; j++) {
                    resp->pedido.items[j].producto_id = ped->items[j].producto_id;
                    strncpy(resp->pedido.items[j].nombre_producto, ped->items[j].nombre_producto, 99);
                    resp->pedido.items[j].cantidad = ped->items[j].cantidad;
                }

                resp->codigo = RESP_OK;
                snprintf(resp->mensaje, TAM_MENSAJE, "Pedido #%d encontrado", ped->id);
                printf("    > Pedido #%d obtenido\n", ped->id);
            } else {
                resp->codigo = RESP_ERROR;
                strcpy(resp->mensaje, "Pedido no encontrado");
                printf("    X Pedido no encontrado: %d\n", pet->pedido_id);
            }
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

        case OP_VERIFICAR_DISPONIBILIDAD_PRODUCTO: {
            int disponible = producto_verificar_disponibilidad_cantidad(pet->producto_id, pet->cantidad);
            if(disponible) {
                resp->codigo = RESP_OK;
                snprintf(resp->mensaje, TAM_MENSAJE, "Producto #%d disponible para cantidad %d", pet->producto_id, pet->cantidad);
            } else {
                resp->codigo = RESP_ERROR; // Use error code to indicate unavailability
                snprintf(resp->mensaje, TAM_MENSAJE, "Producto #%d no disponible para cantidad %d", pet->producto_id, pet->cantidad);
            }
            break;
        }

        case OP_LISTAR_LOGS: {
            // Buscar archivos de log en el directorio
            resp->num_logs = 0;

            // Verificar si existen los archivos típicos de logs
            FILE* file;

            // Archivo de autenticación
            file = fopen("auth.log", "r");
            if(file) {
                fclose(file);
                strncpy(resp->logs[resp->num_logs], "auth.log", 99);
                resp->num_logs++;
            }

            // Archivo de cocina
            file = fopen("cocina.log", "r");
            if(file) {
                fclose(file);
                strncpy(resp->logs[resp->num_logs], "cocina.log", 99);
                resp->num_logs++;
            }

            // Archivo de otros logs si existen
            file = fopen("comandas.log", "r");
            if(file) {
                fclose(file);
                strncpy(resp->logs[resp->num_logs], "comandas.log", 99);
                resp->num_logs++;
            }

            if(resp->num_logs > 0) {
                resp->codigo = RESP_OK;
                snprintf(resp->mensaje, TAM_MENSAJE, "%d archivos de log encontrados", resp->num_logs);
            } else {
                resp->codigo = RESP_ERROR;
                strcpy(resp->mensaje, "No se encontraron archivos de log");
            }
            break;
        }

        case OP_VER_LOG: {
            if(strlen(pet->nombre_archivo) == 0) {
                resp->codigo = RESP_ERROR;
                strcpy(resp->mensaje, "Nombre de archivo no especificado");
                break;
            }

            // Construir el nombre de archivo seguro
            char filename[200];
            snprintf(filename, sizeof(filename), "%.90s", pet->nombre_archivo);

            // Verificar que el archivo es un log
            if(strstr(filename, ".log") == NULL) {
                resp->codigo = RESP_ERROR;
                strcpy(resp->mensaje, "Tipo de archivo no permitido");
                break;
            }

            // Evitar directorios superiores
            if(strstr(filename, "..") != NULL || filename[0] == '/') {
                resp->codigo = RESP_ERROR;
                strcpy(resp->mensaje, "Acceso al archivo no permitido");
                break;
            }

            FILE *file = fopen(filename, "r");
            if(file == NULL) {
                resp->codigo = RESP_ERROR;
                snprintf(resp->mensaje, TAM_MENSAJE, "No se pudo abrir el archivo: %s", filename);
                break;
            }

            char line[500];
            int total_chars = 0;
            int line_count = 0;
            resp->contenido_log[0] = '\0';

            while(fgets(line, sizeof(line), file) != NULL && line_count < 100) {
                if(total_chars + strlen(line) < sizeof(resp->contenido_log) - 1) {
                    strncat(resp->contenido_log, line, sizeof(resp->contenido_log) - total_chars - 1);
                    total_chars += strlen(line);
                }
                line_count++;
            }

            fclose(file);

            resp->codigo = RESP_OK;
            snprintf(resp->mensaje, TAM_MENSAJE, "Contenido del archivo %s leído exitosamente", filename);
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
    
    inventario_init();
    inventario_cargar();
    printf("Ingredientes cargados: %d\n", inventario_get_total());

    productos_init();
    productos_cargar();
    printf("Productos cargados: %d\n", productos_get_total());

    pedidos_init();
    pedidos_cargar();
    printf("Pedidos cargados: %d\n", pedidos_get_total());
    
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
    
    // Señalizar que el servirdor esta listo
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
