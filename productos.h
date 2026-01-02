#ifndef PRODUCTOS_H
#define PRODUCTOS_H

#define MAX_PRODUCTOS 100
#define MAX_NOMBRE_PRODUCTO 100
#define MAX_DESCRIPCION 200
#define MAX_INGREDIENTES_PRODUCTO 20

typedef struct {
    int id;
    int id_ingrediente;
    int cantidad_necesaria;
} IngredienteProducto;

typedef struct {
    int id;
    char nombre[MAX_NOMBRE_PRODUCTO];
    char descripcion[MAX_DESCRIPCION];
    float precio;
    IngredienteProducto ingredientes[MAX_INGREDIENTES_PRODUCTO];
    int num_ingredientes;
} PRODUCTO;

// Gestión de productos
void productos_init();
void productos_cargar();
void productos_guardar();
int productos_get_total();
PRODUCTO* productos_get_by_index(int index);
PRODUCTO* productos_get_by_id(int id);
int productos_agregar(const char* nombre, const char* descripcion, float precio);
int producto_agregar_ingrediente(int id_producto, int id_ingrediente, int cantidad);

// Verificación de disponibilidad
int producto_verificar_disponibilidad(int id_producto);
int producto_verificar_disponibilidad_cantidad(int id_producto, int cantidad_pedido);

// Eliminar producto
int producto_eliminar(int id_producto);

#endif