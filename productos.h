#ifndef PRODUCTOS_H
#define PRODUCTOS_H

#define MAX_PRODUCTOS 100
#define MAX_NOMBRE_PRODUCTO 100
#define MAX_DESCRIPCION 200

typedef struct {
    int id;
    char nombre[MAX_NOMBRE_PRODUCTO];
    char descripcion[MAX_DESCRIPCION];
    float precio;
} PRODUCTO;

// Gestión de productos
void productos_init();
void productos_cargar();
void productos_guardar();
int productos_get_total();
PRODUCTO* productos_get_by_index(int index);
PRODUCTO* productos_get_by_id(int id);
int productos_agregar(const char* nombre, const char* descripcion, float precio);

#endif