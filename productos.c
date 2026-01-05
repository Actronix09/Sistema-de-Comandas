#include "productos.h"
#include "inventario.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static PRODUCTO productos[MAX_PRODUCTOS];
static int total_productos = 0;

void productos_init() {
    total_productos = 0;
}

void productos_cargar() {
    FILE *archivo = fopen("productos", "r");
    if(archivo == NULL) {
        printf("Archivo de productos no encontrado, creando uno nuevo...\n");
        return;
    }

    total_productos = 0;
    char linea[512];

    while(fgets(linea, sizeof(linea), archivo) && total_productos < MAX_PRODUCTOS) {
        linea[strcspn(linea, "\n")] = 0;

        char *token = strtok(linea, "|");
        if(token) productos[total_productos].id = atoi(token);

        token = strtok(NULL, "|");
        if(token) strncpy(productos[total_productos].nombre, token, MAX_NOMBRE_PRODUCTO-1);

        token = strtok(NULL, "|");
        if(token) strncpy(productos[total_productos].descripcion, token, MAX_DESCRIPCION-1);

        token = strtok(NULL, "|");
        if(token) productos[total_productos].precio = atof(token);

        // Inicializar ingredientes
        productos[total_productos].num_ingredientes = 0;

        total_productos++;
    }

    fclose(archivo);
    printf("Productos cargados: %d\n", total_productos);

    // Cargar también las relaciones producto-ingrediente
    archivo = fopen("producto_ingrediente", "r");
    if(archivo != NULL) {
        char linea_rel[512];

        while(fgets(linea_rel, sizeof(linea_rel), archivo)) {
            if(linea_rel[0] == '#') continue;
            if(strlen(linea_rel) < 5) continue;

            linea_rel[strcspn(linea_rel, "\n")] = 0;

            char *token = strtok(linea_rel, "|");
            int id_producto = atoi(token);

            token = strtok(NULL, "|");
            int id_ingrediente = atoi(token);

            token = strtok(NULL, "|");
            int cantidad_necesaria = atoi(token);

            // Encontrar el producto y agregar el ingrediente
            for(int i = 0; i < total_productos; i++) {
                if(productos[i].id == id_producto) {
                    // Verificar que no exceda el máximo de ingredientes
                    if(productos[i].num_ingredientes < MAX_INGREDIENTES_PRODUCTO) {
                        productos[i].ingredientes[productos[i].num_ingredientes].id = productos[i].num_ingredientes + 1;
                        productos[i].ingredientes[productos[i].num_ingredientes].id_ingrediente = id_ingrediente;
                        productos[i].ingredientes[productos[i].num_ingredientes].cantidad_necesaria = cantidad_necesaria;
                        productos[i].num_ingredientes++;
                    }
                    break;
                }
            }
        }

        fclose(archivo);
    }
}

void productos_guardar() {
    FILE *archivo = fopen("productos", "w");
    if(archivo == NULL) return;
    
    for(int i = 0; i < total_productos; i++) {
        fprintf(archivo, "%d|%s|%s|%.2f\n",
                productos[i].id,
                productos[i].nombre,
                productos[i].descripcion,
                productos[i].precio);
    }
    
    fclose(archivo);
}

int productos_get_total() {
    return total_productos;
}

PRODUCTO* productos_get_by_index(int index) {
    if(index >= 0 && index < total_productos)
        return &productos[index];
    return NULL;
}

PRODUCTO* productos_get_by_id(int id) {
    for(int i = 0; i < total_productos; i++) {
        if(productos[i].id == id)
            return &productos[i];
    }
    return NULL;
}

int productos_agregar(const char* nombre, const char* descripcion, float precio) {
    if(total_productos >= MAX_PRODUCTOS)
        return -1;

    // Encontrar el siguiente ID disponible
    int next_id = 1;
    int id_encontrado = 0;
    while(!id_encontrado && next_id <= MAX_PRODUCTOS) {
        int existe = 0;
        for(int i = 0; i < total_productos; i++) {
            if(productos[i].id == next_id) {
                existe = 1;
                break;
            }
        }
        if(!existe) {
            id_encontrado = 1;
        } else {
            next_id++;
        }
    }

    if(!id_encontrado) {
        return -1; // No se pudo encontrar un ID disponible
    }

    productos[total_productos].id = next_id;
    strncpy(productos[total_productos].nombre, nombre, MAX_NOMBRE_PRODUCTO-1);
    strncpy(productos[total_productos].descripcion, descripcion, MAX_DESCRIPCION-1);
    productos[total_productos].precio = precio;
    productos[total_productos].num_ingredientes = 0;
    total_productos++;
    productos_guardar();
    return 0;
}

int producto_agregar_ingrediente(int id_producto, int id_ingrediente, int cantidad) {
    PRODUCTO* prod = productos_get_by_id(id_producto);
    if(prod == NULL || prod->num_ingredientes >= MAX_INGREDIENTES_PRODUCTO) {
        return -1;
    }

    prod->ingredientes[prod->num_ingredientes].id = prod->num_ingredientes + 1;
    prod->ingredientes[prod->num_ingredientes].id_ingrediente = id_ingrediente;
    prod->ingredientes[prod->num_ingredientes].cantidad_necesaria = cantidad;
    prod->num_ingredientes++;

    // Vincular el sistema de inventario
    producto_vincular_ingrediente(id_producto, id_ingrediente, cantidad);

    return 0;
}

int producto_verificar_disponibilidad_cantidad(int id_producto, int cantidad_pedido) {
    PRODUCTO* prod = productos_get_by_id(id_producto);
    if(prod == NULL) return 0;

    // Verificar que haya suficiente cantidad de cada ingredientes
    for(int i = 0; i < prod->num_ingredientes; i++) {
        Ingrediente* ing = inventario_get_by_id(prod->ingredientes[i].id_ingrediente);
        int cantidad_necesaria = prod->ingredientes[i].cantidad_necesaria * cantidad_pedido;
        if(ing == NULL || ing->cantidad < cantidad_necesaria) {
            return 0; // No hay suficiente stock
        }
    }

    return 1; // Producto disponible
}

int producto_verificar_disponibilidad(int id_producto) {
    return producto_verificar_disponibilidad_cantidad(id_producto, 1);
}

// Eliminar producto del sistema
int producto_eliminar(int id_producto) {
    int index = -1;
    // Encontrar el índice del producto a eliminar
    for(int i = 0; i < total_productos; i++) {
        if(productos[i].id == id_producto) {
            index = i;
            break;
        }
    }

    if(index == -1) {
        return -1; // Producto no encontrado
    }

    // Eliminar todas las relaciones que usan este producto
    inventario_eliminar_relaciones_producto(id_producto);

    // Desplazar todos los productos después del eliminado hacia atrás
    for(int i = index; i < total_productos - 1; i++) {
        productos[i] = productos[i + 1];
    }

    // Decrementar el total de productos
    total_productos--;

    // Guardar cambios
    productos_guardar();

    return 0; // Éxito
}

