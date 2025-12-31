#include "productos.h"
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
        
        total_productos++;
    }
    
    fclose(archivo);
    printf("Productos cargados: %d\n", total_productos);
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
    
    productos[total_productos].id = total_productos + 1;
    strncpy(productos[total_productos].nombre, nombre, MAX_NOMBRE_PRODUCTO-1);
    strncpy(productos[total_productos].descripcion, descripcion, MAX_DESCRIPCION-1);
    productos[total_productos].precio = precio;
    
    total_productos++;
    productos_guardar();
    return 0;
}