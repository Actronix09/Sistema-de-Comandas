#include "inventario.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static Ingrediente ingredientes[MAX_INGREDIENTES];
static int total_ingredientes = 0;

// Estructura para relacionar productos con ingredientes
typedef struct {
    int id_producto;
    int id_ingrediente;
    int cantidad_necesaria;
} RelacionProductoIngrediente;

static RelacionProductoIngrediente relaciones[500];
static int total_relaciones = 0;

void inventario_init() {
    total_ingredientes = 0;
    total_relaciones = 0;
}

void inventario_cargar() {
    FILE *archivo = fopen("inventario", "r");
    if(archivo == NULL) {
        printf("Archivo de inventario no encontrado, creando uno nuevo...\n");
        return;
    }

    total_ingredientes = 0;
    char linea[512];

    // Leer ingredientes
    while(fgets(linea, sizeof(linea), archivo) && total_ingredientes < MAX_INGREDIENTES) {
        if(linea[0] == '#') continue;
        if(strlen(linea) < 5) continue;
        
        linea[strcspn(linea, "\n")] = 0;

        char *token = strtok(linea, "|");
        if(token) ingredientes[total_ingredientes].id = atoi(token);

        token = strtok(NULL, "|");
        if(token) strncpy(ingredientes[total_ingredientes].nombre, token, MAX_NOMBRE_INGREDIENTE-1);

        token = strtok(NULL, "|");
        if(token) ingredientes[total_ingredientes].cantidad = atoi(token);

        total_ingredientes++;
    }

    fclose(archivo);
    printf("Ingredientes cargados: %d\n", total_ingredientes);
    
    // Cargar también las relaciones producto-ingrediente
    archivo = fopen("producto_ingrediente", "r");
    if(archivo != NULL) {
        total_relaciones = 0;
        char linea_rel[512];
        
        while(fgets(linea_rel, sizeof(linea_rel), archivo) && total_relaciones < 500) {
            if(linea_rel[0] == '#') continue;
            if(strlen(linea_rel) < 5) continue;
            
            linea_rel[strcspn(linea_rel, "\n")] = 0;

            char *token = strtok(linea_rel, "|");
            if(token) relaciones[total_relaciones].id_producto = atoi(token);

            token = strtok(NULL, "|");
            if(token) relaciones[total_relaciones].id_ingrediente = atoi(token);

            token = strtok(NULL, "|");
            if(token) relaciones[total_relaciones].cantidad_necesaria = atoi(token);

            total_relaciones++;
        }
        
        fclose(archivo);
    }
}

void inventario_guardar() {
    // Guardar ingredientes
    FILE *archivo = fopen("inventario", "w");
    if(archivo == NULL) return;

    for(int i = 0; i < total_ingredientes; i++) {
        fprintf(archivo, "%d|%s|%d\n",
                ingredientes[i].id,
                ingredientes[i].nombre,
                ingredientes[i].cantidad);
    }

    fclose(archivo);
    
    // Guardar relaciones producto-ingrediente
    archivo = fopen("producto_ingrediente", "w");
    if(archivo != NULL) {
        for(int i = 0; i < total_relaciones; i++) {
            fprintf(archivo, "%d|%d|%d\n",
                    relaciones[i].id_producto,
                    relaciones[i].id_ingrediente,
                    relaciones[i].cantidad_necesaria);
        }
        fclose(archivo);
    }
}

int inventario_get_total() {
    return total_ingredientes;
}

Ingrediente* inventario_get_by_index(int index) {
    if(index >= 0 && index < total_ingredientes)
        return &ingredientes[index];
    return NULL;
}

Ingrediente* inventario_get_by_id(int id) {
    for(int i = 0; i < total_ingredientes; i++) {
        if(ingredientes[i].id == id)
            return &ingredientes[i];
    }
    return NULL;
}

int inventario_agregar(const char* nombre, int cantidad) {
    if(total_ingredientes >= MAX_INGREDIENTES)
        return -1;

    // Encontrar el siguiente ID disponible
    int next_id = 1;
    int id_encontrado = 0;
    while(!id_encontrado && next_id <= MAX_INGREDIENTES) {
        int existe = 0;
        for(int i = 0; i < total_ingredientes; i++) {
            if(ingredientes[i].id == next_id) {
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

    ingredientes[total_ingredientes].id = next_id;
    strncpy(ingredientes[total_ingredientes].nombre, nombre, MAX_NOMBRE_INGREDIENTE-1);
    ingredientes[total_ingredientes].cantidad = cantidad;

    total_ingredientes++;
    inventario_guardar();
    return 0;
}

int inventario_actualizar_stock(int id_ingrediente, int cantidad_a_restar) {
    Ingrediente* ing = inventario_get_by_id(id_ingrediente);
    if(ing == NULL) return -1;
    
    if(ing->cantidad < cantidad_a_restar) {
        return -2; // No hay suficiente stock
    }
    
    ing->cantidad -= cantidad_a_restar;
    inventario_guardar();
    return 0;
}

void producto_vincular_ingrediente(int id_producto, int id_ingrediente, int cantidad_necesaria) {
    if(total_relaciones >= 500) return;
    
    // Verificar si ya existe una relación
    for(int i = 0; i < total_relaciones; i++) {
        if(relaciones[i].id_producto == id_producto && 
           relaciones[i].id_ingrediente == id_ingrediente) {
            relaciones[i].cantidad_necesaria = cantidad_necesaria;
            inventario_guardar();
            return;
        }
    }
    
    relaciones[total_relaciones].id_producto = id_producto;
    relaciones[total_relaciones].id_ingrediente = id_ingrediente;
    relaciones[total_relaciones].cantidad_necesaria = cantidad_necesaria;
    total_relaciones++;
    inventario_guardar();
}

// Agregar ingrediente al inventario
int inventario_agregar_ingrediente(const char* nombre, int cantidad) {
    if(total_ingredientes >= MAX_INGREDIENTES)
        return -1;

    // Encontrar el siguiente ID disponible
    int next_id = 1;
    int id_encontrado = 0;
    while(!id_encontrado && next_id <= MAX_INGREDIENTES) {
        int existe = 0;
        for(int i = 0; i < total_ingredientes; i++) {
            if(ingredientes[i].id == next_id) {
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

    ingredientes[total_ingredientes].id = next_id;
    strncpy(ingredientes[total_ingredientes].nombre, nombre, MAX_NOMBRE_INGREDIENTE-1);
    ingredientes[total_ingredientes].cantidad = cantidad;

    total_ingredientes++;
    inventario_guardar();
    return 0;
}

// Modificar cantidad de ingrediente
int inventario_modificar_cantidad(int id_ingrediente, int nueva_cantidad) {
    for(int i = 0; i < total_ingredientes; i++) {
        if(ingredientes[i].id == id_ingrediente) {
            ingredientes[i].cantidad = nueva_cantidad;
            inventario_guardar();
            return 0;
        }
    }
    return -1; // Ingrediente no encontrado
}

// Eliminar ingrediente del inventario
int inventario_eliminar_ingrediente(int id_ingrediente) {
    int index = -1;
    // Encontrar el índice del ingrediente a eliminar
    for(int i = 0; i < total_ingredientes; i++) {
        if(ingredientes[i].id == id_ingrediente) {
            index = i;
            break;
        }
    }

    if(index == -1) {
        return -1; // Ingrediente no encontrado
    }

    // Eliminar todas las relaciones que usan este ingrediente
    for(int i = 0; i < total_relaciones; i++) {
        if(relaciones[i].id_ingrediente == id_ingrediente) {
            // Desplazar todas las relaciones después del eliminado hacia atrás
            for(int j = i; j < total_relaciones - 1; j++) {
                relaciones[j] = relaciones[j + 1];
            }
            total_relaciones--;
            i--; // Revisar la misma posición otra vez por si hay más duplicados
        }
    }

    // Desplazar todos los ingredientes después del eliminado hacia atrás
    for(int i = index; i < total_ingredientes - 1; i++) {
        ingredientes[i] = ingredientes[i + 1];
    }

    // Decrementar el total de ingredientes
    total_ingredientes--;

    // Guardar cambios
    inventario_guardar();

    return 0;
}

// Eliminar relaciones de un producto específico
void inventario_eliminar_relaciones_producto(int id_producto) {
    // Eliminar todas las relaciones que usan este producto
    for(int i = 0; i < total_relaciones; i++) {
        if(relaciones[i].id_producto == id_producto) {
            // Desplazar todas las relaciones después del eliminado hacia atrás
            for(int j = i; j < total_relaciones - 1; j++) {
                relaciones[j] = relaciones[j + 1];
            }
            total_relaciones--;
            i--; // Revisar la misma posición otra vez por si hay más duplicados
        }
    }
}
