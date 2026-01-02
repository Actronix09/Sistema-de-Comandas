#include "pedidos.h"
#include "productos.h"
#include "inventario.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

static PEDIDO pedidos[MAX_PEDIDOS];
static int total_pedidos = 0;
static int siguiente_id = 1;

void pedidos_init() {
    total_pedidos = 0;
    siguiente_id = 1;
}

void pedidos_cargar() {
    FILE *archivo = fopen("pedidos", "r");
    if(archivo == NULL) {
        printf("Archivo de pedidos no encontrado, creando uno nuevo...\n");
        return;
    }
    
    total_pedidos = 0;
    char linea[1024];
    
    while(fgets(linea, sizeof(linea), archivo) && total_pedidos < MAX_PEDIDOS) {
        linea[strcspn(linea, "\n")] = 0;
        
        char *token = strtok(linea, "|");
        if(token) pedidos[total_pedidos].id = atoi(token);
        
        token = strtok(NULL, "|");
        if(token) strncpy(pedidos[total_pedidos].mesa, token, MAX_MESA-1);
        
        token = strtok(NULL, "|");
        if(token) strncpy(pedidos[total_pedidos].mesero, token, 99);
        
        token = strtok(NULL, "|");
        if(token) pedidos[total_pedidos].num_items = atoi(token);
        
        // Leer items
        for(int i = 0; i < pedidos[total_pedidos].num_items && i < MAX_ITEMS_PEDIDO; i++) {
            token = strtok(NULL, "|");
            if(token) pedidos[total_pedidos].items[i].producto_id = atoi(token);
            
            token = strtok(NULL, "|");
            if(token) strncpy(pedidos[total_pedidos].items[i].nombre_producto, token, 99);
            
            token = strtok(NULL, "|");
            if(token) pedidos[total_pedidos].items[i].cantidad = atoi(token);
        }
        
        token = strtok(NULL, "|");
        if(token) pedidos[total_pedidos].estado = atoi(token);
        
        token = strtok(NULL, "|");
        if(token) strncpy(pedidos[total_pedidos].fecha, token, 19);
        
        if(pedidos[total_pedidos].id >= siguiente_id)
            siguiente_id = pedidos[total_pedidos].id + 1;
        
        total_pedidos++;
    }
    
    fclose(archivo);
    printf("Pedidos cargados: %d\n", total_pedidos);
}

void pedidos_guardar() {
    FILE *archivo = fopen("pedidos", "w");
    if(archivo == NULL) return;
    
    for(int i = 0; i < total_pedidos; i++) {
        fprintf(archivo, "%d|%s|%s|%d",
                pedidos[i].id,
                pedidos[i].mesa,
                pedidos[i].mesero,
                pedidos[i].num_items);
        
        // Guardar items
        for(int j = 0; j < pedidos[i].num_items; j++) {
            fprintf(archivo, "|%d|%s|%d",
                    pedidos[i].items[j].producto_id,
                    pedidos[i].items[j].nombre_producto,
                    pedidos[i].items[j].cantidad);
        }
        
        fprintf(archivo, "|%d|%s\n",
                pedidos[i].estado,
                pedidos[i].fecha);
    }
    
    fclose(archivo);
}

int pedidos_get_total() {
    return total_pedidos;
}

PEDIDO* pedidos_get_by_index(int index) {
    if(index >= 0 && index < total_pedidos)
        return &pedidos[index];
    return NULL;
}

PEDIDO* pedidos_get_by_id(int id) {
    for(int i = 0; i < total_pedidos; i++) {
        if(pedidos[i].id == id)
            return &pedidos[i];
    }
    return NULL;
}

int pedidos_crear(const char* mesa, const char* mesero, ItemPedido* items, int num_items) {
    if(total_pedidos >= MAX_PEDIDOS || num_items > MAX_ITEMS_PEDIDO)
        return -1;

    // Verificar disponibilidad de ingredientes para cada producto
    for(int i = 0; i < num_items; i++) {
        if(!producto_verificar_disponibilidad_cantidad(items[i].producto_id, items[i].cantidad)) {
            return -2; // No hay ingredientes suficientes
        }
    }

    pedidos[total_pedidos].id = siguiente_id++;
    strncpy(pedidos[total_pedidos].mesa, mesa, MAX_MESA-1);
    strncpy(pedidos[total_pedidos].mesero, mesero, 99);
    pedidos[total_pedidos].num_items = num_items;

    for(int i = 0; i < num_items; i++) {
        pedidos[total_pedidos].items[i] = items[i];
    }

    pedidos[total_pedidos].estado = ESTADO_NO_EMPEZADO;

    // Obtener fecha actual
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    strftime(pedidos[total_pedidos].fecha, 20, "%d-%m-%Y %H:%M:%S", tm_info);

    int id_creado = pedidos[total_pedidos].id;
    total_pedidos++;
    pedidos_guardar();

    return id_creado;
}

int pedidos_cambiar_estado(int id, EstadoPedido nuevo_estado) {
    for(int i = 0; i < total_pedidos; i++) {
        if(pedidos[i].id == id) {
            EstadoPedido estado_anterior = pedidos[i].estado;
            pedidos[i].estado = nuevo_estado;

            // Si el estado cambió a ESTADO_LISTO, reducir el stock de ingredientes
            if(nuevo_estado == ESTADO_LISTO && estado_anterior != ESTADO_LISTO) {
                for(int j = 0; j < pedidos[i].num_items; j++) {
                    PRODUCTO* prod = productos_get_by_id(pedidos[i].items[j].producto_id);
                    if(prod != NULL) {
                        for(int k = 0; k < prod->num_ingredientes; k++) {
                            int cantidad_a_restar = prod->ingredientes[k].cantidad_necesaria * pedidos[i].items[j].cantidad;
                            inventario_actualizar_stock(prod->ingredientes[k].id_ingrediente, cantidad_a_restar);
                        }
                    }
                }
            }

            pedidos_guardar();
            return 0;
        }
    }
    return -1;
}

int pedidos_contar_por_estado(EstadoPedido estado) {
    int count = 0;
    for(int i = 0; i < total_pedidos; i++) {
        if(pedidos[i].estado == estado)
            count++;
    }
    return count;
}

// Función para eliminar pedido
int pedido_eliminar(int id_pedido) {
    int index = -1;
    // Encontrar el índice del pedido a eliminar
    for(int i = 0; i < total_pedidos; i++) {
        if(pedidos[i].id == id_pedido) {
            index = i;
            break;
        }
    }

    if(index == -1) {
        return -1; // Pedido no encontrado
    }

    // Desplazar todos los pedidos después del eliminado hacia atrás
    for(int i = index; i < total_pedidos - 1; i++) {
        pedidos[i] = pedidos[i + 1];
    }

    // Decrementar el total de pedidos
    total_pedidos--;

    // Guardar cambios
    pedidos_guardar();

    return 0; // Éxito
}
