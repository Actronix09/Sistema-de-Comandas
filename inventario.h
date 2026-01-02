#ifndef INVENTARIO_H
#define INVENTARIO_H

#define MAX_INGREDIENTES 100
#define MAX_NOMBRE_INGREDIENTE 100

typedef struct {
    int id;
    char nombre[MAX_NOMBRE_INGREDIENTE];
    int cantidad;  // cantidad disponible
} Ingrediente;

// Gestión de inventario
void inventario_init();
void inventario_cargar();
void inventario_guardar();
int inventario_get_total();
Ingrediente* inventario_get_by_index(int index);
Ingrediente* inventario_get_by_id(int id);
int inventario_agregar(const char* nombre, int cantidad);
int inventario_actualizar_stock(int id_ingrediente, int cantidad_a_restar);

// Compatibilidad con productos
void producto_vincular_ingrediente(int id_producto, int id_ingrediente, int cantidad_necesaria);
int producto_verificar_disponibilidad(int id_producto);

// Funciones adicionales para administración de ingredientes
int inventario_agregar_ingrediente(const char* nombre, int cantidad);
int inventario_modificar_cantidad(int id_ingrediente, int nueva_cantidad);
int inventario_eliminar_ingrediente(int id_ingrediente);
void inventario_eliminar_relaciones_producto(int id_producto);

#endif